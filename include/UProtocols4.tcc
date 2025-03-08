#ifndef UPROTOCOLS2_TCC
#define UPROTOCOLS2_TCC

#include "UProtocols.hh"
#include "URandom.tcc"
#include "UQueue.tcc"

_EXTERN_C
#include "kernel/string.h"
_ADD_KALLOC
_ADD_KFREE
_ADD_INITLOCK
_ADD_KERN_PRINT_FUNC
_ADD_ACQUIRE
_ADD_RELEASE
void wakeup(void*);
void sleep(void*, struct spinlock*);
_END_EXTERN_C

template<typename Signature = IPv4> requires IPProtocols<Signature>
class __TCPBase : public Signature
{
public:
    using Mybase = Signature;

    static const auto ProtocolNumber        = 6;
    static const auto TCBTableSize          = 16;
    static const auto SourcePortMin         = 49152;
    static const auto SourcePortMax         = 65535;

    static const auto HeaderSizeMin         = 20;
    static const auto HeaderSizeMax         = 60;
    static const auto SourcePort            = 0; // 0 - 1
    static const auto DestinationPort       = 2; // 2 - 3
    static const auto SequenceNumber        = 4; // 4 - 7
    static const auto AcknowledgementNumber = 8; // 8 - 11
    static const auto DataOffset            = 12; // 12 - 12+4bit
    static const auto DataOffsetMask        = 0b11110000;
    static const auto Flags                 = 13;
    static const auto Window                = 14; // 14 - 15
    static const auto Checksum              = 16; // 16 - 17
    static const auto UrgentPointer         = 18; // 18 - 19
    static const auto Options               = 20; // 20 - ???

    enum TCPFlags : BYTE
    {
        FIN = 0b00000001, // Last packet from sender
        SYN = 0b00000010, // Synchronize sequence numbers
        RST = 0b00000100, // Reset the connection
        PSH = 0b00001000, // Push function
        ACK = 0b00010000, // Acknowledgment
        URG = 0b00100000, // Urgent pointer
        ECE = 0b01000000, // ECN-Echo
        CWR = 0b10000000, // Congestion window reduced
    };

    static spinlock TCPLock;

    __TCPBase() : Mybase()
    {
        Mybase::SetProtocol(ProtocolNumber);
        SetDataOffset(HeaderSizeMin / sizeof(DWORD));
    }
    __TCPBase(const Mybase& Frame) : Mybase(Frame) {}

    virtual ~__TCPBase() {}

    int DataSize()const {return Mybase::DataSize() - GetDataOffset() * sizeof(DWORD);}

    // Getters and Setters
    WORD GetSourcePort()const
    {return Mybase::template GetDataAs<WORD>(SourcePort);}
    WORD GetDestinationPort()const
    {return Mybase::template GetDataAs<WORD>(DestinationPort);}
    DWORD GetSequenceNumber()const
    {return Mybase::template GetDataAs<DWORD>(SequenceNumber);}
    DWORD GetAcknowledgementNumber()const
    {return Mybase::template GetDataAs<DWORD>(AcknowledgementNumber);}
    BYTE GetDataOffset()const
    {return Mybase::template GetDataAs<BYTE>(DataOffset) >> 4;}
    BYTE GetFlags()const
    {return Mybase::template GetDataAs<BYTE>(Flags);}
    WORD GetWindow()const
    {return Mybase::template GetDataAs<WORD>(Window);}
    WORD GetChecksum()const
    {return Mybase::template GetDataAs<WORD>(Checksum);}
    WORD GetUrgentPointer()const
    {return Mybase::template GetDataAs<WORD>(UrgentPointer);}

    void GetOptions(LPVOID Dst)const
    {
        int OptionSize = GetDataOffset() * sizeof(DWORD) - HeaderSizeMin;
        Mybase::GetData(Dst, Options, OptionSize);
    }

    void GetData(LPVOID Dst, int Start, int Size, BOOL Reverse = 0)const
    {
        int HeaderSize = GetDataOffset() * sizeof(DWORD);
        Mybase::GetData(Dst, HeaderSize + Start, Size, Reverse);
    }

    template<typename Tp>
    Tp GetDataAs(int Start, BOOL Reverse = 1)const
    {
        union {Tp i; BYTE b[sizeof(Tp)];} Splitter;
        __TCPBase::GetData(Splitter.b, Start, sizeof(Tp), Reverse);
        return Splitter.i;
    }

    void SetSourcePort(WORD NewSourcePort)
    {Mybase::template SetDataFrom<WORD>(NewSourcePort, SourcePort);}
    void SetDestinationPort(WORD NewDestinationPort)
    {Mybase::template SetDataFrom<WORD>(NewDestinationPort, DestinationPort);}
    void SetSequenceNumber(DWORD NewSequenceNumber)
    {Mybase::template SetDataFrom<DWORD>(NewSequenceNumber, SequenceNumber);}
    void SetAcknowledgementNumber(DWORD NewAcknowledgementNumber)
    {Mybase::template SetDataFrom<DWORD>(NewAcknowledgementNumber, AcknowledgementNumber);}
    void SetFlags(BYTE NewFlags)
    {Mybase::template SetDataFrom<BYTE>(NewFlags, Flags);}
    void SetWindow(WORD NewWindow)
    {Mybase::template SetDataFrom<WORD>(NewWindow, Window);}
    void SetUrgentPointer(WORD NewUrgentPointer)
    {Mybase::template SetDataFrom<WORD>(NewUrgentPointer, UrgentPointer);}

    void SetData(LPCVOID Src, int Start, int Size)
    {
        Mybase::SetData(Src, GetDataOffset() * sizeof(DWORD) + Start, Size);
    }

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
        __TCPBase::SetData(Splitter.b, Start, sizeof(Tp));
    }

protected:
    void SetDataOffset(BYTE NewDataOffset)
    {
        BYTE Field = Mybase::template GetDataAs<BYTE>(DataOffset);
        Field &= ~DataOffsetMask;
        Field |= (NewDataOffset << 4) & DataOffsetMask;
        Mybase::template SetDataFrom<BYTE>(Field, DataOffset);
    }

    virtual void SetOptions(LPCVOID Src, int Size) = 0;

    void SetChecksum(WORD NewChecksum)
    {
        Mybase::template SetDataFrom<WORD>(NewChecksum, Checksum);
    }

    virtual WORD VerifyChecksum(BOOL ComputeOnly = 0)const = 0;

