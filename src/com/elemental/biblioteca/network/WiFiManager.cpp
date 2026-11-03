#include "WiFiManager.h"


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

// Cuanto esperar a que el ESP salga de "busy s..." / "busy p...".
const unsigned int BUSY_BUDGET_MS = 12000;

}


WiFiManager::WiFiManager(
        ESPAtDriver& driver
)
:
_esp(driver)
{

}



bool WiFiManager::begin() {
    //_esp.begin(ESP_SERIAL_BAUD);
       sendAT("AT+RFPOWER=50");

    delay(500);

    flushESP();
    Serial.println(
        F("[WiFi] Iniciando ESP-01...")
    );
    String resp = sendAT("AT");
    if(resp.indexOf("OK") == -1)
    {
        Serial.println(
            F("[WiFi] ERROR: ESP no responde")
        );
        return false;
    }

    Serial.println(
        F("[WiFi] ESP detectado")
    );


    // Sin eco: baja el trafico serie (cada comando ya no vuelve como texto),
    // evita que el eco se mezcle con respuestas como CLOSED y mantiene la
    // clave WiFi fuera de cualquier log. Se pierde con AT+RST, por eso va aqui.
    sendAT("ATE0");


    sendAT("AT+CWMODE=1");


    delay(500);


    sendAT("AT+SLEEP=0");


    delay(200);


    sendAT("AT+CWQAP",3000);

    delay(1000);

    flushESP();

    return reconnect();

}

bool WiFiManager::sendRaw(
        const String& data
)
{

    return _esp.writeRaw(
        data.c_str()
    );

}

bool WiFiManager::readRaw(
        String& data
)
{

    return _esp.readRaw(data);

}

bool WiFiManager::reconnect()
{

    String cmd;


    cmd = "AT+CWJAP=\"";
    cmd += WIFI_SSID;
    cmd += "\",\"";
    cmd += WIFI_PASSWORD;
    cmd += "\"";



    Serial.println(
        F("[WiFi] Conectando...")
    );



    String resp = sendATWhenIdle(
        cmd,
        20000,
        BUSY_BUDGET_MS,
        "WIFI CONNECTED"
    );



    bool joined =
        resp.indexOf("WIFI CONNECTED") != -1 ||
        resp.indexOf("ALREADY CONNECTED") != -1;

    if(!joined)
        return false;



    if(resp.indexOf("WIFI CONNECTED") != -1)
    {

        // Orden normal: WIFI CONNECTED -> WIFI GOT IP -> OK. Si no se
        // consume el OK final, se le puede colar al siguiente comando.
        waitFor(
            "GOT IP",
            8000
        );

        waitFor(
            "OK",
            2000
        );

    }



    // "WIFI CONNECTED" no garantiza IP: el log mostraba CIPSTART -> "no ip".
    // Se consulta CIFSR hasta tener una IP valida.
    String ip;

    unsigned long start = millis();

    do
    {

        ip = getIP();

        if(ip.length() > 0 && ip != "0.0.0.0")
            break;

        delay(500);

    }
    while(millis() - start < 10000);



    if(ip.length() == 0 || ip == "0.0.0.0")
    {

        Serial.println(
            F("[WiFi] Sin IP")
        );

        return false;

    }



    Serial.println(
        F("[WiFi] Conectado")
    );

    Serial.println(
        ip
    );


    return true;

}



bool WiFiManager::isConnected()
{

    String resp = sendATWhenIdle(
        "AT+CWJAP?",
        5000,
        BUSY_BUDGET_MS
    );


    // "busy s..." / "busy p..." NO significa WiFi caido: el ESP solo esta
    // ocupado. Declararlo caido provocaba un CWJAP que derribaba el TCP.
    if(isBusy(resp))
        return true;

    if(resp.indexOf("+CWJAP:") == -1 || resp.indexOf("No AP") != -1)
        return false;

    // Asociado al AP no basta: hace falta IP (DHCP).
    String ip = getIP();
    return ip.length() > 0 && ip != "0.0.0.0";

}



String WiFiManager::getIP()
{

    String resp = sendATWhenIdle(
        "AT+CIFSR",
        2000,
        4000
    );


    int pos = resp.indexOf("STAIP,\"");

    if(pos < 0)
        return "";

    pos += strlen("STAIP,\"");


    int end = resp.indexOf(
        "\"",
        pos
    );

    if(end < 0)
        return "";


    return resp.substring(
        pos,
        end
    );

}



