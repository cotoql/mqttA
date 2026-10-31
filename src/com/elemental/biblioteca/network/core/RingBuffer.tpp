#pragma once

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

template<size_t Size>
RingBuffer<Size>::RingBuffer()
{
    clear();
}

template<size_t Size>
void RingBuffer<Size>::clear()
{
    _head = 0;
    _tail = 0;
    _count = 0;
    _dropped = 0;
}

template<size_t Size>
bool RingBuffer<Size>::push(char c)
{
    if(full())
    {
        ++_dropped;
        return false;
    }

    _buffer[_head] = c;

    _head = (_head + 1) % Size;

    ++_count;

    return true;
}

template<size_t Size>
bool RingBuffer<Size>::pop(char& c)
{
    if(empty())
        return false;

    c = _buffer[_tail];

    _tail = (_tail + 1) % Size;

    --_count;

    return true;
}

template<size_t Size>
bool RingBuffer<Size>::peek(char& c) const
{
    if(empty())
        return false;

    c = _buffer[_tail];

    return true;
}

template<size_t Size>
bool RingBuffer<Size>::at(size_t index, char& c) const
{
    if(index >= _count)
        return false;

    c = _buffer[(_tail + index) % Size];

    return true;
}

template<size_t Size>
bool RingBuffer<Size>::empty() const
{
    return (_count == 0);
}

template<size_t Size>
bool RingBuffer<Size>::full() const
{
    return (_count == Size);
}

template<size_t Size>
size_t RingBuffer<Size>::size() const
{
    return _count;
}

template<size_t Size>
size_t RingBuffer<Size>::capacity() const
{
    return Size;
}

template<size_t Size>
size_t RingBuffer<Size>::free() const
{
    return Size - _count;
}

template<size_t Size>
char RingBuffer<Size>::operator[](size_t index) const
{
    if(index >= _count)
        return '\0';

    return _buffer[(_tail + index) % Size];
}

template<size_t Size>
size_t RingBuffer<Size>::droppedCount() const
{
    return _dropped;
}

template<size_t Size>
void RingBuffer<Size>::resetDroppedCount()
{
    _dropped = 0;
}

}
}
}
}
}