public:
    virtual int ToDevice(NetworkAdapter& Device, BOOL HeaderOnly) = 0;

    void Print(LPCSTR Title)const
    {
        if (Title) {cprintf((char*)Title);}
        Mybase::Print("");
        cprintf((char*)"------------------------------\n");
        cprintf((char*)"Source Port: %d\n", GetSourcePort());
        cprintf((char*)"Destination Port: %d\n", GetDestinationPort());
        cprintf((char*)"Sequence: 0x%x\n", GetSequenceNumber());
        cprintf((char*)"Acknowledgement: 0x%x\n", GetAcknowledgementNumber());
        cprintf((char*)"Data Offset: %d (%d)\n", GetDataOffset(), GetDataOffset() * sizeof(DWORD));
        cprintf((char*)"Flags: 0x%x\n", GetFlags());
        cprintf((char*)"Window: %d\n", GetWindow());
        cprintf((char*)"Checksum: 0x%x (Verified: 0x%x)\n", GetChecksum(), VerifyChecksum());
        cprintf((char*)"Urgent Pointer: 0x%x\n", GetUrgentPointer());
        cprintf((char*)"Data: (len=%d)\n", DataSize());
    }
};

template<typename Signature> requires IPProtocols<Signature>
spinlock __TCPBase<Signature>::TCPLock;

template<BYTE Version> class TCP;
template<BYTE Version> class TCB;
template<BYTE Version>
using TCPController = TCB<Version>;
template<BYTE Version>
using TransmissionControlBlock = TCB<Version>;

template<>
class TCP<4> : public __TCPBase<IPv4>
{
public:
    using Mybase = IPv4;

    template<BYTE Version>
    friend class TCB;

    TCP() : __TCPBase<IPv4>() {}
    TCP(const Mybase& Frame) : __TCPBase(Frame) {}

    static ArrayList<class TCB<4>, TCBTableSize> TCBTable;

    // Getters and Setters
protected:
    void SetOptions(LPCVOID Src, int Size)override
    {
        int IPHeaderSize = GetInternetHeaderLength() * sizeof(DWORD);
        int RemoveSize = GetDataOffset() * sizeof(DWORD) - HeaderSizeMin;
        int OptionsPad = IPHeaderSize + Options;
        if (RemoveSize) {Mybase::EraseData(OptionsPad, RemoveSize);}
        if (Size) {Mybase::InsertData(Src, OptionsPad, Size);}
        int FinalHeaderSize = (HeaderSizeMin + Size) / 4;
        if ((HeaderSizeMin + Size) % 4 != 0) {FinalHeaderSize += 1;}
        SetDataOffset(FinalHeaderSize);
    }

private:
    WORD VerifyChecksum(BOOL ComputeOnly = 0)const override
    {
        DWORD PseudoHeader
            = ((GetSourceAddressBE() >> 16) & 0xFFFF)
            + (GetSourceAddressBE() & 0xFFFF)
            + ((GetDestinationAddressBE() >> 16) & 0xFFFF)
            + (GetDestinationAddressBE() & 0xFFFF)
            + (WORD(ProtocolNumber))
            + (WORD(Mybase::DataSize()));

        int TotalSize = Mybase::DataSize();
        if ((TotalSize % 2) == 1) {TotalSize = TotalSize / 2 + 1;}
        else {TotalSize /= 2;}
        DWORD InitAddition = PseudoHeader;
        for (int i = 0; i < TotalSize; ++i)
        {
            if (ComputeOnly && i == Checksum / 2) {continue;}
            InitAddition += Mybase::GetDataAs<WORD>(i * 2);
        }

        while(InitAddition >> 16)
        {
            InitAddition = (InitAddition & 0xFFFF) + (InitAddition >> 16);
        }
        return ~WORD(InitAddition);
    }

public:
    BOOL IsValid()const
    {
        return !VerifyChecksum();
    }

    int ToDevice(NetworkAdapter& Device, BOOL HeaderOnly)override
    {
        auto it = Mybase::IPFind(&Device);
        if (it == Mybase::AdapterIPAddressTable.end())
        {
            cprintf((char*)"[IPv4] No available addresses.\n");
            return -1;
        }

        Mybase::SetSourceAddress(it->IPAddress);
        WORD TCPLength = HeaderOnly ? GetDataOffset() * sizeof(DWORD) : 0;
        if (TCPLength)
        {
            TCPLength += GetInternetHeaderLength() * sizeof(DWORD);
            Mybase::Resize(TCPLength);
        }

        SetChecksum(VerifyChecksum(1));
        /*if (VerifyChecksum())
        {
            cprintf((LPSTR)"[TCP] WARNING: Verifying transmission checksum error (0x%x)\n",
                VerifyChecksum());
        }*/
        return Mybase::ToDevice(Device, TCPLength);
    }

    static void AcquireLock() {acquire(&TCPLock);}
    static void ReleaseLock() {release(&TCPLock);}

    static void Register();
    static void Main(NetworkAdapter* Device, const Mybase& Frame);
};

template<>
class TCP<6> : public __TCPBase<IPv6>
{
    // TODO...
};

/*
    TCP Connection State Diagram
    Reference: RFC 9293 3.3.2 fig.5

                                +---------+ ---------\      active OPEN
                                |  CLOSED |            \    -----------
                                +---------+<---------\   \   create TCB
                                  |     ^              \   \  snd SYN
                     passive OPEN |     |   CLOSE        \   \
                     ------------ |     | ----------       \   \
                      create TCB  |     | delete TCB         \   \
                                  V     |                      \   \
              rcv RST (note 1)  +---------+            CLOSE    |    \
           -------------------->|  LISTEN |          ---------- |     |
          /                     +---------+          delete TCB |     |
         /           rcv SYN      |     |     SEND              |     |
        /           -----------   |     |    -------            |     V
    +--------+      snd SYN,ACK  /       \   snd SYN          +--------+
    |        |<-----------------           ------------------>|        |
    |  SYN   |                    rcv SYN                     |  SYN   |
    |  RCVD  |<-----------------------------------------------|  SENT  |
    |        |                  snd SYN,ACK                   |        |
    |        |------------------           -------------------|        |
    +--------+   rcv ACK of SYN  \       /  rcv SYN,ACK       +--------+
       |         --------------   |     |   -----------
       |                x         |     |     snd ACK
       |                          V     V
       |  CLOSE                 +---------+
       | -------                |  ESTAB  |
       | snd FIN                +---------+
       |                 CLOSE    |     |    rcv FIN
       V                -------   |     |    -------
    +---------+         snd FIN  /       \   snd ACK         +---------+
    |  FIN    |<----------------          ------------------>|  CLOSE  |
    | WAIT-1  |------------------                            |   WAIT  |
    +---------+          rcv FIN  \                          +---------+
      | rcv ACK of FIN   -------   |                          CLOSE  |
      | --------------   snd ACK   |                         ------- |
      V        x                   V                         snd FIN V
    +---------+               +---------+                    +---------+
    |FINWAIT-2|               | CLOSING |                    | LAST-ACK|
    +---------+               +---------+                    +---------+
      |              rcv ACK of FIN |                 rcv ACK of FIN |
      |  rcv FIN     -------------- |    Timeout=2MSL -------------- |
      |  -------            x       V    ------------        x       V
       \ snd ACK              +---------+delete TCB          +---------+
         -------------------->|TIME-WAIT|------------------->| CLOSED  |
                              +---------+                    +---------+
*/

