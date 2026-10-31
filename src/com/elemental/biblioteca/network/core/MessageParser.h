#pragma once

#include <Arduino.h>

#include "Command.h"

namespace com
{
namespace elemental
{
namespace biblioteca
{
namespace network
{
namespace core
{

class MessageParser
{
public:

    MessageParser();

    bool parse(
        const char* line,
        Command& command
    );

private:

    bool parseIpAddress(
        const char* line,
        Command& command
    );

    bool parseHttpResponse(
        const char* line,
        Command& command
    );

    bool parseIpData(
        const char* line,
        Command& command
    );

    uint16_t parseHttpStatus(
        const char* line
    );

    uint16_t parseIpdLength(
        const char* line
    );

};

}
}
}
}
}