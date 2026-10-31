#pragma once

#include <Arduino.h>

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

class Command
{
public:

    enum class Type
    {
        Unknown,

        Ok,
        Error,
        Fail,

        Ready,
        Busy,

        Prompt,

        WifiConnected,
        WifiDisconnected,
        WifiGotIp,

        Connect,
        Closed,

        SendOk,
        SendFail,

        IpAddress,

        HttpResponse,

        Data
    };

    static const size_t DATA_SIZE = 128;

public:

    Command();

    void clear();

    void setType(Type type);

    void setData(const char* data);

    void setHttpStatus(uint16_t status);

    void setDataLength(uint16_t length);

    Type type() const;

    const char* data() const;

    uint16_t httpStatus() const;

    uint16_t dataLength() const;

    bool is(Type type) const;

    bool hasData() const;

private:

    Type _type;

    char _data[DATA_SIZE];

    uint16_t _httpStatus;

    uint16_t _dataLength;
};

}
}
}
}
}