template<BYTE Version>
class TCB // Transmission Control Block
{
public:
    using FrameType = TCP<Version>;
    using IPType    = FrameType::Mybase;

    static const auto DefWindowSize         = 2000;

    //template<BYTE Version>
    friend class TCP<Version>;

    enum TCBStates
    {
        LISTEN       = 1,
        SYN_SENT     = 2,
        SYN_RECEIVED = 3,
        ESTABLISHED  = 4,
        FIN_WAIT_1   = 5,
        FIN_WAIT_2   = 6,
        CLOSE_WAIT   = 9,
        CLOSING      = 7,
        LAST_ACK     = 10,
        TIME_WAIT    = 8,
        CLOSED       = 0,
    };

private:
    TCB* ParentBody = nullptr;
    BOOL Started = 0;
    TCBStates State = CLOSED;
    NetworkAdapter* Iface = nullptr;
    FrameType* Frame = nullptr;
    BYTE* Window = nullptr;

    // RFC 9293 3.3.1
    struct SendSequenceVariables
    {
        DWORD Unacknowledged;
        DWORD Next;
        WORD  Window;
        WORD  UrgentPointer;
        DWORD SequenceNumber; // Used for last window update
        DWORD AcknowledgmentNumber; // Used for last window update
    }SendSequence;
    DWORD InitialSendSequenceNumber;

    struct ReceiveSequenceVariables
    {
        DWORD Next;
        WORD  Window;
        WORD  UrgentPointer;
    }ReceiveSequence;
    DWORD InitialReceiveSequenceNumber;

    struct CurrentSegmentVariables
    {
        DWORD SequenceNumber;
        DWORD AcknowledgmentNumber;
        DWORD Length;
        WORD  Window;
        WORD  UrgentPointer;
    }CurrentSegment;// __declspec(deprecated);

    //LinkedQueue<TCP<Version>> TransmitQueue;
    LinkedQueue<TCB<Version>*> ReceiveQueue;

public:
    TCB() {Init();}
    ~TCB() {Destory();}

    TCB(NetworkAdapter* Iface, const FrameType* Frame) : TCB()
    {
        this->Iface = Iface;
        Frame->CopyTo(this->Frame);
    }

    void Init()
    {
        //Started = 1;
        Frame = new FrameType();
        Window = (BYTE*)kalloc();
    }

    void Destory()
    {
        delete Frame;
        kfree((char*)Window);
    }

    // Getters and Setters
    TCB* GetParentBody()const { return ParentBody; }
    BOOL IsStarted()const {return Started;}
    FrameType* GetFrame() {return Frame;}
    const FrameType* GetFrame()const {return Frame;}
    NetworkAdapter* GetDevice()const {return Iface;}
    TCBStates GetState()const {return State;}

    constexpr static const BYTE SYNOptions[] =
    {
        0x02, 0x04, 0x05, 0xB4, // MSS
        //0x04, 0x02,             // SACK Perm.
        //0x03, 0x03, 0x01        // Window scale
    };
    static const int SYNOptionsSize = 4;

protected:
    void SetParentBody(TCB* NewParentBody) {ParentBody = NewParentBody;}
    void Start() {Started = 1;}
    void SetFrame(const FrameType* Frame)
    {
        this->Frame->Clear();
        Frame->CopyTo(this->Frame);
    }
    void SetDevice(NetworkAdapter* NewDevice) {Iface = NewDevice;}
    void SetState(TCBStates NewState)
    {
        State = NewState;
        cprintf((LPSTR)"[TCB] Switched to new state: %d\n", State);
    }

public:
    BOOL IsReadyForReception()
    {
        return GetState() == ESTABLISHED || GetState() == FIN_WAIT_1 || GetState() == FIN_WAIT_2;
    }

    BOOL IsReadyForTranssmission()
    {
        return GetState() == ESTABLISHED || GetState() == CLOSE_WAIT;
    }

    void Fork(TCB* ParentBody, NetworkAdapter* Device, const TCP<4>* TCPFrame)
    {
        this->Start();
        this->SetState(ParentBody->GetState());
        this->SetDevice(Device);
        this->GetFrame()->SetSourcePort(ParentBody->GetFrame()->GetSourcePort());
        this->GetFrame()->SetDestinationAddress(TCPFrame->GetSourceAddress());
        this->GetFrame()->SetDestinationPort(TCPFrame->GetSourcePort());
        this->ReceiveSequence.Window = DefWindowSize;
        this->SetParentBody(ParentBody);
    }

    void Clear()
    {
        //TransmitQueue.__dist_clear();
        *this = TCB();
        Init();
    }

    void ClearFrame()
    {
        int IPHeaderSize = Frame->GetInternetHeaderLength() * sizeof(DWORD);
        int TCPHeaderSize = Frame->GetDataOffset() * sizeof(DWORD);
        if (Frame->DataSize())
        {
            Frame->EraseData(IPHeaderSize + TCPHeaderSize, Frame->DataSize());
        }
        Frame->SetOptions(nullptr, 0);
    }

    void UpdateRouteData()
    {
        typename decltype(IPType::RouteTable)::iterator Route;
        Route = IPType::RouteTableMatch(Frame->GetDestinationAddress());
        if (Route == IPType::RouteTable.end()) {return;}
        Iface = (NetworkAdapter*)Route->Iface;
    }

    int SendControl(DWORD Sequence, DWORD Acknowledge, BYTE Flags)
    {
        cprintf((LPSTR)"[TCB] Sending control: SEQ - 0x%x, ACK - 0x%x, FLG - 0x%x\n",
            Sequence, Acknowledge, Flags);
        Frame->SetSequenceNumber(Sequence);
        Frame->SetAcknowledgementNumber(Acknowledge);
        Frame->SetFlags(Flags);
        Frame->SetWindow(ReceiveSequence.Window);
        if (Flags & FrameType::SYN) {Frame->SetOptions(SYNOptions, SYNOptionsSize);}
        UpdateRouteData();
        int ReturnValue = Iface ? Frame->ToDevice(*Iface, 1) : -1;
        // if (ReturnValue > 0) {TransmitQueue.push(new FrameType(Frame));}
        if (Flags & FrameType::SYN) {Frame->SetOptions(nullptr, 0);}
        return ReturnValue;
    }

    int SendData(LPCVOID Data, int Size)
    {
        Frame->SetSequenceNumber(SendSequence.Next);
        Frame->SetAcknowledgementNumber(ReceiveSequence.Next);
        Frame->SetFlags(FrameType::ACK | FrameType::PSH);
        Frame->SetData(Data, 0, Size);
        UpdateRouteData();
        Frame->ToDevice(*Iface, 0);
        // if (ReturnValue > 0) {TransmitQueue.push(new FrameType(Frame));}
        ClearFrame();
        return Size;
    }

