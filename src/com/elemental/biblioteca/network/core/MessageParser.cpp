#include "MessageParser.h"

#include <string.h>
#include <stdlib.h>

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

MessageParser::MessageParser()
{
}

bool MessageParser::parse(
    const char* line,
    Command& command
)
{
    command.clear();

    if(line == nullptr)
        return false;

    //-------------------------------------------------
    // Respuestas simples
    //-------------------------------------------------

    if(strcmp(line,"OK")==0)
    {
        command.setType(Command::Type::Ok);
        return true;
    }

    if(strcmp(line,"ERROR")==0)
    {
        command.setType(Command::Type::Error);
        return true;
    }

    if(strcmp(line,"FAIL")==0)
    {
        command.setType(Command::Type::Fail);
        return true;
    }

    if(strcmp(line,"ready")==0)
    {
        command.setType(Command::Type::Ready);
        return true;
    }

    if(strcmp(line,"busy p...")==0)
    {
        command.setType(Command::Type::Busy);
        return true;
    }

    if(strcmp(line,"SEND OK")==0)
    {
        command.setType(Command::Type::SendOk);
        return true;
    }

    if(strcmp(line,"SEND FAIL")==0)
    {
        command.setType(Command::Type::SendFail);
        return true;
    }

    if(strcmp(line,"CONNECT")==0)
    {
        command.setType(Command::Type::Connect);
        return true;
    }

    if(strcmp(line,"CLOSED")==0)
    {
        command.setType(Command::Type::Closed);
        return true;
    }

    if(strcmp(line,"WIFI CONNECTED")==0)
    {
        command.setType(Command::Type::WifiConnected);
        return true;
    }

    if(strcmp(line,"WIFI DISCONNECT")==0)
    {
        command.setType(Command::Type::WifiDisconnected);
        return true;
    }

    if(strcmp(line,"WIFI GOT IP")==0)
    {
        command.setType(Command::Type::WifiGotIp);
        return true;
    }

    if(strcmp(line,">")==0)
    {
        command.setType(Command::Type::Prompt);
        return true;
    }

    //-------------------------------------------------
    // Respuestas complejas
    //-------------------------------------------------

    if(parseIpAddress(line,command))
        return true;

    if(parseHttpResponse(line,command))
        return true;

    if(parseIpData(line,command))
        return true;

    //-------------------------------------------------

    command.setType(Command::Type::Unknown);
    command.setData(line);

    return true;
}

bool MessageParser::parseIpAddress(
    const char* line,
    Command& command
)
{
    if(strstr(line,"STAIP")==nullptr)
        return false;

    command.setType(Command::Type::IpAddress);
    command.setData(line);

    return true;
}

bool MessageParser::parseHttpResponse(
    const char* line,
    Command& command
)
{
    if(strncmp(line,"HTTP/",5)!=0)
        return false;

    command.setType(Command::Type::HttpResponse);

    command.setHttpStatus(
        parseHttpStatus(line)
    );

    command.setData(line);

    return true;
}

bool MessageParser::parseIpData(
    const char* line,
    Command& command
)
{
    if(strncmp(line,"+IPD",4)!=0)
        return false;

    command.setType(Command::Type::Data);

    command.setDataLength(
        parseIpdLength(line)
    );

    command.setData(line);

    return true;
}

uint16_t MessageParser::parseHttpStatus(
    const char* line
)
{
    const char* p=strchr(line,' ');

    if(p==nullptr)
        return 0;

    return (uint16_t)atoi(p+1);
}

uint16_t MessageParser::parseIpdLength(
    const char* line
)
{
    const char* p=strchr(line,':');

    if(p==nullptr)
        return 0;

    const char* comma=strchr(line,',');

    if(comma==nullptr || comma>p)
        return 0;

    return (uint16_t)atoi(comma+1);
}

}
}
}
}
}