bool WiFiManager::isBusy(
        const String& resp
)
{

    return resp.indexOf("busy") != -1;

}



String WiFiManager::sendATWhenIdle(
        const String& cmd,
        unsigned int timeoutMs,
        unsigned int busyBudgetMs,
        const String& waitForStr
)
{

    unsigned long start = millis();

    String resp;


    for(;;)
    {

        resp = sendAT(
            cmd,
            timeoutMs,
            waitForStr
        );


        if(!isBusy(resp))
            return resp;


        if(millis() - start >= busyBudgetMs)
            return resp;


        delay(300);

    }

}



bool WiFiManager::resetModule()
{

    Serial.println(
        F("[WiFi] Reiniciando modulo (AT+RST)...")
    );


    flushESP();

    _esp.send("AT+RST");


    // Tras el reset el ESP escribe basura a 74880 baud y luego "ready".
    waitFor(
        "ready",
        6000
    );

    delay(1500);

    flushESP();


    return begin();

}



String WiFiManager::sendAT(
        const String& cmd,
        unsigned int timeoutMs,
        const String& waitForStr
)
{

    flushESP();


    // Si estábamos esperando el prompt ">" de un AT+CIPSEND anterior
    // que nunca se resolvió (p.ej. por timeout), no queremos arrastrar
    // esa señal hacia este comando nuevo.
    _esp.clearPrompt();


    bool waitingForPrompt = (waitForStr == ">");


    _esp.send(cmd.c_str());
     delay(10);



    String response;


    unsigned long start = millis();



    while(
        millis() - start < timeoutMs
    )
    {


        _esp.update();


        // ── Caso especial: prompt ">" ───────────────────────────
        // No pasa por readCommand()/CommandQueue porque el ESP no lo
        // termina con '\n' (ver ESPAtDriver::readLine). Se detecta a
        // nivel de bytes vía promptPending(), evitando el deadlock de
        // esperar una línea completa que nunca va a llegar mientras
        // el ESP está esperando que nosotros le mandemos datos.
        if(waitingForPrompt && _esp.promptPending())
        {

            _esp.clearPrompt();

            response += ">";

            return response;

        }


        Command command;



        while(
            _esp.readCommand(command)
        )
        {

#if WIFI_DEBUG_RX
            Serial.print("Tipo = ");
            Serial.println((int)command.type());
            Serial.print("Data = ");
            Serial.println(command.data());
#endif
            response += extractCommand(command);



            if(
                waitForStr.length() &&
                response.indexOf(waitForStr) != -1
            )
            {
                return response;
            }


        }



        // El ESP rechaza el comando con "busy s..." / "busy p..." y NO manda
        // OK/ERROR despues: no tiene sentido esperar todo el timeout.
        if(isBusy(response))
            break;



        if(waitForStr.length()==0)
        {

            if(
                response.indexOf("OK")!=-1 ||
                response.indexOf("ERROR")!=-1 ||
                response.indexOf("FAIL")!=-1
            )
            {
                break;
            }

        }


    }



    return response;

}



bool WiFiManager::waitFor(
        const String& target,
        unsigned int timeoutMs
)
{

    String buffer;


    unsigned long start = millis();



    while(
        millis()-start < timeoutMs
    )
    {

        _esp.update();



        Command command;



        while(
            _esp.readCommand(command)
        )
        {

            buffer += extractCommand(command);


            if(buffer.indexOf(target)!=-1)
                return true;

        }

    }



    return false;

}



void WiFiManager::flushESP()
{
    String basura;

    while(_esp.readRaw(basura))
    {
        basura = "";
    }

    Command command;

    while(_esp.readCommand(command))
    {

    }
}



String WiFiManager::extractCommand(const Command& command)
{
    switch(command.type())
    {
        case Command::Type::Ok:
            return "OK\n";

        case Command::Type::Error:
            return "ERROR\n";

        case Command::Type::Fail:
            return "FAIL\n";

        case Command::Type::Connect:
            return "CONNECT\n";

        case Command::Type::Closed:
            return "CLOSED\n";

        case Command::Type::WifiConnected:
            return "WIFI CONNECTED\n";

        case Command::Type::WifiGotIp:
            return "WIFI GOT IP\n";

        case Command::Type::WifiDisconnected:
            return "WIFI DISCONNECT\n";

        case Command::Type::Ready:
            return "ready\n";

        case Command::Type::Busy:
            return "busy p...\n";

        case Command::Type::Prompt:
            return ">\n";

        default:
            return String(command.data()) + "\n";
    }
}


}
}
}
}