    // Static functions
    static int Open()
    {
        cprintf((LPSTR)"[TCB] TCB Starting...\n");
        FrameType::AcquireLock();
        for (auto i = 0U; i < FrameType::TCBTable.size(); ++i)
        {
            if (!FrameType::TCBTable[i].IsStarted())
            {
                FrameType::TCBTable[i].Start();
                FrameType::ReleaseLock();
                cprintf((LPSTR)"[TCB] TCB Started at %d\n", i);
                return i;
            }
        }
        FrameType::ReleaseLock();
        return -1;
    }

    static int Close(int Index)
    {
        if (Index > int(FrameType::TCBTable.size())) {return -1;}
        cprintf((LPSTR)"[TCB] TCB %d - Stopping...\n", Index);
        FrameType::AcquireLock();
        auto CurrentApp = &(FrameType::TCBTable[Index]);
        if (!CurrentApp->IsStarted())
        {
            FrameType::ReleaseLock();
            return -2;
        }

        switch (CurrentApp->GetState())
        {
        case SYN_RECEIVED:
        case ESTABLISHED:
            CurrentApp->SendControl(
                CurrentApp->SendSequence.Next,
                CurrentApp->ReceiveSequence.Next,
                FrameType::FIN | FrameType::ACK);
            CurrentApp->SetState(FIN_WAIT_1);
            ++CurrentApp->SendSequence.Next;
            sleep(CurrentApp, &FrameType::TCPLock);
            break;
        case CLOSE_WAIT:
            CurrentApp->SendControl(
                CurrentApp->SendSequence.Next,
                CurrentApp->ReceiveSequence.Next,
                FrameType::FIN | FrameType::ACK);
            CurrentApp->SetState(LAST_ACK);
            ++CurrentApp->SendSequence.Next;
            sleep(CurrentApp, &FrameType::TCPLock);
            break;
        default:
            break;
        }
        CurrentApp->Clear();
        cprintf((LPSTR)"[TCB] TCB %d - Stopped\n", Index);
        FrameType::ReleaseLock();
        return 0;
    }

    static int Bind(int Index, WORD Port)
    {
        if (Index > int(FrameType::TCBTable.size())) {return -1;}
        // Auto-detect
        //if ((IP & IP::LocalhostMask) == IP::Localhost) {return 1;} // Not supported just now

        cprintf((LPSTR)"[TCB] Binding TCB %d to port %d\n", Index, Port);
        auto CurrentApp = &(FrameType::TCBTable[Index]);
        FrameType::AcquireLock();
        for (auto i = 0U; i < FrameType::TCBTable.size(); ++i)
        {
            if (FrameType::TCBTable[i].GetFrame()->GetSourcePort() == Port)
            {
                FrameType::ReleaseLock();
                return -2;
            }
        }
        if (!CurrentApp->IsStarted() || CurrentApp->GetState() != CLOSED)
        {
            FrameType::ReleaseLock();
            return -3;
        }
        CurrentApp->GetFrame()->SetSourcePort(Port);
        cprintf((LPSTR)"[TCB] TCB %d - Current port is %d\n", Index,
            CurrentApp->GetFrame()->GetSourcePort());
        FrameType::ReleaseLock();
        return 0;
    }

    static int Listen(int Index, int Backlog)
    {
        if (Index > int(FrameType::TCBTable.size())) {return -1;}
        FrameType::AcquireLock();

        cprintf((LPSTR)"[TCB] TCB %d - Starting listen... (%d)\n", Index, Backlog);
        auto CurrentApp = &(FrameType::TCBTable[Index]);
        if (!CurrentApp->IsStarted() ||
            CurrentApp->GetState() != CLOSED ||
            !CurrentApp->GetFrame()->GetSourcePort())
        {
            FrameType::ReleaseLock();
            return -3;
        }
        CurrentApp->SetState(LISTEN);

        FrameType::ReleaseLock();
        return 0;
    }

    static int Connect(int Index, DWORD DestinationAddress, WORD DestinationPort)
    {
        // TODO... Only for client
        return 0;
    }

    static int Accept(int Index, DWORD* DestinationAddress, WORD* DestinationPort)
    {
        if (Index > int(FrameType::TCBTable.size())) {return -1;}
        FrameType::AcquireLock();

        cprintf((LPSTR)"[TCB] TCB %d - Waiting for accept...\n", Index);
        auto CurrentApp = &(FrameType::TCBTable[Index]);
        if (!CurrentApp->IsStarted() || CurrentApp->GetState() != LISTEN)
        {
            FrameType::ReleaseLock();
            return -3;
        }

        TCB<4>* CurrentLog;
        while (1)
        {
            if (!CurrentApp->ReceiveQueue.empty())
            {
                CurrentLog = CurrentApp->ReceiveQueue.front();
                CurrentApp->ReceiveQueue.pop();
                break;
            }
            else {sleep(CurrentApp, &FrameType::TCPLock);}
        }

        if (DestinationAddress)
        {
            *DestinationAddress = CurrentLog->GetFrame()->GetDestinationAddress();
        }
        if (DestinationPort)
        {
            *DestinationPort = CurrentLog->GetFrame()->GetDestinationPort();
        }
        int IP1, IP2, Ip3, IP4;
        IP::IPSplit(*DestinationAddress, IP1, IP2, Ip3, IP4);
        cprintf((LPSTR)"[TCB] TCB %d - %d.%d.%d.%d:%d connected\n",
            Index, IP1, IP2, Ip3, IP4, *DestinationPort);

        FrameType::ReleaseLock();
        return CurrentLog - FrameType::TCBTable.data();
    }

    static int Receive(int Index, LPVOID Destination, int Size)
    {
        if (Index > int(FrameType::TCBTable.size())) {return -1;}
        FrameType::AcquireLock();

        auto CurrentApp = &(FrameType::TCBTable[Index]);
        int TotalSize;
        while (!(TotalSize = DefWindowSize - CurrentApp->ReceiveSequence.Window))
        {
            if (!CurrentApp->IsReadyForReception())
            {
                FrameType::ReleaseLock();
                return 0;
            }
            sleep(CurrentApp, &FrameType::TCPLock);
        }

        int TrueDataSize = Size < TotalSize ? Size : TotalSize;
        memcopy(Destination, CurrentApp->Window, TrueDataSize);
        memcopy(CurrentApp->Window, CurrentApp->Window + TrueDataSize, TotalSize - TrueDataSize);
        CurrentApp->ReceiveSequence.Window += TrueDataSize;

        FrameType::ReleaseLock();
        return TrueDataSize;
    }

