#include "WiFiManager.h"


namespace com
{
namespace elemental
{
namespace biblioteca
{
namespace network
{


WiFiManager::WiFiManager(
        ESPAtDriver& driver
)
:
_esp(driver)
{

}



bool WiFiManager::begin() {
    //_esp.begin(ESP_SERIAL_BAUD);

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



    String resp = sendAT(
        cmd,
        20000,
        "WIFI CONNECTED"
    );



    if(resp.indexOf("WIFI CONNECTED") != -1)
    {

        waitFor(
            "GOT IP",
            5000
        );


        delay(1000);


        Serial.println(
            F("[WiFi] Conectado")
        );


        Serial.println(
            getIP()
        );


        return true;

    }



    if(resp.indexOf("ALREADY CONNECTED") != -1)
    {
        return true;
    }



    return false;

}



bool WiFiManager::isConnected()
{

    String resp = sendAT(
        "AT+CWJAP?",
        5000
    );


    return
        resp.indexOf("+CWJAP:") != -1 &&
        resp.indexOf("No AP") == -1;

}



String WiFiManager::getIP()
{

    String resp = sendAT(
        "AT+CIFSR"
    );


    int pos = resp.indexOf("STAIP,\"");

if(pos < 0)
    return "";

pos += strlen("STAIP,\"");


    int end = resp.indexOf(
        "\"",
        pos
    );


    return resp.substring(
        pos,
        end
    );

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

            Serial.print("Tipo = ");
            Serial.println((int)command.type());
            Serial.print("Data = ");
            Serial.println(command.data());
            response += extractCommand(command);



            if(
                waitForStr.length() &&
                response.indexOf(waitForStr) != -1
            )
            {
                return response;
            }


        }



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
