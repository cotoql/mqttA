#include "MqttClient.h"

#include <stdio.h>
#include <string.h>

namespace com
{
namespace elemental
{
namespace biblioteca
{
namespace network
{


namespace
{

const unsigned long PROMPT_TIMEOUT_MS      = 3000;
const unsigned long SEND_TIMEOUT_MS        = 5000;
const unsigned long CONNACK_TIMEOUT_MS     = 5000;
const unsigned long SUBACK_TIMEOUT_MS      = 5000;
const unsigned long PING_TIMEOUT_MS        = 10000;
const unsigned long RECONNECT_INTERVAL_MS  = 5000;

// Cuanto esperar a que el ESP salga de "busy s..." antes de rendirse.
const unsigned int  BUSY_BUDGET_MS         = 12000;

// Tras N fallos de conexion seguidos se reinicia el modulo (AT+RST).
const uint8_t       RESET_AFTER_FAILS      = 6;

// Espera entre intentos: 5 s * (1 + fallos), con tope de 5 pasos (30 s).
const uint8_t       MAX_BACKOFF_STEPS      = 5;

// Escribe un string MQTT (2 bytes de largo + bytes). Devuelve lo escrito.
size_t putString(uint8_t* dst, const char* s)
{
    size_t n = strlen(s);

    dst[0] = (uint8_t)(n >> 8);
    dst[1] = (uint8_t)(n & 0xFF);

    memcpy(dst + 2, s, n);

    return n + 2;
}

}


MqttClient::MqttClient(
    WiFiManager& wifi,
    ESPAtDriver& esp
)
:
_wifi(wifi),
_esp(esp)
{
    for(uint8_t i = 0; i < MAX_SUBS; ++i)
        _subs[i] = nullptr;

    memset(_win, 0, sizeof(_win));
    memset(_topic, 0, sizeof(_topic));
}



// ============================================================
//  Configuracion
// ============================================================

void MqttClient::setServer(const char* host, uint16_t port)
{
    _host = host;
    _port = port;
}

void MqttClient::setCredentials(const char* user, const char* pass)
{
    _user = user;
    _pass = pass;
}

void MqttClient::setClientId(const char* clientId)
{
    _clientId = clientId;
}

void MqttClient::setKeepAlive(uint16_t seconds)
{
    _keepAlive = seconds;
}

void MqttClient::onMessage(MessageCallback callback)
{
    _callback = callback;
}

bool MqttClient::connected() const
{
    return _connected;
}



// ============================================================
//  API publica
// ============================================================

bool MqttClient::connect()
{
    if(_host == nullptr)
        return false;

    _connected   = false;
    _pingPending = false;

    if(!openTcp())
    {
        Serial.println(F("[MQTT] TCP fallido"));
        return false;
    }

    resetStream();

    size_t length = buildConnect();

    if(length == 0 || !sendPacket(_tx, length))
    {
        closeTcp();
        return false;
    }

    if(!waitFlag(_connAckReceived, CONNACK_TIMEOUT_MS))
    {
        Serial.println(F("[MQTT] sin CONNACK"));
        closeTcp();
        return false;
    }

    if(_connAckCode != 0)
    {
        // 4 = usuario/clave incorrectos, 5 = no autorizado
        Serial.print(F("[MQTT] CONNACK rechazado, rc="));
        Serial.println(_connAckCode);

        closeTcp();
        return false;
    }

    _connected = true;

    Serial.println(F("[MQTT] conectado"));

    for(uint8_t i = 0; i < _subCount; ++i)
    {
        if(!subscribeNow(_subs[i]))
        {
            Serial.println(F("[MQTT] SUBSCRIBE fallido"));
            closeTcp();
            return false;
        }
    }

    return true;
}



void MqttClient::disconnect()
{
    if(_connected)
    {
        static const uint8_t packet[2] = { 0xE0, 0x00 };

        sendPacket(packet, 2);
    }

    closeTcp();
}



bool MqttClient::subscribe(const char* topic)
{
    if(topic == nullptr || topic[0] == '\0')
        return false;

    bool known = false;

    for(uint8_t i = 0; i < _subCount; ++i)
    {
        if(strcmp(_subs[i], topic) == 0)
            known = true;
    }

    if(!known)
    {
        if(_subCount >= MAX_SUBS)
            return false;

        _subs[_subCount++] = topic;
    }

    if(_connected)
        return subscribeNow(topic);

    return true;
}



bool MqttClient::publish(const char* topic, const char* payload)
{
    if(!_connected || topic == nullptr)
        return false;

    size_t topicLen   = strlen(topic);
    size_t payloadLen = (payload != nullptr) ? strlen(payload) : 0;

    size_t remaining = 2 + topicLen + payloadLen;

    // remaining < 128 => el "remaining length" ocupa 1 solo byte
    if(topicLen == 0 || remaining > TX_MAX - 2)
    {
        Serial.println(F("[MQTT] publish: mensaje demasiado grande"));
        return false;
    }

    size_t i = 0;

    _tx[i++] = 0x30;                 // PUBLISH, QoS 0, sin retain
    _tx[i++] = (uint8_t)remaining;

    i += putString(_tx + i, topic);

    if(payloadLen > 0)
    {
        memcpy(_tx + i, payload, payloadLen);
        i += payloadLen;
    }

    if(!sendPacket(_tx, i))
    {
        Serial.println(F("[MQTT] publish fallido"));
        closeTcp();
        return false;
    }

    return true;
}



void MqttClient::update()
{
    pump();

    if(_closed)
    {
        _closed = false;

        if(_connected)
        {
            Serial.println(F("[MQTT] conexion cerrada por el servidor"));
            _connected   = false;
            _pingPending = false;
        }
    }


    //-------------------------------------------------
    // Conectado: keepalive
    //-------------------------------------------------

    if(_connected)
    {
        unsigned long now = millis();

        if(_pingPending)
        {
            if(now - _pingSentAt >= PING_TIMEOUT_MS)
            {
                Serial.println(F("[MQTT] sin PINGRESP, se reconecta"));
                closeTcp();
            }
        }
        else if(now - _lastTx >= (unsigned long)_keepAlive * 500UL)
        {
            static const uint8_t pingReq[2] = { 0xC0, 0x00 };

            // Se marca ANTES de enviar: el PINGRESP puede llegar pegado al
            // "SEND OK" y procesarse dentro del propio sendPacket().
            _pingPending = true;
            _pingSentAt  = millis();

            if(!sendPacket(pingReq, 2))
                closeTcp();
        }

        return;
    }


    //-------------------------------------------------
    // Sin conexion: reintento con espera entre intentos
    //-------------------------------------------------

    if((long)(millis() - _nextAttempt) < 0)
        return;

    Serial.println(F("[MQTT] conectando..."));

    if(connect())
    {
        _failCount = 0;
    }
    else
    {
        if(_failCount < 255)
            ++_failCount;

        // Solo se revisa el WiFi si falla el TCP. Si el ESP esta ocupado,
        // isConnected() ya no lo toma por "WiFi caido".
        if(!_wifi.isConnected())
        {
            Serial.println(F("[MQTT] WiFi caido, reconectando..."));
            _wifi.reconnect();
        }

        if(_failCount >= RESET_AFTER_FAILS)
        {
            Serial.println(F("[MQTT] demasiados fallos, reiniciando el ESP"));
            _wifi.resetModule();
            _failCount = 0;
        }
    }

    uint8_t steps = (_failCount < MAX_BACKOFF_STEPS) ? _failCount : MAX_BACKOFF_STEPS;

    _nextAttempt = millis() + RECONNECT_INTERVAL_MS * (1UL + steps);
}



// ============================================================
//  Capa TCP / AT
// ============================================================

void MqttClient::resetStream()
{
    _gotPrompt       = false;
    _sendOk          = false;
    _error           = false;
    _closed          = false;
    _busy            = false;
    _connAckReceived = false;
    _subAckReceived  = false;
    _connAckCode     = 0xFF;
    _subAckCode      = 0xFF;

    _ipdState     = IpdState::Scan;
    _ipdLen       = 0;
    _ipdRemaining = 0;

    memset(_win, 0, sizeof(_win));

    _rxState  = RxState::Header;
    _remLen   = 0;
    _remMult  = 1;
    _bodyIdx  = 0;
}



bool MqttClient::openTcp()
{
    // Cierra una sesion colgada (si no hay, responde ERROR y no pasa nada).
    // Si el ESP sigue enviando ("busy s...") se espera, no se insiste.
    _wifi.sendATWhenIdle("AT+CIPCLOSE", 1000, BUSY_BUDGET_MS);

    delay(200);

    // Conexion unica: el formato de datos entrantes es "+IPD,<len>:".
    _wifi.sendATWhenIdle("AT+CIPMUX=0", 1000, BUSY_BUDGET_MS);

    String cmd = "AT+CIPSTART=\"TCP\",\"";
    cmd += _host;
    cmd += "\",";
    cmd += String(_port);

    String resp = _wifi.sendATWhenIdle(cmd, 8000, BUSY_BUDGET_MS);

    // "CONNECT FAIL" contiene "CONNECT": primero se descarta el fallo.
    bool fail = resp.indexOf("FAIL") != -1 || resp.indexOf("ERROR") != -1;
    bool ok   = !fail && (resp.indexOf("CONNECT") != -1 || resp.indexOf("OK") != -1);

    _wifi.flushESP();

    return ok;
}



void MqttClient::closeTcp()
{
    _connected   = false;
    _pingPending = false;

    // Tras un SEND fallido el ESP puede seguir en "busy s..." varios segundos
    // hasta soltar SEND FAIL/CLOSED: se espera a que quede libre.
    _wifi.sendATWhenIdle("AT+CIPCLOSE", 2000, BUSY_BUDGET_MS);
    _wifi.flushESP();

    resetStream();
}



bool MqttClient::sendPacket(const uint8_t* data, size_t length)
{
    char cmd[24];

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned int)length);

