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

template<size_t Size>
class RingBuffer
{
public:

    RingBuffer();

    void clear();

    bool push(char c);

    bool pop(char& c);

    bool peek(char& c) const;

    bool at(size_t index, char& c) const;

    bool empty() const;

    bool full() const;

    size_t size() const;

    size_t capacity() const;

    size_t free() const;

    char operator[](size_t index) const;

    // Cuántos bytes se han perdido por llegar con el buffer lleno.
    // push() los descarta silenciosamente (no puede bloquear ni hacer
    // crecer el buffer), pero este contador permite detectarlo desde
    // afuera en vez de que el overflow pase completamente inadvertido.
    size_t droppedCount() const;

    void resetDroppedCount();

private:

    char _buffer[Size];

    size_t _head;

    size_t _tail;

    size_t _count;

    size_t _dropped;
};

}
}
}
}
}

#include "RingBuffer.tpp"