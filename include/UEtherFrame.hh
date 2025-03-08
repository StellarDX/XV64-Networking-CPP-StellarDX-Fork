#pragma once

#ifndef UMACADDRESS_H
#define UMACADDRESS_H

#include "UDef.hh"
#include "UNetworkAdapter.hh"

struct ProtocolMainFunctionInvoker
{
    using InitFunctionType = void(*)();
    using MainFuncType = void(*)(NetworkAdapter*, const EthernetFrame&);

    InitFunctionType Register;
    int EtherType;
    MainFuncType InvokeMain;
};

extern const ProtocolMainFunctionInvoker ProtocolInvokers[];

class EthernetFrame
{
public:
    static const auto HeaderSize   = 6 + 6 + 2; // 2 * MAC + type
    static const auto MinDataSize  = 46;
    static const auto MaxDataSize  = 1500;
    static const auto TailSize     = 4; // CRC size
    static const auto MinFrameSize = HeaderSize + MinDataSize + TailSize;
    static const auto MaxFrameSize = HeaderSize + MaxDataSize + TailSize;

    static const auto Destination  = 0;  // 0 - 5
    static const auto Source       = 6;  // 6 - 11
    static const auto Type         = 12; // 12 - 13
    static const auto Payload      = 14; // 14 - (14 + FrameSize - 1)

    constexpr static const BYTE BroadcastAddr[]
        = "\xFF\xFF\xFF\xFF\xFF\xFF";

private:
    BYTE Data[MaxFrameSize];
    int FrameSize = MinFrameSize;

public:
    EthernetFrame() {ctor();}
    EthernetFrame(LPCVOID Data, int Size);

    void ctor()
    {
        ClearData();
        FrameSize = MinDataSize;
    }

    const BYTE* Get()const {return Data;}
    int Size()const {return FrameSize;}
    int DataSize()const {return FrameSize - HeaderSize/* - TailSize*/;}
    void Resize(int NewSize);

    void Clear();
    void ClearData();

    void CopyTo(EthernetFrame* Dst)const;

    void SetDestination(LPCVOID MACAddr);
protected:
    void SetSource(LPCVOID MACAddr);
    void SetEtherType(WORD Type);
    void SetData(LPCVOID Data, int Start, int Size);

    void InsertData(LPCVOID Data, int Start, int Size);
    void EraseData(int Start, int Size);

    template<typename Tp>
    void SetDataFrom(Tp Src, int Start, bool Reverse = 1)
    {
        union {Tp i; BYTE b[sizeof(Tp)];} Splitter;
        Splitter.i = Src;
        if (Reverse)
        {
            for (int i = 0; i < int(sizeof(Tp)) / 2; ++i)
            {
                BYTE Tmp = Splitter.b[i];
                Splitter.b[i] = Splitter.b[sizeof(Tp) - 1 - i];
                Splitter.b[sizeof(Tp) - 1 - i] = Tmp;
            }
        }
        SetData(Splitter.b, Start, sizeof(Tp));
    }

    void FixSize();

public:
    void GetDestination(LPVOID MACAddr)const;
    void GetSource(LPVOID MACAddr)const;
    WORD GetEtherType()const;
    void GetData(LPVOID Data, int Start, int Size, BOOL Reverse = 0)const;

    template<typename Tp>
    Tp GetDataAs(int Start, BOOL Reverse = 1)const
    {
        union {Tp i; BYTE b[sizeof(Tp)];} Splitter;
        GetData(Splitter.b, Start, sizeof(Tp), Reverse);
        return Splitter.i;
    }

    //protected: BYTE& operator[](int Index) {return Data[Index];}
    BYTE operator[](int Index)const {return Data[Index];}

    BOOL IsBroadcast()const;
    void Broadcast();
    int ToDevice(NetworkAdapter& Device);

    void Print(const char* Title)const;
    void PrintHexData()const;
};

static const QWORD EtherFrameBufferMaxSize = 180;
extern QWORD EtherFrameBufferCurrentSize;
extern EthernetFrame GlobalEtherFrameBuffer[EtherFrameBufferMaxSize];

void FrameBufferHandler(NetworkAdapter* Device, EthernetFrame* Buffer, int Size);

#endif // UMACADDRESS_H