    _gotPrompt = false;
    _sendOk    = false;
    _error     = false;
    _busy      = false;

    _esp.send(cmd);

    if(!waitFlag(_gotPrompt, PROMPT_TIMEOUT_MS))
    {
        if(_busy)
            Serial.println(F("[MQTT] ESP ocupado, no acepto CIPSEND"));
        else
            Serial.println(F("[MQTT] el ESP no dio el prompt '>'"));

        return false;
    }

    _esp.writeBytes(data, length);

    if(!waitFlag(_sendOk, SEND_TIMEOUT_MS))
    {
        Serial.println(F("[MQTT] sin SEND OK"));
        return false;
    }

    _lastTx = millis();

    return true;
}



bool MqttClient::waitFlag(bool& flag, unsigned long timeoutMs)
{
    unsigned long start = millis();

    while(millis() - start < timeoutMs)
    {
        pump();

        if(flag)
            return true;

        if(_error || _closed || _busy)
            return false;
    }

    return false;
}



void MqttClient::pump()
{
    int b;

    while((b = _esp.readByte()) >= 0)
        processByte((uint8_t)b);
}



// ============================================================
//  Demux del flujo del ESP
// ============================================================

bool MqttClient::endsWith(const char* token) const
{
    size_t n = strlen(token);

    return memcmp(_win + WIN - n, token, n) == 0;
}



void MqttClient::processByte(uint8_t b)
{
    //-------------------------------------------------
    // Datos de un "+IPD,<len>:" -> van al parser MQTT
    //-------------------------------------------------

    if(_ipdState == IpdState::Payload)
    {
        // Se descuenta ANTES de procesar: si un callback re-entrara,
        // el contador ya esta consistente.
        if(--_ipdRemaining == 0)
            _ipdState = IpdState::Scan;

        feedMqtt(b);

        return;
    }


    //-------------------------------------------------
    // Largo del "+IPD,"
    //-------------------------------------------------

    if(_ipdState == IpdState::Length)
    {
        if(b >= '0' && b <= '9')
        {
            _ipdLen = (uint16_t)(_ipdLen * 10 + (b - '0'));
            return;
        }

        if(b == ':' && _ipdLen > 0)
        {
            _ipdRemaining = _ipdLen;
            _ipdState     = IpdState::Payload;
            return;
        }

        // Formato inesperado: se vuelve a tratar este byte como texto.
        _ipdState = IpdState::Scan;
    }


    //-------------------------------------------------
    // Texto del ESP: se buscan tokens en una ventana deslizante
    //-------------------------------------------------

    memmove(_win, _win + 1, WIN - 1);

    _win[WIN - 1] = (char)b;

    if(b == '>')
    {
        _gotPrompt = true;
        return;
    }

    if(endsWith("+IPD,"))
    {
        _ipdState = IpdState::Length;
        _ipdLen   = 0;

        memset(_win, 0, sizeof(_win));

        return;
    }

    if(endsWith("SEND OK"))
    {
        _sendOk = true;
        return;
    }

    if(endsWith("SEND FAIL") || endsWith("ERROR"))
    {
        _error = true;
        return;
    }

    if(endsWith("CLOSED"))
    {
        _closed = true;
        return;
    }

    // "busy s..." = sigue enviando, "busy p..." = procesando otro comando.
    if(endsWith("busy s...") || endsWith("busy p..."))
    {
        _busy = true;
        return;
    }
}



// ============================================================
//  Ensamblado de paquetes MQTT
// ============================================================

void MqttClient::feedMqtt(uint8_t b)
{
    switch(_rxState)
    {

        case RxState::Header:

            _pktHeader = b;
            _remLen    = 0;
            _remMult   = 1;
            _rxState   = RxState::Length;

            break;


        case RxState::Length:

            _remLen  += (uint32_t)(b & 0x7F) * _remMult;
            _remMult *= 128;

            if(b & 0x80)
            {
                // Mas de 4 bytes de largo: flujo corrupto, se resincroniza.
                if(_remMult >= 268435456UL)
                    _rxState = RxState::Header;

                break;
            }

            _bodyIdx = 0;

            if(_remLen == 0)
            {
                _rxState = RxState::Header;

                handlePacket(0);
            }
            else
            {
                _rxState = RxState::Body;
            }

            break;


        case RxState::Body:

            if(_bodyIdx < RX_MAX)
                _rxBuf[_bodyIdx] = b;

            ++_bodyIdx;

            if(_bodyIdx >= _remLen)
            {
                _rxState = RxState::Header;

                if(_remLen <= RX_MAX)
                    handlePacket((uint16_t)_remLen);
                else
                    Serial.println(F("[MQTT] paquete descartado (muy grande)"));
            }

            break;

    }
}



void MqttClient::handlePacket(uint16_t length)
{
    switch(_pktHeader >> 4)
    {

        case 2:     // CONNACK

            if(length >= 2)
            {
                _connAckCode     = _rxBuf[1];
                _connAckReceived = true;
            }

            break;


        case 9:     // SUBACK

            if(length >= 3)
            {
                _subAckCode     = _rxBuf[2];
                _subAckReceived = true;
            }

            break;


        case 13:    // PINGRESP

            _pingPending = false;

            break;


        case 3:     // PUBLISH

            handlePublish(length);

            break;


        default:

            break;

    }
}



void MqttClient::handlePublish(uint16_t length)
{
    if(length < 2)
        return;

    uint16_t topicLen = ((uint16_t)_rxBuf[0] << 8) | _rxBuf[1];

    uint16_t index = 2 + topicLen;

    // Nos suscribimos en QoS 0, pero por si acaso se salta el packet id.
    if(((_pktHeader >> 1) & 0x03) > 0)
        index += 2;

    if(index > length || topicLen >= TOPIC_MAX)
    {
        Serial.println(F("[MQTT] PUBLISH invalido, descartado"));
        return;
    }

    memcpy(_topic, _rxBuf + 2, topicLen);

    _topic[topicLen] = '\0';

    if(_callback != nullptr)
        _callback(_topic, _rxBuf + index, length - index);
}



// ============================================================
//  Construccion de paquetes
// ============================================================

size_t MqttClient::buildConnect()
{
    bool hasUser = (_user != nullptr && _user[0] != '\0');

    // En MQTT 3.1.1 la clave solo se puede enviar junto con el usuario.
    bool hasPass = hasUser && (_pass != nullptr && _pass[0] != '\0');

    size_t remaining = 10 + 2 + strlen(_clientId);

    if(hasUser)
        remaining += 2 + strlen(_user);

    if(hasPass)
        remaining += 2 + strlen(_pass);

    if(remaining > TX_MAX - 2)
    {
        Serial.println(F("[MQTT] CONNECT demasiado grande"));
        return 0;
    }

    size_t i = 0;

    _tx[i++] = 0x10;                    // CONNECT
    _tx[i++] = (uint8_t)remaining;

    _tx[i++] = 0x00;                    // nombre de protocolo "MQTT"
    _tx[i++] = 0x04;
    _tx[i++] = 'M';
    _tx[i++] = 'Q';
    _tx[i++] = 'T';
    _tx[i++] = 'T';

    _tx[i++] = 0x04;                    // nivel 3.1.1

    uint8_t flags = 0x02;               // clean session

    if(hasUser)
        flags |= 0x80;

    if(hasPass)
        flags |= 0x40;

    _tx[i++] = flags;

    _tx[i++] = (uint8_t)(_keepAlive >> 8);
    _tx[i++] = (uint8_t)(_keepAlive & 0xFF);

    i += putString(_tx + i, _clientId);

    if(hasUser)
        i += putString(_tx + i, _user);

    if(hasPass)
        i += putString(_tx + i, _pass);

    return i;
}



bool MqttClient::subscribeNow(const char* topic)
{
    size_t topicLen = strlen(topic);

    size_t remaining = 2 + 2 + topicLen + 1;

    if(remaining > TX_MAX - 2)
        return false;

    if(++_packetId == 0)
        _packetId = 1;

    size_t i = 0;

    _tx[i++] = 0x82;                    // SUBSCRIBE
    _tx[i++] = (uint8_t)remaining;

    _tx[i++] = (uint8_t)(_packetId >> 8);
    _tx[i++] = (uint8_t)(_packetId & 0xFF);

    i += putString(_tx + i, topic);

    _tx[i++] = 0x00;                    // QoS 0

    _subAckReceived = false;
    _subAckCode     = 0xFF;

    if(!sendPacket(_tx, i))
        return false;

    if(!waitFlag(_subAckReceived, SUBACK_TIMEOUT_MS))
        return false;

    // 0x80 = el broker rechazo la suscripcion (ACL, topic invalido...)
    return _subAckCode != 0x80;
}


}
}
}
}
