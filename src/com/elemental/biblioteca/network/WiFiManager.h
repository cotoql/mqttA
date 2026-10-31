#pragma once

#include <Arduino.h>

#include "core/ESPAtDriver.h"
#include "core/Command.h"

// WIFI_SSID y WIFI_PASSWORD viven en src/secrets.h
#include "../../../../secrets.h"

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
