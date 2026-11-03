#include "ESPAtDriver.h"

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


ESPAtDriver::ESPAtDriver(
        HardwareSerial& serial,
        RingBuffer<128>& buffer,
        MessageParser& parser,
        CommandQueue& queue
)
:
_serial(serial),
_buffer(buffer),
_parser(parser),
_queue(queue)
{

}



bool ESPAtDriver::begin(uint32_t baud)
{
    _serial.begin(baud);

    delay(200);

    _buffer.clear();

    while(_serial.available())
        _serial.read();

    return true;
}



void ESPAtDriver::update()
{

    /*
        1.
        Leer datos del ESP
    */

while(_serial.available())
{
    char c = (char)_serial.read();

    //Serial.print("[RX] ");
    //Serial.write(c);
    //Serial.println();

    _buffer.push(c);
}


    /*
        2.
        Buscar líneas completas
    */

    while(readLine())
    {

        Command command;


        if(_parser.parse(_lineBuffer,command))
        {

            _queue.push(command);

        }

    }


}



bool ESPAtDriver::send(const char* command) {
    Serial.print(F("[ESP TX] "));

    // No volcar SSID/clave al monitor serie.
    if(strncmp(command, "AT+CWJAP=", 9) == 0)
        Serial.println(F("AT+CWJAP=\"***\",\"***\""));
    else
        Serial.println(command);

    _serial.println(command);
    return true;
}



bool ESPAtDriver::available()
{

    return !_queue.empty();

}



bool ESPAtDriver::readCommand(Command& command)
{

    return _queue.pop(command);

}

bool ESPAtDriver::writeRaw(
        const char* data
)
{

    _serial.print(data);

    return true;

}

bool ESPAtDriver::readRaw(
        String& data
)
{

    bool received=false;


    // 1) Primero drenar lo que ya haya sido leído del serial hacia el
    //    RingBuffer (p.ej. por un update() previo) pero que todavía no
    //    haya sido reclamado por readLine(). Si no se hace esto, esos
    //    bytes quedan huérfanos al pasar del modo "líneas" al modo
    //    "crudo" (nadie más los va a leer).
    char buffered;

    while(_buffer.pop(buffered))
    {

        data += buffered;

        received=true;

    }


    // 2) Luego seguir leyendo directo del serial: datos nuevos que
    //    aún no pasaron por update()/_buffer.
    while(_serial.available())
    {

        char c=_serial.read();

        data += c;

        received=true;

    }


    return received;

}



size_t ESPAtDriver::writeBytes(const uint8_t* data, size_t length)
{
    if(data == nullptr || length == 0)
        return 0;

    return _serial.write(data, length);
}



int ESPAtDriver::readByte()
{
    char c;

    if(_buffer.pop(c))
        return (uint8_t)c;

    if(_serial.available())
        return _serial.read();

    return -1;
}



bool ESPAtDriver::promptPending() const
{

    return _promptPending;

}



void ESPAtDriver::clearPrompt()
{

    _promptPending = false;

}



size_t ESPAtDriver::droppedBytes() const
{

    return _buffer.droppedCount();

}



bool ESPAtDriver::readLine()
{

    char c;


    while(_buffer.pop(c))
    {

        // ── Detección del prompt ">" ────────────────────────────
        // El ESP lo manda como primer carácter de una "línea" que
        // nunca se completa con '\n' (se queda esperando datos
        // crudos). Lo detectamos aparte, sin meterlo en _lineBuffer
        // ni depender de MessageParser/CommandQueue.
        if(_lineIndex == 0 && c == '>')
        {

            _promptPending = true;

            _skipSpaceAfterPrompt = true;

            continue;

        }


        if(_skipSpaceAfterPrompt)
        {

            _skipSpaceAfterPrompt = false;


            if(c == ' ')
                continue;

        }


        if(c == '\n')
        {

            _lineBuffer[_lineIndex]='\0';

            _lineIndex=0;


            return true;

        }


        if(c != '\r')
        {

            if(_lineIndex < sizeof(_lineBuffer)-1)
            {

                _lineBuffer[_lineIndex++] = c;

            }

        }

    }


    return false;

} 



}
}
}
}
}