    static int Transmit(int Index, LPCVOID Data, int Size)
    {
        if (Index > int(FrameType::TCBTable.size())) {return -1;}
        FrameType::AcquireLock();

        auto CurrentApp = &(FrameType::TCBTable[Index]);
        if (!CurrentApp->IsStarted() || !CurrentApp->IsReadyForTranssmission())
        {
            FrameType::ReleaseLock();
            return -3;
        }

        CurrentApp->SendData(Data, Size);
        CurrentApp->SendSequence.Next += Size;

        FrameType::ReleaseLock();
        return 0;
    }

    // Event functions
    void DoClosed(const FrameType* TCPFrame)
    {
        cprintf((LPSTR)"[TCB] Current state: CLOSED\n");
        if (TCPFrame->GetFlags() & FrameType::RST){return;}
        DWORD Sequence, Acknowledge;
        if (TCPFrame->GetFlags() & FrameType::ACK)
        {
            Sequence = TCPFrame->GetAcknowledgementNumber();
            Acknowledge = 0;
        }
        else
        {
            Sequence = 0;
            Acknowledge = TCPFrame->GetSequenceNumber();
            if (TCPFrame->GetFlags() & FrameType::SYN)
            {
                ++Acknowledge;
            }
            if (TCPFrame->DataSize())
            {
                Acknowledge += TCPFrame->DataSize();
            }
            if (TCPFrame->GetFlags() & FrameType::FIN)
            {
                ++Acknowledge;
            }
        }
        SendControl(Sequence, Acknowledge, FrameType::RST);
    }

    void DoListen(const FrameType* TCPFrame)
    {
        cprintf((LPSTR)"[TCB] Current state: LISTEN\n");
        if (TCPFrame->GetFlags() & FrameType::RST){return;}
        DWORD Sequence, Acknowledge;
        if (TCPFrame->GetFlags() & FrameType::ACK)
        {
            Sequence = TCPFrame->GetAcknowledgementNumber();
            Acknowledge = 0;
            SendControl(Sequence, Acknowledge, FrameType::RST);
            return;
        }
        if (TCPFrame->GetFlags() & FrameType::SYN)
        {
            ReceiveSequence.Next = TCPFrame->GetSequenceNumber() + 1;
            InitialReceiveSequenceNumber = TCPFrame->GetSequenceNumber();
            mt19937l* Engine = new mt19937l(time(nullptr));
            InitialSendSequenceNumber = Engine->Gen();
            delete Engine;
            Sequence = InitialSendSequenceNumber;
            Acknowledge = ReceiveSequence.Next;
            SendControl(Sequence, Acknowledge, FrameType::SYN | FrameType::ACK);
            SendSequence.Next = InitialSendSequenceNumber + 1;
            SendSequence.Unacknowledged = InitialSendSequenceNumber;
            SetState(SYN_RECEIVED);
        }
    }

    void DoSynSent(const FrameType* TCPFrame)
    {
        cprintf((LPSTR)"[TCB] Current state: SYN-SENT\n");
        DWORD Sequence, Acknowledge;
        if (TCPFrame->GetFlags() & FrameType::ACK)
        {
            if (TCPFrame->GetAcknowledgementNumber() <= InitialSendSequenceNumber ||
                TCPFrame->GetAcknowledgementNumber() > SendSequence.Next)
            {
                if (!(TCPFrame->GetFlags() & FrameType::RST))
                {
                    Sequence = TCPFrame->GetAcknowledgementNumber();
                    Acknowledge = 0;
                    SendControl(Sequence, Acknowledge, FrameType::RST);
                }
                return;
            }
        }
        if (TCPFrame->GetFlags() & FrameType::RST)
        {
            if (TCPFrame->GetFlags() & FrameType::ACK)
            {
                // TCB Close
            }
            return;
        }
        if (TCPFrame->GetFlags() & FrameType::SYN)
        {
            ReceiveSequence.Next = TCPFrame->GetSequenceNumber() + 1;
            InitialReceiveSequenceNumber = TCPFrame->GetSequenceNumber();
            if (TCPFrame->GetFlags() & FrameType::ACK)
            {
                SendSequence.Unacknowledged = TCPFrame->GetAcknowledgementNumber();
                if (SendSequence.Unacknowledged > InitialSendSequenceNumber)
                {
                    SetState(ESTABLISHED);
                    Sequence = SendSequence.Next;
                    Acknowledge = ReceiveSequence.Next;
                    SendControl(Sequence, Acknowledge, FrameType::ACK);
                    wakeup(this);
                }
                return;
            }
            Sequence = InitialSendSequenceNumber;
            Acknowledge = ReceiveSequence.Next;
            SendControl(Sequence, Acknowledge, FrameType::ACK);
        }
    }

    void DoSynReceived(const FrameType* TCPFrame)
    {
        cprintf((LPSTR)"[TCB] Current state: SYN-RSVD\n");
        if (SendSequence.Unacknowledged <= TCPFrame->GetAcknowledgementNumber() &&
            TCPFrame->GetAcknowledgementNumber() <= SendSequence.Next)
        {
            SetState(ESTABLISHED);
            ParentBody->ReceiveQueue.push(this);
            wakeup(ParentBody);
        }
        else
        {
            SendControl(TCPFrame->GetAcknowledgementNumber(), 0, FrameType::RST);
        }
    }

    void DoFinWait1(const FrameType* TCPFrame)
    {
        cprintf((LPSTR)"[TCB] Current state: FIN-WAIT-1\n");
        if (TCPFrame->GetAcknowledgementNumber() == SendSequence.Next)
        {
            State = FIN_WAIT_2;
        }
    }

    int DoClosing(const FrameType* TCPFrame)
    {
        cprintf((LPSTR)"[TCB] Current state: %d\n", GetState());
        if (SendSequence.Unacknowledged < TCPFrame->GetAcknowledgementNumber() &&
            TCPFrame->GetAcknowledgementNumber() <= SendSequence.Next)
        {
            SendSequence.Unacknowledged = TCPFrame->GetAcknowledgementNumber();
        }
        else if (TCPFrame->GetAcknowledgementNumber() > SendSequence.Next)
        {
            SendControl(SendSequence.Next, ReceiveSequence.Next, FrameType::ACK);
            return 1;
        }

        if (State == FIN_WAIT_1) {DoFinWait1(TCPFrame);}
        else if (State == CLOSING)
        {
            if (TCPFrame->GetAcknowledgementNumber() == SendSequence.Next)
            {
                State = TIME_WAIT;
                wakeup(this);
            }
            return 1;
        }
        return 0;
    }

    void DoLastAck(const FrameType* TCPFrame)
    {
        cprintf((LPSTR)"[TCB] Current state: LAST-ACK\n");
        wakeup(this);
        Clear();
    }

