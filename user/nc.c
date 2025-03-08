#include "types.h"
#include "syscalls.h"
#include "inet.h"
#include "Socket.h"
#include "unix/stdio.h"
#include "string.h"

void TCPServer(unsigned short Port)
{
    printf("TCP Server starting...\n");
    int SocketFD = socket(Internet, Stream, 0);
    if (SocketFD < 0)
    {
        printf("TCP Server start failed\n");
        return;
    }
    printf("TCP Server started (%d)\n", SocketFD);

    printf("Binding port: %d\n", Port);
    if (bind(SocketFD, 0, Port) < 0)
    {
        printf("Failed to bind port.\n");
        goto finished;
    }
    printf("DONE.\n");

    listen(SocketFD, 120);

    printf("Waiting for connection...\n");
    unsigned int DIPAddress;
    unsigned short DPort;
    int DestinationFD = accept(SocketFD, &DIPAddress, &DPort);
    if (DestinationFD < 0)
    {
        printf("Failed to accept.\n");
        goto finished;
    }
    printf("%d.%d.%d.%d:%d connected.\n",
        (DIPAddress) & 0xFF,
        (DIPAddress >> 8) & 0xFF,
        (DIPAddress >> 16) & 0xFF,
        (DIPAddress >> 24) & 0xFF, DPort);

    char Buffer[2000];
    while (1)
    {
        int ReturnValue = sockrecv(DestinationFD, Buffer, 2000);
        if (ReturnValue <= 0)
        {
            printf("%d.%d.%d.%d:%d disconnected\n",
                (DIPAddress) & 0xFF,
                (DIPAddress >> 8) & 0xFF,
                (DIPAddress >> 16) & 0xFF,
                (DIPAddress >> 24) & 0xFF, DPort);
            break;
        }
        printf("Received %d bytes data: %s", ReturnValue, Buffer);
        //socksend(DestinationFD, Buffer, ReturnValue);
        for (int i = 0; i < ReturnValue; ++i)
        {
            Buffer[i] = 0;
        }
    }

    finished:
    close(SocketFD);
}

void UDPServer(unsigned short Port)
{
    printf("UDP Server starting...\n");
    int SocketFD = socket(Internet, Datagram, 0);
    if (SocketFD < 0)
    {
        printf("UDP Server start failed\n");
        return;
    }
    printf("UDP Server started (%d)\n", SocketFD);

    printf("Binding port: %d\n", Port);
    if (bind(SocketFD, 0, Port) < 0)
    {
        printf("Failed to bind port.\n");
        goto finished;
    }
    printf("DONE.\n");

    printf("Receiving data...\n");
    char Buffer[2000];
    while (1)
    {
        unsigned int DIPAddress;
        unsigned short DPort;
        int ReturnValue = sockrecvfrom(SocketFD, &DIPAddress, &DPort, Buffer, 2000);
        if (ReturnValue <= 0 || !strncmp(Buffer, "q!\n", -1))
        {
            printf("Stopping...\n");
            break;
        }
        printf("Received %d bytes data from %d.%d.%d.%d:%d : %s",
            ReturnValue,
            (DIPAddress) & 0xFF,
            (DIPAddress >> 8) & 0xFF,
            (DIPAddress >> 16) & 0xFF,
            (DIPAddress >> 24) & 0xFF,
            DPort,
            Buffer);
        for (int i = 0; i < ReturnValue; ++i)
        {
            Buffer[i] = 0;
        }
    }

    finished:
    close(SocketFD);
}

int main(int argc, char *argv[])
{
    if (argc == 1) {return 0x0D000721;}
    if (argc == 2)
    {
        TCPServer(atoi(argv[1]));
    }
    else
    {
        if (!strncmp(argv[2], "-u", -1))
        {
            UDPServer(atoi(argv[1]));
        }
    }
    return procexit();
}
