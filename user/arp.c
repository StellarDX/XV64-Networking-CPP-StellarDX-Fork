#include "types.h"
#include "syscalls.h"
#include "inet.h"

int main(int argc, char *argv[])
{
    PrintARPTable();
    return procexit();
}
