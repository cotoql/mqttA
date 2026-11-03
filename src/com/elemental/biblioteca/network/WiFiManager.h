#pragma once

#include <Arduino.h>

#include "core/ESPAtDriver.h"
#include "core/Command.h"

// WIFI_SSID y WIFI_PASSWORD viven en src/secrets.h
#include "../../../../secrets.h"

// 1 = imprime cada linea que llega del ESP ("Tipo = / Data = ").
// Es util para depurar, pero cada Serial.print bloquea y el RX de la Mega
// (64 bytes) puede desbordarse mientras tanto -> bytes perdidos.
// Con ATE0 el eco ya no se ve; si lo activas, la respuesta a CWJAP no trae la clave.
#ifndef WIFI_DEBUG_RX
#define WIFI_DEBUG_RX 0
#endif

namespace com
{
namespace elemental
{
namespace biblioteca
{
namespace network
{


using core::ESPAtDriver;
using core::Command;



class WiFiManager
{

public:

    explicit WiFiManager(
        ESPAtDriver& driver
    );


    bool begin();


    bool reconnect();


    bool isConnected();


    String getIP();


    bool sendRaw(
        const String& data
    );


    bool readRaw(
        String& data
    );


    String sendAT(
        const String& cmd,
        unsigned int timeoutMs = 2000,
        const String& waitForStr = ""
    );


    // Igual que sendAT(), pero si el ESP contesta "busy s..." / "busy p..."
    // (esta enviando o procesando) espera y reintenta hasta busyBudgetMs.
    // Nunca manda otro comando "encima" de uno en curso.
    String sendATWhenIdle(
        const String& cmd,
        unsigned int timeoutMs = 2000,
        unsigned int busyBudgetMs = 12000,
        const String& waitForStr = ""
    );


    static bool isBusy(
        const String& resp
    );


    // AT+RST + begin(). Ultimo recurso cuando el ESP queda colgado.
    bool resetModule();


    bool waitFor(
        const String& target,
        unsigned int timeoutMs
    );


    void flushESP();


private:

    ESPAtDriver& _esp;


    String extractCommand(
        const Command& command
    );

};


}
}
}
}
