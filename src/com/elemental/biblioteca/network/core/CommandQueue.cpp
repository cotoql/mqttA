#include "CommandQueue.h"

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

CommandQueue::CommandQueue()
{
    clear();
}

void CommandQueue::clear()
{
    _head = 0;
    _tail = 0;
    _count = 0;
}

bool CommandQueue::push(const Command& command)
{
    if(full())
        return false;

    _commands[_head] = command;

    _head = (_head + 1) % MAX_COMMANDS;

    ++_count;

    return true;
}

bool CommandQueue::pop(Command& command)
{
    if(empty())
        return false;

    command = _commands[_tail];

    _tail = (_tail + 1) % MAX_COMMANDS;

    --_count;

    return true;
}

bool CommandQueue::peek(Command& command) const
{
    if(empty())
        return false;

    command = _commands[_tail];

    return true;
}

bool CommandQueue::empty() const
{
    return (_count == 0);
}

bool CommandQueue::full() const
{
    return (_count == MAX_COMMANDS);
}

size_t CommandQueue::size() const
{
    return _count;
}

size_t CommandQueue::capacity() const
{
    return MAX_COMMANDS;
}

}
}
}
}
}