    void StoreData(const FrameType* TCPFrame)
    {
        cprintf((LPSTR)"[TCB] Storing data...\n");
        DWORD Sequence, Acknowledge;
        TCPFrame->GetData(Window + (DefWindowSize - ReceiveSequence.Window),
            0, TCPFrame->DataSize());
        ReceiveSequence.Next = TCPFrame->GetSequenceNumber() + TCPFrame->DataSize();
        ReceiveSequence.Window -= TCPFrame->DataSize();
        Sequence = SendSequence.Next;
        Acknowledge = ReceiveSequence.Next;
        SendControl(Sequence, Acknowledge, FrameType::ACK);
        wakeup(this);
    }

    void Terminate(const FrameType* TCPFrame)
    {
        ++ReceiveSequence.Next;
        SendControl(SendSequence.Next, ReceiveSequence.Next, FrameType::ACK);
        switch (State)
        {
        case SYN_RECEIVED:
        case ESTABLISHED:
            SetState(CLOSE_WAIT);
            wakeup(this);
            break;
        case FIN_WAIT_1:
            SetState(FIN_WAIT_2);
            break;
        case FIN_WAIT_2:
            SetState(TIME_WAIT);
            wakeup(this);
            break;
        default:
            break;
        }
    }

    // Main Function
    void Main(const FrameType* TCPFrame)
    {
        switch (State)
        {
        case LISTEN:
            DoListen(TCPFrame);
            return;
        case SYN_SENT:
            DoSynSent(TCPFrame);
            return;
        case CLOSED:
            DoClosed(TCPFrame);
            return;
        default:
            break;
        }

        if (TCPFrame->GetSequenceNumber() != ReceiveSequence.Next)
        {
            cprintf((LPSTR)"[TCB] Sequence number check failed.\n");
            return;
        }
        if (TCPFrame->GetFlags() & (FrameType::RST | FrameType::SYN))
        {
            cprintf((LPSTR)"[TCB] Reconnecting...\n");
            return;
        }
        if (!(TCPFrame->GetFlags() & (FrameType::ACK)))
        {
            cprintf((LPSTR)"[TCB] Not ACK.\n");
            return;
        }

        int ReturnValue;
        switch (State)
        {
        case SYN_RECEIVED:
            DoSynReceived(TCPFrame);
        case ESTABLISHED:
        case FIN_WAIT_1:
        case FIN_WAIT_2:
        case CLOSE_WAIT:
        case CLOSING:
            ReturnValue = DoClosing(TCPFrame);
            if (ReturnValue) {return;}
            break;
        case LAST_ACK:
            DoLastAck(TCPFrame);
            return;
        default:
            break;
        }

        if ((TCPFrame->GetFlags() & FrameType::PSH) && TCPFrame->DataSize())
        {
            switch (State)
            {
            case ESTABLISHED:
            case FIN_WAIT_1:
            case FIN_WAIT_2:
                StoreData(TCPFrame);
                break;
            default:
                break;
            }
        }

        if (TCPFrame->GetFlags() & (FrameType::FIN))
        {
            Terminate(TCPFrame);
            return;
        }
    }
};

inline void TCP<4>::Register()
{
    cprintf((LPSTR)"[TCP] Registering...\n");
    TCBTable.resize(TCBTable.max_size());
    for (auto i = 0U; i < TCBTable.size(); ++i)
    {
        TCBTable[i].Init();
    }
    initlock(&TCPLock, (char*)"TCP");
    cprintf((LPSTR)"[TCP] DONE.\n");
}

inline void TCP<4>::Main(NetworkAdapter* Device, const Mybase& Frame)
{
    TCP<4>* TCPFrame = new TCP<4>(Frame);
    //TCPFrame->Print("TCP Received.\n");
    if (!TCPFrame->IsValid())
    {
        //cprintf((char*)"[TCP] Invalid TCP frame. (0x%x)\n",
        cprintf((char*)"[TCP] WARNING: TCP Checksum incorrect (0x%x), "
            R"(maybe caused by "TCP checksum offload.")" "\n",
            TCPFrame->VerifyChecksum());
        //return;
    }

    cprintf((LPSTR)"[TCB] Received: SEQ - 0x%x, ACK - 0x%x, FLG - 0x%x\n",
        TCPFrame->GetSequenceNumber(),
        TCPFrame->GetAcknowledgementNumber(),
        TCPFrame->GetFlags());

    decltype(TCBTable)::iterator App, AppF = nullptr, AppL = nullptr;
    AcquireLock();
    for (App = TCBTable.begin(); App != TCBTable.end(); ++App)
    {
        if (!App->IsStarted())
        {
            if (!AppF)
            {
                //cprintf((LPSTR)"[TCB] Allocate new block.\n");
                AppF = App;
            }
        }
        else if ((!App->GetDevice() || App->GetDevice() == Device)
            && App->GetFrame()->GetSourcePort() == TCPFrame->GetDestinationPort())
        {
            if (App->GetFrame()->GetDestinationAddress() == TCPFrame->GetSourceAddress() &&
                App->GetFrame()->GetDestinationPort() == TCPFrame->GetSourcePort())
            {
                //cprintf((LPSTR)"[TCB] Found specified block.\n");
                break;
            }
            if ((App->GetState() == App->LISTEN) && !AppL)
            {
                //cprintf((LPSTR)"[TCB] Found listening.\n");
                AppL = App;
            }
        }
    }

    if (App == TCBTable.end())
    {
        if (!AppF || !AppL || !(TCPFrame->GetFlags() & SYN))
        {
            cprintf((LPSTR)"[TCB] Unexpected state, sendinng RST...\n");
            // Send RST, TODO...
            ReleaseLock();
            return;
        }

        App = AppF;
        App->Fork(AppL, Device, TCPFrame);
    }

    App->Main(TCPFrame);
    ReleaseLock();
    delete TCPFrame;
}



template<typename Signature = IPv4> requires IPProtocols<Signature>
class __UDPBase : public Signature
{
public:
    using Mybase = Signature;

    static const auto ProtocolNumber        = 17;
    static const auto UDPTableSize          = 16;

    static const auto HeaderSize            = 8;
    static const auto SourcePort            = 0; // 0 - 1
    static const auto DestinationPort       = 2; // 2 - 3
    static const auto Length                = 4; // 4 - 5
    static const auto Checksum              = 6; // 6 - 7

    static spinlock UDPLock;

    __UDPBase() : Mybase()
    {
        Mybase::SetProtocol(ProtocolNumber);
        SetLength(HeaderSize);
    }
    __UDPBase(const Mybase& Frame) : Mybase(Frame) {}

    virtual ~__UDPBase() {}

    int DataSize()const {return GetLength() - HeaderSize;}

    // Getters and Setters
    WORD GetSourcePort()const
    {return Mybase::template GetDataAs<WORD>(SourcePort);}
    WORD GetDestinationPort()const
    {return Mybase::template GetDataAs<WORD>(DestinationPort);}
    WORD GetLength()const
    {return Mybase::template GetDataAs<WORD>(Length);}
    WORD GetChecksum()const
    {return Mybase::template GetDataAs<WORD>(Checksum);}

