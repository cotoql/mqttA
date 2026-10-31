#include "Command.h"

#include <string.h>

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

Command::Command()
{
    clear();
}

void Command::clear()
{
    _type = Type::Unknown;

    _data[0] = '\0';

    _httpStatus = 0;

    _dataLength = 0;
}

void Command::setType(Type type)
{
    _type = type;
}

void Command::setData(const char* data)
{
    if(data == nullptr)
    {
        _data[0] = '\0';
        return;
    }

    strncpy(_data,
            data,
            DATA_SIZE - 1);

    _data[DATA_SIZE - 1] = '\0';
}

void Command::setHttpStatus(uint16_t status)
{
    _httpStatus = status;
}

void Command::setDataLength(uint16_t length)
{
    _dataLength = length;
}

Command::Type Command::type() const
{
    return _type;
}

const char* Command::data() const
{
    return _data;
}

uint16_t Command::httpStatus() const
{
    return _httpStatus;
}

uint16_t Command::dataLength() const
{
    return _dataLength;
}

bool Command::is(Type type) const
{
    return _type == type;
}

bool Command::hasData() const
{
    return _data[0] != '\0';
}

}
}
}
}
}