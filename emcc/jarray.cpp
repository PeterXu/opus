#include "base/common.h"
#include <emscripten.h>

extern "C" {

// String

EMSCRIPTEN_KEEPALIVE
size_t String_size(String *self)
{
    return self->size();
}

EMSCRIPTEN_KEEPALIVE
String* String_new()
{
    return new std::string();
}

EMSCRIPTEN_KEEPALIVE
const char* String_data(String *self)
{
    return self->c_str();
}

EMSCRIPTEN_KEEPALIVE
void String_delete(String *self)
{
    delete self;
}

// Int16Array

EMSCRIPTEN_KEEPALIVE
size_t Int16Array_size(Int16Array *self)
{
    return self->size();
}

EMSCRIPTEN_KEEPALIVE
Int16Array* Int16Array_new()
{
    return new std::vector<int16_t>();
}

EMSCRIPTEN_KEEPALIVE
const int16_t* Int16Array_data(Int16Array *self)
{
    return &(*self)[0];
}

EMSCRIPTEN_KEEPALIVE
void Int16Array_delete(Int16Array *self)
{
    delete self;
}

} // extern "C"