    void GetData(LPVOID Dst, int Start, int Size, BOOL Reverse = 0)const
    {
        Mybase::GetData(Dst, HeaderSize + Start, Size, Reverse);
    }

    template<typename Tp>
    Tp GetDataAs(int Start, BOOL Reverse = 1)const
    {
        union {Tp i; BYTE b[sizeof(Tp)];} Splitter;
        __UDPBase::GetData(Splitter.b, Start, sizeof(Tp), Reverse);
        return Splitter.i;
    }

    void SetSourcePort(WORD NewSourcePort)
    {Mybase::template SetDataFrom<WORD>(NewSourcePort, SourcePort);}
    void SetDestinationPort(WORD NewDestinationPort)
    {Mybase::template SetDataFrom<WORD>(NewDestinationPort, DestinationPort);}

    void SetData(LPCVOID Src, int Start, int Size)
    {
        Mybase::SetData(Src, HeaderSize + Start, Size);
    }

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
        __UDPBase::SetData(Splitter.b, Start, sizeof(Tp));
    }

protected:
    void SetLength(WORD NewLength)
    {
        Mybase::template SetDataFrom<WORD>(NewLength, Length);
    }

    void SetChecksum(WORD NewChecksum)
    {
        Mybase::template SetDataFrom<WORD>(NewChecksum, Checksum);
    }

    virtual WORD VerifyChecksum(BOOL ComputeOnly = 0)const = 0;

public:
    virtual int ToDevice(NetworkAdapter& Device, BOOL HeaderOnly) = 0;

    void Print(LPCSTR Title)const
    {
        if (Title) {cprintf((char*)Title);}
        Mybase::Print("");
        cprintf((char*)"------------------------------\n");
        cprintf((char*)"Source Port: %d\n", GetSourcePort());
        cprintf((char*)"Destination Port: %d\n", GetDestinationPort());
        cprintf((char*)"Length: 0x%x\n", GetLength());
        cprintf((char*)"Checksum: 0x%x (Verified: 0x%x)\n", GetChecksum(), VerifyChecksum());
    }
};

template<typename Signature> requires IPProtocols<Signature>
spinlock __UDPBase<Signature>::UDPLock;

template<BYTE Version> class UDP;
template<BYTE Version> class UDPController;

template<>
class UDP<4> : public __UDPBase<IPv4>
{
public:
    using Mybase = IPv4;

    template<BYTE Version>
    friend class UDPController;

    UDP() : __UDPBase<IPv4>() {}
    UDP(const Mybase& Frame) : __UDPBase(Frame) {}

    static ArrayList<class UDPController<4>, UDPTableSize> UDPTable;

private:
    WORD VerifyChecksum(BOOL ComputeOnly = 0)const override
    {
        DWORD PseudoHeader
            = ((GetSourceAddressBE() >> 16) & 0xFFFF)
              + (GetSourceAddressBE() & 0xFFFF)
              + ((GetDestinationAddressBE() >> 16) & 0xFFFF)
              + (GetDestinationAddressBE() & 0xFFFF)
              + (WORD(ProtocolNumber))
              + (WORD(Mybase::DataSize()));

        int TotalSize = Mybase::DataSize();
        if ((TotalSize % 2) == 1) {TotalSize = TotalSize / 2 + 1;}
        else {TotalSize /= 2;}
        DWORD InitAddition = PseudoHeader;
        for (int i = 0; i < TotalSize; ++i)
        {
            if (ComputeOnly && i == Checksum / 2) {continue;}
            InitAddition += Mybase::GetDataAs<WORD>(i * 2);
        }

        while(InitAddition >> 16)
        {
            InitAddition = (InitAddition & 0xFFFF) + (InitAddition >> 16);
        }
        return ~WORD(InitAddition);
    }

public:
    BOOL IsValid()const
    {
        return !VerifyChecksum();
    }

    int ToDevice(NetworkAdapter& Device, BOOL HeaderOnly = 0)override
    {
        auto it = Mybase::IPFind(&Device);
        if (it == Mybase::AdapterIPAddressTable.end())
        {
            cprintf((char*)"[IPv4] No available addresses.\n");
            return -1;
        }

        Mybase::SetSourceAddress(it->IPAddress);
        SetChecksum(VerifyChecksum(1));
        /*if (VerifyChecksum())
        {
            cprintf((LPSTR)"[TCP] WARNING: Verifying transmission checksum error (0x%x)\n",
                VerifyChecksum());
        }*/
        return Mybase::ToDevice(Device, 0);
    }

    static void AcquireLock() {acquire(&UDPLock);}
    static void ReleaseLock() {release(&UDPLock);}

    static void Register();
    static void Main(NetworkAdapter* Device, const Mybase& Frame);
};

template<BYTE Version>
class UDPController
{
public:
    using FrameType = UDP<Version>;
    using IPType    = FrameType::Mybase;

    friend class UDP<Version>;

private:
    BOOL Started = 0;
    NetworkAdapter* Iface = nullptr;
    FrameType* Frame = nullptr;
    LinkedQueue<FrameType*> ReceiveQueue;

public:
    UDPController() {Init();}
    ~UDPController() {Destory();}

    // Getters and Setters
    void Start() {Started = 1;}
    BOOL IsStarted()const {return Started;}
    FrameType* GetFrame() {return Frame;}
    const FrameType* GetFrame()const {return Frame;}
    NetworkAdapter* GetDevice()const {return Iface;}

    void Init()
    {
        Frame = new FrameType();
    }

    void Destory()
    {
        while (!ReceiveQueue.empty())
        {
            delete ReceiveQueue.front();
            ReceiveQueue.pop();
        }
        delete Frame;
    }

    void Clear()
    {
        *this = UDPController();
        Init();
    }

    void ClearFrame()
    {
        int IPHeaderSize = Frame->GetInternetHeaderLength() * sizeof(DWORD);
        int UDPHeaderSize = FrameType::HeaderSize;
        if (Frame->DataSize())
        {
            Frame->EraseData(IPHeaderSize + UDPHeaderSize, Frame->DataSize());
        }
    }

    void UpdateRouteData()
    {
        typename decltype(IPType::RouteTable)::iterator Route;
        Route = IPType::RouteTableMatch(Frame->GetDestinationAddress());
        if (Route == IPType::RouteTable.end()) {return;}
        Iface = (NetworkAdapter*)Route->Iface;
    }

    int SendData(DWORD Destination, WORD DestiPort, LPCVOID Data, int Size)
    {
        if (!Destination || !DestiPort) {return -1;}
        Frame->SetDestinationAddress(Destination);
        Frame->SetDestinationPort(DestiPort);
        Frame->SetData(Data, 0, Size);
        if (!Iface) {UpdateRouteData();}
        Frame->ToDevice(*Iface);
        ClearFrame();
        return Size;
    }

