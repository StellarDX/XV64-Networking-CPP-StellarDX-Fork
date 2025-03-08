#ifndef SOCKET_H
#define SOCKET_H

enum DomainType
{
    Unspecified,
    Localhost,
    Internet
};

enum ConnectionType
{
    Stream,  // TCP
    Datagram // UDP
};

int socket(int Domain, int Type, int Protocol);
int bind(int SocketFD, unsigned int Address, int Port);
int listen(int SocketFD, int Backlog);
int accept(int SocketFD, unsigned int* DestiAddress, unsigned short* DestiPort);
int socksend(int SocketFD, const void* Source, int Size);
int sockrecv(int SocketFD, void* Destination, int Size);
int socksendto(int SocketFD, unsigned int Address, int Port, const void* Source, int Size);
int sockrecvfrom(int SocketFD, unsigned int* DestiAddress, unsigned short* DestiPort, void* Destination, int Size);

#endif // SOCKET_H
