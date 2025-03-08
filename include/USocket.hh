#ifndef USOCKET_HH
#define USOCKET_HH

#include "UDef.hh"

#ifdef __cplusplus
_EXTERN_C
#endif

#include "file.h"

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

struct file* CreateSocket(int Domain, int Type, int Protocol);
void DestorySocket(struct file* f);
int BindSocket(const struct file* f, DWORD Address, WORD Port);
int SocketStartListen(const struct file* f, int Backlog);
struct file* SocketAccept(const struct file* f, DWORD* DestinationAddress, WORD* DestinationPort);
int SocketRead(const struct file* f, LPVOID Buffer, int Size);
int SocketReceiveFrom(const struct file* f, DWORD* Address, WORD* Port, LPVOID Buffer, int Size);
int SocketWrite(const struct file* f, LPCVOID Buffer, int Size);
int SocketSendTo(const struct file* f, DWORD Address, WORD Port, LPVOID Buffer, int Size);

int SOC_CreateSocket();
int SOC_BindSocket();
int SOC_SocketStartListen();
int SOC_SocketAccept();
int SOC_SocketRead();
int SOC_SocketReceiveFrom();
int SOC_SocketWrite();
int SOC_SocketSendTo();

#ifdef __cplusplus
_END_EXTERN_C
#endif

#endif // USOCKET_HH
