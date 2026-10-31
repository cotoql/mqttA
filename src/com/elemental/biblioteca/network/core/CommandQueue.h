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

class CommandQueue
{
public:

    static const size_t MAX_COMMANDS = 16;

    CommandQueue();

    void clear();

    bool push(const Command& command);

    bool pop(Command& command);

    bool peek(Command& command) const;

    bool empty() const;

    bool full() const;

    size_t size() const;

    size_t capacity() const;

private:

    Command _commands[MAX_COMMANDS];

    size_t _head;

    size_t _tail;

    size_t _count;
};

}
}
}
}
}