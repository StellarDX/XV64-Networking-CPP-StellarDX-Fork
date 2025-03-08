#include "USocket.hh"
#include "UProtocols4.tcc"

_EXTERN_C

file* filealloc(void);
int argint(int n, int* ip);
int fdalloc(struct file* f);
void fileclose(file* f);
int argfd(int n, int* pfd, struct file** pf);
int argptr(int n, char** pp, int size);

file* CreateSocket(int Domain, int Type, int Protocol)
{
    if (Domain != Internet || (Type != Stream && Type != Datagram) || Protocol)
    {
        return nullptr;
    }

    file* f = filealloc();
    if (!f) {return nullptr;}
    f->Socket.Type = Type;
    f->Socket.Desc = (Type == Stream) ? TCB<4>::Open() : UDPController<4>::Open();
    f->type = file::FD_SOCKET;
    f->readable = 1;
    f->writable = 1;
    return f;
}

void DestorySocket(file* f)
{
    if (f->Socket.Type == Stream) {TCB<4>::Close(f->Socket.Desc);}
    else if (f->Socket.Type == Datagram) {UDPController<4>::Close(f->Socket.Desc);}
}

int BindSocket(const file* f, DWORD Address, WORD Port)
{
    switch (f->Socket.Type)
    {
    case Stream:
        return TCB<4>::Bind(f->Socket.Desc, Port);
    case Datagram:
        return UDPController<4>::Bind(f->Socket.Desc, Address, Port);
        break;
    default:
        return -1;
    }
}

int SocketStartListen(const file* f, int Backlog)
{
    switch (f->Socket.Type)
    {
    case Stream:
        return TCB<4>::Listen(f->Socket.Desc, Backlog);
    default:
        return -1;
    }
}

file* SocketAccept(const file* f, DWORD* DestinationAddress, WORD* DestinationPort)
{
    if (f->Socket.Type != Stream) {return nullptr;}
    file* af = filealloc();
    if (!af) {return nullptr;}
    int Accepted = TCB<4>::Accept(f->Socket.Desc, DestinationAddress, DestinationPort);
    if (Accepted < 0)
    {
        fileclose(af);
        return nullptr;
    }
    af->Socket.Type = f->Socket.Type;
    af->Socket.Desc = Accepted;
    af->type = file::FD_SOCKET;
    af->readable = 1;
    af->writable = 1;
    return af;
}

int SocketRead(const file* f, LPVOID Buffer, int Size)
{
    switch (f->Socket.Type)
    {
    case Stream:
        return TCB<4>::Receive(f->Socket.Desc, Buffer, Size);
    default:
        return -1;
    }
}

int SocketReceiveFrom(const file* f, DWORD* Address, WORD* Port, LPVOID Buffer, int Size)
{
    switch (f->Socket.Type)
    {
    case Datagram:
        return UDPController<4>::Receive(f->Socket.Desc, Address, Port, Buffer, Size);
    default:
        return -1;
    }
}

int SocketWrite(const file* f, LPCVOID Buffer, int Size)
{
    switch (f->Socket.Type)
    {
    case Stream:
        return TCB<4>::Transmit(f->Socket.Desc, Buffer, Size);
    default:
        return -1;
    }
}

int SocketSendTo(const file* f, DWORD Address, WORD Port, LPVOID Buffer, int Size)
{
    switch (f->Socket.Type)
    {
    case Datagram:
        return UDPController<4>::Transmit(f->Socket.Desc, Address, Port, Buffer, Size);
    default:
        return -1;
    }
}

// System calls

int SOC_CreateSocket()
{
    int Domain, Type, Protocol;
    file* f;
    if (argint(0, &Domain) < 0 || argint(1, &Type) < 0 || argint(2, &Protocol) < 0)
    {
        return -1;
    }
    f = CreateSocket(Domain, Type, Protocol);
    if (!f) {return -2;}
    int fd = fdalloc(f);
    if (fd < 0)
    {
        fileclose(f);
        return -3;
    }
    return fd;
}

int SOC_BindSocket()
{
    file* f;
    int Address;
    int Port;
    if (argfd(0, 0, &f) < 0 || argint(1, &Address) < 0 || argint(2, &Port) < 0)
    {
        return -1;
    }
    if (f->type != file::FD_SOCKET) {return -2;}
    return BindSocket(f, Address, Port);
}

int SOC_SocketStartListen()
{
    file* f;
    int Backlog;
    if (argfd(0, 0, &f) < 0 || argint(1, &Backlog) < 0)
    {
        return -1;
    }
    if (f->type != file::FD_SOCKET) {return -2;}
    return SocketStartListen(f, Backlog);
}

int SOC_SocketAccept()
{
    file* f, *af;
    int afd;
    DWORD* IP;
    WORD* Port;
    if (argfd(0, 0, &f) < 0 ||
        argptr(1, (char**)(&IP), sizeof(DWORD)) < 0 ||
        argptr(2, (char**)(&Port), sizeof(WORD)) < 0)
    {
        return -1;
    }
    if (f->type != file::FD_SOCKET) {return -2;}
    af = SocketAccept(f, IP, Port);
    if (!af) {return -2;}
    afd = fdalloc(af);
    if (afd < 0)
    {
        fileclose(af);
        return -3;
    }
    return afd;
}

int SOC_SocketRead()
{
    file* f;
    void* Buffer;
    int Size;
    if (argfd(0, 0, &f) < 0 ||
        argptr(1, (char**)(&Buffer), sizeof(BYTE)) < 0 ||
        argint(2, &Size) < 0)
    {
        return -1;
    }
    if (f->type != file::FD_SOCKET) {return -2;}
    return SocketRead(f, Buffer, Size);
}

int SOC_SocketReceiveFrom()
{
    file* f;
    DWORD* Addr;
    WORD* Port;
    void* Buffer;
    int Size;
    if (argfd(0, 0, &f) < 0 ||
        argptr(1, (char**)(&Addr), sizeof(DWORD)) < 0 ||
        argptr(2, (char**)(&Port), sizeof(WORD)) < 0 ||
        argptr(3, (char**)(&Buffer), sizeof(BYTE)) < 0 ||
        argint(4, &Size) < 0)
    {
        return -1;
    }
    if (f->type != file::FD_SOCKET) {return -2;}
    return SocketReceiveFrom(f, Addr, Port, Buffer, Size);
}

int SOC_SocketWrite()
{
    file* f;
    void* Buffer;
    int Size;
    if (argfd(0, 0, &f) < 0 ||
        argptr(1, (char**)(&Buffer), sizeof(BYTE)) < 0 ||
        argint(2, &Size) < 0)
    {
        return -1;
    }
    if (f->type != file::FD_SOCKET) {return -2;}
    return SocketWrite(f, Buffer, Size);
}

int SOC_SocketSendTo()
{
    file* f;
    int Address;
    int Port;
    void* Buffer;
    int Size;
    if (argfd(0, 0, &f) < 0 ||
        argint(1, &Address) < 0 ||
        argint(2, &Port) < 0 ||
        argptr(3, (char**)(&Buffer), sizeof(BYTE)) < 0 ||
        argint(4, &Size) < 0)
    {
        return -1;
    }
    if (f->type != file::FD_SOCKET) {return -2;}
    return SocketSendTo(f, Address, Port, Buffer, Size);
}

_END_EXTERN_C