    // Static Functions
    static int Open()
    {
        cprintf((LPSTR)"[UDP Controller] Starting Controller...\n");
        FrameType::AcquireLock();
        for (auto i = 0U; i < FrameType::UDPTable.size(); ++i)
        {
            if (!FrameType::UDPTable[i].IsStarted())
            {
                FrameType::UDPTable[i].Start();
                FrameType::ReleaseLock();
                cprintf((LPSTR)"[UDP Controller] Controller Started at %d\n", i);
                return i;
            }
        }
        FrameType::ReleaseLock();
        return -1;
    }

    static int Close(int Index)
    {
        if (Index > int(FrameType::UDPTable.size())) {return -1;}

        cprintf((LPSTR)"[UDP Controller] Controller %d - Closing...\n", Index);
        FrameType::AcquireLock();
        auto Block = &(FrameType::UDPTable[Index]);
        if (!Block->IsStarted())
        {
            FrameType::ReleaseLock();
            return -2;
        }
        Block->Clear();
        FrameType::ReleaseLock();
        cprintf((LPSTR)"[UDP Controller] Controller %d - Closed.\n", Index);
        return 0;
    }

    static int Bind(int Index, DWORD Address, WORD Port)
    {
        int Address1, Address2, Address3, Address4;
        IP::IPSplit(Address, Address1, Address2, Address3, Address4);
        cprintf((LPSTR)"[UDP Controller] Binding Controller %d to %d.%d.%d.%d:%d\n",
            Index, Address1, Address2, Address3, Address4, Port);
        if (Index > int(FrameType::UDPTable.size())) {return -1;}
        if ((Address & IP::LocalhostMask) == IP::Localhost)
        {
            return -2;
        }
        FrameType::AcquireLock();
        auto Block = &(FrameType::UDPTable[Index]);
        if (!Block->IsStarted())
        {
            FrameType::ReleaseLock();
            return -3;
        }
        Block->GetFrame()->SetSourceAddress(Address);
        auto IPAddr = IP::IPFind(Address);
        if (Address)
        {
            if (IPAddr == IP::AdapterIPAddressTable.end())
            {
                FrameType::ReleaseLock();
                return -4;
            }
        }
        for (int i = 0; i < int(FrameType::UDPTable.size()); ++i)
        {
            auto Blk = &(FrameType::UDPTable[i]);
            if (Blk->IsStarted() && (i != Index) &&
                (!Blk->Iface || Blk->Iface == IPAddr->Adapter) &&
                Port == Blk->GetFrame()->GetSourcePort())
            {
                FrameType::ReleaseLock();
                return -5;
            }
        }
        Block->Iface = Address ? (NetworkAdapter*)IPAddr->Adapter : nullptr;
        Block->GetFrame()->SetSourcePort(Port);
        FrameType::ReleaseLock();
        cprintf((LPSTR)"[UDP Controller] Controller %d - Current port is %d\n", Index,
            Block->GetFrame()->GetSourcePort());
        return 0;
    }

    static int Bind(int Index, NetworkAdapter* Adapter, WORD Port)
    {
        if (Index > int(FrameType::UDPTable.size())) {return -1;}
        FrameType::AcquireLock();
        auto Block = &(FrameType::UDPTable[Index]);
        if (!Block->IsStarted())
        {
            FrameType::ReleaseLock();
            return -2;
        }
        for (int i = 0; i < FrameType::UDPTable.size(); ++i)
        {
            auto Blk = &(FrameType::UDPTable[i]);
            if (Blk->IsStarted() && (i != Index) &&
                (!Blk->Iface || Blk->Iface == Adapter) &&
                Port == Blk->GetFrame()->GetSourcePort())
            {
                FrameType::ReleaseLock();
                return -3;
            }
        }
        Block->Iface = Adapter;
        Block->GetFrame()->SetSourcePort(Port);
        FrameType::ReleaseLock();
        return 0;
    }

    static int Receive(int Index, DWORD* DestiAddress, WORD* DestiPort, LPVOID Destination, int Size)
    {
        if (Index > int(FrameType::UDPTable.size())) {return -1;}
        FrameType::AcquireLock();
        auto Block = &(FrameType::UDPTable[Index]);
        if (!Block->IsStarted())
        {
            FrameType::ReleaseLock();
            return -2;
        }
        FrameType* Frame;
        while (1)
        {
            if (!Block->ReceiveQueue.empty())
            {
                Frame = Block->ReceiveQueue.front();
                Block->ReceiveQueue.pop();
                break;
            }
            else {sleep(Block, &FrameType::UDPLock);}
        }
        FrameType::ReleaseLock();
        if (DestiAddress && DestiPort)
        {
            *DestiAddress = Frame->GetSourceAddress();
            *DestiPort = Frame->GetSourcePort();
        }
        int TrueSize = Size < Frame->DataSize() ? Size : Frame->DataSize();
        Frame->GetData(Destination, 0, TrueSize);
        delete Frame;
        return TrueSize;
    }

    static int Transmit(int Index, DWORD DestiAddress, WORD DestiPort, LPCVOID Destination, int Size)
    {
        if (Index > int(FrameType::UDPTable.size())) {return -1;}
        FrameType::AcquireLock();
        auto Block = &(FrameType::UDPTable[Index]);
        Block->SendData(DestiAddress, DestiPort, Destination, Size);
        FrameType::ReleaseLock();
        return Size;
    }
};

inline void UDP<4>::Register()
{
    cprintf((LPSTR)"[UDP] Registering...\n");
    UDPTable.resize(UDPTable.max_size());
    for (auto i = 0U; i < UDPTable.size(); ++i)
    {
        UDPTable[i].Init();
    }
    initlock(&UDPLock, (char*)"UDP");
    cprintf((LPSTR)"[UDP] DONE.\n");
}

inline void UDP<4>::Main(NetworkAdapter* Device, const Mybase& Frame)
{
    UDP<4>* UDPFrame = new UDP<4>(Frame);
    if (!UDPFrame->IsValid())
    {
        //cprintf((char*)"[UDP] Invalid TCP frame. (0x%x)\n",
        cprintf((char*)"[UDP] WARNING: UDP Checksum incorrect (0x%x)\n",
            UDPFrame->VerifyChecksum());
        //return;
    }

    cprintf((LPSTR)"[UDP] Frame Received.\n");
    AcquireLock();
    for (auto Block = UDPTable.begin(); Block != UDPTable.end(); ++Block)
    {
        if (Block->IsStarted() && (!Block->GetDevice() || Block->GetDevice() == Device)
            && Block->GetFrame()->GetSourcePort() == UDPFrame->GetDestinationPort())
        {
            //cprintf((LPSTR)"[UDP] Found specified block.\n");
            Block->ReceiveQueue.push(UDPFrame);
            wakeup(Block);
            ReleaseLock();
            return;
        }
    }

    ReleaseLock();
    // Send network unreachable. (ICMP)
}

#endif // UPROTOCOLS2_TCC
