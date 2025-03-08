#include "types.h"
#include "unix/stdio.h"
#include "unix/stdlib.h"
#include "string.h"
#include "syscalls.h"
#include "unix/stdint.h"
#include "inet.h"

unsigned StringToIPHex(const char* Str, _Bool* OK)
{
    char* StartPoint = (char*)Str, *EndPoint;
    union{uint32_t i; uint8_t b[4];} Result;
    long Temp = 0;
    Result.i = 0;

    for (int i = 0; i < 4; ++i)
    {
        Temp = ustrtol(StartPoint, &EndPoint, 10);
        if ((Temp < 0 || Temp > 255) ||
            (StartPoint == EndPoint) ||
            ((i == 3 && *EndPoint != '\0') || (i != 3 && *EndPoint != '.')))
        {
            if (OK) {*OK = 0;}
            return -1;
        }
        Result.b[i] = Temp;
        StartPoint = EndPoint + 1;
    }

    if (OK) {*OK = 1;}
    return Result.i;
}

int main(int argc, char *argv[])
{
    _Bool OK = 1;
    uint32_t IP = StringToIPHex(argv[1], &OK);
    if (!OK) {procexit();}
    Ping(IP);
    return procexit();
}
