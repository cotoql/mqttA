#pragma once

// ============================================================
//  com::elemental::biblioteca::network::MqttClient
//
//  Cliente MQTT 3.1.1 (QoS 0) sobre TCP con comandos AT clasicos
//  del ESP-01 (AT+CIPSTART / AT+CIPSEND). No requiere firmware con
//  comandos AT+MQTT*.
//
//  Notas de diseno:
//   - Los paquetes MQTT son binarios (contienen 0x00 y 0x0A), asi que
//     NO pasan por el pipeline de lineas (MessageParser/CommandQueue).
//     Se escriben con ESPAtDriver::writeBytes() y se leen con
//     ESPAtDriver::readByte().
//   - Una vez abierta la sesion TCP, este objeto es el UNICO que lee
//     del ESP: no llames esp.update() ni wifi.sendAT() mientras haya
//     conexion MQTT activa.
//   - El callback onMessage() se ejecuta dentro de update(): que sea
//     corto y NO llames publish()/subscribe() desde adentro.
//   - Los const char* que se pasan (host, usuario, topics...) no se
//     copian: deben vivir mientras viva el cliente (los #define de
//     secrets.h son literales, sirven).
// ============================================================

#include <Arduino.h>

#include "WiFiManager.h"
#include "core/ESPAtDriver.h"

namespace com
{
namespace elemental
{
namespace biblioteca
{
namespace network
{


class MqttClient
{

public:

    typedef void (*MessageCallback)(
        const char* topic,
        const uint8_t* payload,
        size_t length
    );

    static const size_t TX_MAX    = 128;   // paquete saliente maximo
    static const size_t RX_MAX    = 128;   // paquete entrante maximo (el resto se descarta)
    static const size_t TOPIC_MAX = 64;    // largo maximo de topic entrante
    static const size_t MAX_SUBS  = 4;     // suscripciones que se re-suscriben solas


    MqttClient(
        WiFiManager& wifi,
        ESPAtDriver& esp
    );


    void setServer(const char* host, uint16_t port);

    void setCredentials(const char* user, const char* pass);

    void setClientId(const char* clientId);

    void setKeepAlive(uint16_t seconds);

    void onMessage(MessageCallback callback);


    // Abre TCP, envia CONNECT, espera CONNACK y re-suscribe los topics.
    bool connect();

    void disconnect();

    bool connected() const;


    // Registra el topic (se re-suscribe tras cada reconexion). Si ya hay
    // conexion, se suscribe de inmediato.
    bool subscribe(const char* topic);

    // QoS 0, sin retain. Payload de texto (maximo ~90 bytes con topic corto).
    bool publish(const char* topic, const char* payload);


    // Llamar en cada loop(): lee datos entrantes, mantiene el keepalive
    // y reintenta la conexion (con 5 s de espera entre intentos).
    void update();


private:

    enum class IpdState : uint8_t
    {
        Scan,
        Length,
        Payload
    };

    enum class RxState : uint8_t
    {
        Header,
        Length,
        Body
    };

    static const size_t WIN = 9;   // ventana de deteccion = token mas largo ("SEND FAIL")


private:

    // --- dependencias y configuracion ---
    WiFiManager& _wifi;
    ESPAtDriver& _esp;

    const char*     _host     = nullptr;
    uint16_t        _port     = 1883;
    const char*     _user     = nullptr;
    const char*     _pass     = nullptr;
    const char*     _clientId = "arduino";
    uint16_t        _keepAlive = 60;
    MessageCallback _callback = nullptr;

    const char* _subs[MAX_SUBS];
    uint8_t     _subCount = 0;

    // --- estado de sesion ---
    bool          _connected   = false;
    bool          _pingPending = false;
    unsigned long _lastTx      = 0;
    unsigned long _pingSentAt  = 0;
    unsigned long _nextAttempt = 0;
    uint16_t      _packetId    = 0;
    uint8_t       _failCount   = 0;      // intentos de conexion fallidos seguidos

    // --- banderas que activa el flujo entrante ---
    bool    _gotPrompt        = false;
    bool    _sendOk           = false;
    bool    _error            = false;
    bool    _closed           = false;
    bool    _busy             = false;   // el ESP contesto "busy s..." / "busy p..."
    bool    _connAckReceived  = false;
    bool    _subAckReceived   = false;
    uint8_t _connAckCode      = 0xFF;
    uint8_t _subAckCode       = 0xFF;

    // --- demux del flujo del ESP ("+IPD,<len>:<datos>") ---
    IpdState _ipdState     = IpdState::Scan;
    uint16_t _ipdLen       = 0;
    uint16_t _ipdRemaining = 0;
    char     _win[WIN];

    // --- ensamblador de paquetes MQTT ---
    RxState  _rxState   = RxState::Header;
    uint8_t  _pktHeader = 0;
    uint32_t _remLen    = 0;
    uint32_t _remMult   = 1;
    uint32_t _bodyIdx   = 0;
    uint8_t  _rxBuf[RX_MAX];
    char     _topic[TOPIC_MAX];

    uint8_t  _tx[TX_MAX];


private:

    void resetStream();

    bool openTcp();

    void closeTcp();

    bool sendPacket(const uint8_t* data, size_t length);

    bool waitFlag(bool& flag, unsigned long timeoutMs);

    void pump();

    void processByte(uint8_t b);

    bool endsWith(const char* token) const;

    void feedMqtt(uint8_t b);

    void handlePacket(uint16_t length);

    void handlePublish(uint16_t length);

    size_t buildConnect();

    bool subscribeNow(const char* topic);

};


}
}
}
}
