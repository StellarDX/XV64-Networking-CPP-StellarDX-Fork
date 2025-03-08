#include "UEtherFrame.hh"
#include "UNetworkAdapter.hh"

_EXTERN_C
#include "kernel/string.h"
_ADD_KERN_PRINT_FUNC
_ADD_KALLOC
_ADD_KFREE
_END_EXTERN_C

QWORD EtherFrameBufferCurrentSize = 0;
EthernetFrame GlobalEtherFrameBuffer[EtherFrameBufferMaxSize];

EthernetFrame::EthernetFrame(LPCVOID Data, int Size)
{
    FrameSize = Size >= MinFrameSize ? Size : MinFrameSize;
    //FrameSize = Size;
    memcopy(this->Data, Data, Size);
}

void EthernetFrame::Resize(int NewSize)
{
    if (FrameSize < NewSize)
    {
        FrameSize = NewSize;
        return;
    }
    EraseData(NewSize, FrameSize - NewSize);
}

void EthernetFrame::Clear()
{
    *this = EthernetFrame();
}

void EthernetFrame::ClearData()
{
    for (int i = Payload; i < MaxFrameSize; ++i)
    {
        Data[i] = 0;
    }
}

void EthernetFrame::CopyTo(EthernetFrame* Dst) const
{
    Dst->FrameSize = this->FrameSize;
    memcopy(Dst->Data, this->Data, this->FrameSize);
}

void EthernetFrame::SetDestination(LPCVOID MACAddr)
{
    for (int i = 0; i < 6; ++i)
    {
        Data[Destination + i] = ((BYTE*)MACAddr)[i];
    }
}

void EthernetFrame::SetSource(LPCVOID MACAddr)
{
    for (int i = 0; i < 6; ++i)
    {
        Data[Source + i] = ((BYTE*)MACAddr)[i];
    }
}

void EthernetFrame::SetEtherType(WORD Type)
{
    union {WORD i; BYTE b[2];} Splitter = {.i = Type};
    Data[this->Type] = Splitter.b[1];
    Data[this->Type + 1] = Splitter.b[0];
}

void EthernetFrame::SetData(LPCVOID Data, int Start, int Size)
{
    int NewSize = HeaderSize + Start + Size;
    FrameSize = NewSize > FrameSize ? NewSize : FrameSize;
    if (FrameSize < MinFrameSize) {FrameSize = MinFrameSize;}
    for (int i = 0; i < Size; ++i)
    {
        this->Data[Payload + Start + i] = ((BYTE*)Data)[i];
    }
}

void EthernetFrame::InsertData(LPCVOID Data, int Start, int Size)
{
    BYTE* Buffer = (BYTE*)kalloc();
    GetData(Buffer, Start, FrameSize - Start);
    SetData(Buffer, Start + Size, FrameSize - Start);
    SetData(Data, Start, Size);
    kfree((char*)Buffer);
}

void EthernetFrame::EraseData(int Start, int Size)
{
    if (Start + Size == FrameSize)
    {
        for (int i = Start; i < Size; ++i)
        {
            this->Data[i] = 0;
        }
        FrameSize -= Size;
        if (FrameSize < MinFrameSize) {FrameSize = MinFrameSize;}
        return;
    }

    BYTE* Buffer = (BYTE*)kalloc();
    GetData(Buffer, Start + Size, FrameSize - Start + Size);
    SetData(Buffer, Start, FrameSize - Start + Size);
    kfree((char*)Buffer);
    for (int i = Start + Size; i < FrameSize; ++i)
    {
        this->Data[i] = 0;
    }
    FrameSize -= Size;
    if (FrameSize < MinFrameSize) {FrameSize = MinFrameSize;}
}

void EthernetFrame::GetDestination(LPVOID MACAddr) const
{
    for (int i = 0; i < 6; ++i)
    {
        ((BYTE*)MACAddr)[i] = Data[Destination + i];
    }
}

void EthernetFrame::GetSource(LPVOID MACAddr) const
{
    for (int i = 0; i < 6; ++i)
    {
        ((BYTE*)MACAddr)[i] = Data[Source + i];
    }
}

