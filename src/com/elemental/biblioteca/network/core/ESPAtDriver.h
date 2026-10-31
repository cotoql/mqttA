#pragma once

#include <Arduino.h>

#include "RingBuffer.h"
#include "MessageParser.h"
#include "CommandQueue.h"
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


class ESPAtDriver
{

public:

    ESPAtDriver(
        HardwareSerial& serial,
        RingBuffer<128>& buffer,
        MessageParser& parser,
        CommandQueue& queue
    );


    bool begin(uint32_t baud);


    void update();


    bool send(const char* command);


    bool available();


    bool readCommand(Command& command);

bool writeRaw(
    const char* data
);


bool readRaw(
    String& data
);

    // Escribe bytes crudos (pueden incluir 0x00; writeRaw() corta en el
    // primer '\0' porque usa print(const char*)). Necesario para paquetes
    // binarios como los de MQTT.
    size_t writeBytes(const uint8_t* data, size_t length);

    // Lee UN byte crudo: primero lo que haya en el RingBuffer y luego
    // directo del serial. Devuelve -1 si no hay nada. No pasa por el
    // parser de lineas, asi que sirve para flujos binarios (+IPD).
    int readByte();

    // ── Detección del prompt ">" ───────────────────────────────
    // El ESP-AT responde al ">" de AT+CIPSEND SIN terminarlo en '\n'
    // (queda esperando los bytes crudos justo después). Por eso NO
    // puede detectarse vía readCommand()/MessageParser, que solo
    // entregan líneas completas. Este mecanismo lo detecta a nivel
    // de bytes, de forma independiente al pipeline de líneas/cola.
    bool promptPending() const;

    void clearPrompt();

    // Cuántos bytes se han perdido por overflow del RingBuffer interno.
    size_t droppedBytes() const;

private:

    // Devuelve true cuando _lineBuffer contiene una línea completa
    // y recién terminada (ver definición para detalles).
    bool readLine();


private:

    HardwareSerial& _serial;

    RingBuffer<128>& _buffer;

    MessageParser& _parser;

    CommandQueue& _queue;

    // Línea en construcción. Antes era un array LOCAL a update(), lo
    // cual es un bug: si una línea llegaba partida entre dos llamadas
    // a update() (algo normal, los bytes de Serial llegan de a poco),
    // los caracteres ya acumulados se perdían al salir de la función
    // (memoria de pila reciclada), pero el índice seguía "recordando"
    // una posición que ya no correspondía a nada válido. Ahora todo
    // el estado de la línea en construcción vive en el objeto.
    char _lineBuffer[128];

    size_t _lineIndex = 0;

    bool _promptPending = false;

    // Tras consumir un '>' al inicio de línea, el ESP suele mandar un
    // espacio justo después ("> "). Si no lo descartamos, ese espacio
    // quedaría pegado al inicio de la siguiente línea real (" SEND OK"
    // en vez de "SEND OK"), rompiendo las comparaciones de MessageParser.
    bool _skipSpaceAfterPrompt = false;


};


} // namespace core
} // namespace network
} // namespace biblioteca
} // namespace elemental
} // namespace com