WORD EthernetFrame::GetEtherType() const
{
    union {WORD i; BYTE b[2];} Splitter = {.b = {Data[Type + 1], Data[Type]}};
    return Splitter.i;
}

void EthernetFrame::GetData(LPVOID Data, int Start, int Size, BOOL Reverse) const
{
    for (int i = 0; i < Size; ++i)
    {
        int RealPos = Payload + Start + i;
        if (Reverse) {RealPos = Payload + Start + Size - i - 1;}
        ((BYTE*)Data)[i] = this->Data[RealPos];
    }
}

BOOL EthernetFrame::IsBroadcast() const
{
    BYTE FrameDestination[6];
    GetDestination(FrameDestination);
    return !memcmp(BroadcastAddr, FrameDestination, 6);
}

void EthernetFrame::Broadcast()
{
    SetDestination(BroadcastAddr);
}

int EthernetFrame::ToDevice(NetworkAdapter& Device)
{
    SetSource(Device.GetMACAddress());
    return Device.Transmit(*this);
}

void EthernetFrame::Print(const char* Title)const
{
    if (Title) {cprintf((char*)Title);}
    BYTE Source[6], Destination[6];
    GetDestination(Destination);
    GetSource(Source);
    cprintf((char*)" Destination: %x:%x:%x:%x:%x:%x\n",
        Destination[0], Destination[1], Destination[2],
        Destination[3], Destination[4], Destination[5]);
    cprintf((char*)"      Source: %x:%x:%x:%x:%x:%x\n",
        Source[0], Source[1], Source[2], Source[3], Source[4], Source[5]);
    cprintf((char*)"   EtherType: 0x%x\n", GetEtherType());
    cprintf((char*)" Data Length: %d\n", DataSize());
}

void EthernetFrame::PrintHexData()const
{
    int offset, index;
    const BYTE* src = Data;

    auto isascii = [](int c)
    {
        return (c >= 0) && (c <= 127);
    };

    auto isprint = [](int ch)
    {
        return (ch > 0x1F && ch < 0x7F);
    };

    cprintf((char*)"+------+-------------------------------------------------+------------------+\n");
    for (offset = 0; offset < (int)FrameSize; offset += 16)
    {
        cprintf((char*)"| %04x | ", offset);
        for (index = 0; index < 16; index++)
        {
            if (offset + index < (int)FrameSize)
            {
                cprintf((char*)"%02x ", 0xff & src[offset + index]);
            }
            else
            {
                cprintf((char*)"   ");
            }
        }
        cprintf((char*)"| ");
        for (index = 0; index < 16; index++)
        {
            if (offset + index < (int)FrameSize)
            {
                if (isascii(src[offset + index]) && isprint(src[offset + index]))
                {
                    cprintf((char*)"%c", src[offset + index]);
                }
                else
                {
                    cprintf((char*)".");
                }
            }
            else
            {
                cprintf((char*)" ");
            }
        }
        cprintf((char*)" |\n");
    }
    cprintf((char*)"+------+-------------------------------------------------+------------------+\n");
}

// ---------- Static Functions ---------- //

BOOL FrameFilter(NetworkAdapter* Device, EthernetFrame Frame)
{
    if (Frame.IsBroadcast()) {return 1;}
    auto DeviceMACAddress = Device->GetMACAddress();
    BYTE FrameDestination[6];
    Frame.GetDestination(FrameDestination);
    return !memcmp(DeviceMACAddress, FrameDestination, 6);
}

void FrameBufferHandler(NetworkAdapter* Device, EthernetFrame* Buffer, int Size)
{
    for (int i = 0; i < Size; ++i)
    {
        if (!FrameFilter(Device, Buffer[i])) {continue;}
        for (int j = 0; ProtocolInvokers[j].Register && ProtocolInvokers[j].InvokeMain; ++j)
        {
            if (Buffer[i].GetEtherType() == ProtocolInvokers[j].EtherType)
            {
                ProtocolInvokers[j].InvokeMain(Device, Buffer[i]);
            }
        }
    }
}
