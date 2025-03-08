#include "UProtocols.hh"
#include "UProtocols4.tcc"
#include "UNetworkAdapter.hh"
#include "URandom.tcc"

_EXTERN_C
_ADD_KERN_PRINT_FUNC
_ADD_INITLOCK
_ADD_KALLOC
_ADD_KFREE
_ADD_DELAY
_END_EXTERN_C

const ProtocolMainFunctionInvoker ProtocolInvokers[] =
{
    {.Register = IPv4::Register, .EtherType = IPv4::EtherType, .InvokeMain = IPv4::Main},
    {.Register = ARP::Register, .EtherType = ARP::EtherType, .InvokeMain = ARP::Main},
    {.Register = nullptr, .EtherType = 0, .InvokeMain = nullptr} // End iterator
};

// -------------------------- ARP Protocol -------------------------- //

time_t ARP::ARPTimestamp;
spinlock ARP::ARPLock;
ArrayList<ARP::ARPTableItem, ARP::ARPTableSize> ARP::ARPTable;

WORD ARP::GetHardwareType() const
{
    return GetDataAs<WORD>(HardwareType);
}

WORD ARP::GetProtocolType() const
{
    return GetDataAs<WORD>(ProtocolType);
}

BYTE ARP::GetHardwareLength() const
{
    return GetDataAs<BYTE>(HardwareLength);
}

BYTE ARP::GetProtocolLength() const
{
    return GetDataAs<BYTE>(ProtocolLength);
}

WORD ARP::GetOperation() const
{
    return GetDataAs<WORD>(Operation);
}

void ARP::GetSenderHardwareAddress(BYTE* MACAddrDst) const
{
    GetData(MACAddrDst, SenderHardwareAddr, 6);
}

DWORD ARP::GetSenderProtocolAddress() const
{
    return GetDataAs<DWORD>(SenderProtocolAddr, 0);
}

void ARP::GetTargetHardwareAddress(BYTE* MACAddrDst) const
{
    GetData(MACAddrDst, TargetHardwareAddr, 6);
}

DWORD ARP::GetTargetProtocolAddress() const
{
    return GetDataAs<DWORD>(TargetProtocolAddr, 0);
}

void ARP::SetHardwareType(WORD NewHardwareType)
{
    SetDataFrom<WORD>(NewHardwareType, HardwareType);
}

void ARP::SetProtocolType(WORD NewProtocolType)
{
    SetDataFrom<WORD>(NewProtocolType, ProtocolType);
}

void ARP::SetHardwareLength(BYTE NewHardwareLength)
{
    SetDataFrom<BYTE>(NewHardwareLength, HardwareLength);
}

void ARP::SetProtocolLength(BYTE NewProtocolLength)
{
    SetDataFrom<BYTE>(NewProtocolLength, ProtocolLength);
}

void ARP::SetOperation(WORD NewOperation)
{
    SetDataFrom<WORD>(NewOperation, Operation);
}

void ARP::SetSenderHardwareAddress(const BYTE* MACAddr)
{
    SetData(MACAddr, SenderHardwareAddr, 6);
}

void ARP::SetSenderProtocolAddress(DWORD SenderProtocolAddress)
{
    SetDataFrom<DWORD>(SenderProtocolAddress, SenderProtocolAddr, 0);
}

void ARP::SetTargetHardwareAddress(const BYTE* MACAddr)
{
    SetData(MACAddr, TargetHardwareAddr, 6);
}

void ARP::SetTargetProtocolAddress(DWORD TargetProtocolAddress)
{
    SetDataFrom<DWORD>(TargetProtocolAddress, TargetProtocolAddr, 0);
}

BOOL ARP::IsValid() const
{
    return (DataSize() > 28) && (GetHardwareType() == 1) &&
        (GetProtocolType() == IPv4::EtherType) && (GetHardwareLength() == 6) &&
        (GetProtocolLength() == 4);
}

void ARP::Prepare(Operations Op)
{
    SetEtherType(EtherType);
    SetHardwareType(1);
    SetProtocolType(IPv4::EtherType);
    SetHardwareLength(6);
    SetProtocolLength(4);
    SetOperation(Op); // 1 for request, 2 for reply
}

void ARP::AcquireLock()
{
    acquire(&ARPLock);
}

void ARP::ReleaseLock()
{
    release(&ARPLock);
}

void ARP::ARPTableAdd(ARPTableItem NewItem)
{
    ARPTable.push_back(NewItem);
}

void ARP::ARPTableRemove(DWORD IP)
{
    auto it = ARPTableFind(IP);
    if (it == ARPTable.end()) {return;}
    ARPTable.erase(it);
}

void ARP::ARPTableUpdate(DWORD IP, ARPTableItem NewItem)
{
    auto it = ARPTableFind(IP);
    if (it == ARPTable.end()) {return;}
    *it = NewItem;
}

decltype(ARP::ARPTable)::iterator ARP::ARPTableFind(DWORD IP)
{
    return find_if(ARPTable.begin(), ARPTable.end(),
        [IP](decltype(ARP::ARPTable)::value_type v)
    {
        return v.Address == IP;
    });
}

BOOL ARP::Tester::ARPingIsTesting = 0;
BOOL ARP::Tester::ARPingIsReceived = 0;

int ARP::Tester::ARPing(DWORD TargetIPAddress)
{
    int TargetIP[4];
    IPv4::IPSplit(TargetIPAddress, TargetIP[0], TargetIP[1], TargetIP[2], TargetIP[3]);
    cprintf((char*)"ARPING %d.%d.%d.%d\n",
        TargetIP[0], TargetIP[1], TargetIP[2], TargetIP[3]);

    // Init
    ARPingIsTesting = 1;
    ARPingIsReceived = 0;
    int ARPingSent = 0;
    int ARPingReceived = 0;
    ReturnValue ReturnVal = Passed;

    ARP* ARPFrame = new ARP();
    int Delay = 0;
    int Count = 0;
    int UnansweredCount = 0;

    // Find IP
    decltype(IPv4::RouteTable)::iterator Route;
    decltype(IPv4::AdapterIPAddressTable)::iterator SenderAddress;
    NetworkAdapter* Device;
    Route = IPv4::RouteTableMatch(TargetIPAddress);
    if (Route == IPv4::RouteTable.end()) {goto EndTesting;}
    Device = Route->Iface;
    SenderAddress = IPv4::IPFind(Device);
    if (SenderAddress == IPv4::AdapterIPAddressTable.end()) {goto EndTesting;}

    // Prepare
    ARPFrame->Prepare(ARP::Request);
    ARPFrame->SetSenderHardwareAddress(Device->GetMACAddress());
    ARPFrame->SetSenderProtocolAddress(SenderAddress->IPAddress);
    ARPFrame->SetTargetProtocolAddress(TargetIPAddress);
    ARPFrame->Broadcast();
    //ARPFrame.Print(">>> ARP Send Request <<<\n");

    for (Count = 0; Count < 1; ++Count)
    {
        // Send
        ARPFrame->ToDevice(*Device);
        ++ARPingSent;

        // Receive
        while(/*!ARPingIsReceived &&*/ MaxDelay > Delay) // check for multiple receptions
        {
            microdelay(10);
            ++Delay;
        }
        if (!ARPingIsReceived)
        {
            cprintf((char*)"Timeout\n");
            ReturnVal = Timeout;
            ++UnansweredCount;
            continue;
        }
        ++ARPingReceived;
        ARPingIsReceived = 0;
    }

    EndTesting:
    delete ARPFrame;

    if (ReturnVal == IPNotFound)
    {
        cprintf ((char*)"Local machine has no available IP address.\n");
        return ReturnVal;
    }

    int UnAns = (UnansweredCount * 100) / (Count);
    cprintf((char*)"\n--- %d.%d.%d.%d statistics ---\n",
        TargetIP[0], TargetIP[1], TargetIP[2], TargetIP[3]);
    cprintf((char*)"%d packets transmitted, %d packets received, %d% unanswered\n",
        ARPingSent, ARPingReceived, UnAns > 100 ? 100 : UnAns);

    ARPingIsTesting = 0;
    ARPingIsReceived = 0;
    return ReturnVal;
}

void ARP::Print(const char* Title) const
{
    if (Title) {cprintf((char*)Title);}
    Mybase::Print("");
    cprintf((char*)"------------------------------\n");
    cprintf((char*)" hrd: 0x%x\n", GetHardwareType());
    cprintf((char*)" pro: 0x%x\n", GetProtocolType());
    cprintf((char*)" hln: %d\n", GetHardwareLength());
    cprintf((char*)" pln: %d\n", GetProtocolLength());
    cprintf((char*)"  op: %d\n", GetOperation());
    BYTE SHA[6], THA[6];
    int SPA[4], TPA[4];
    GetSenderHardwareAddress(SHA);
    GetTargetHardwareAddress(THA);
    IPv4::IPSplit(GetSenderProtocolAddress(), SPA[0], SPA[1], SPA[2], SPA[3]);
    IPv4::IPSplit(GetTargetProtocolAddress(), TPA[0], TPA[1], TPA[2], TPA[3]);
    cprintf((char*)" sha: %02x:%02x:%02x:%02x:%02x:%02x\n", SHA[0], SHA[1], SHA[2], SHA[3], SHA[4], SHA[5]);
    cprintf((char*)" spa: %d.%d.%d.%d\n", SPA[0], SPA[1], SPA[2], SPA[3]);
    cprintf((char*)" tha: %02x:%02x:%02x:%02x:%02x:%02x\n", THA[0], THA[1], THA[2], THA[3], THA[4], THA[5]);
    cprintf((char*)" tpa: %d.%d.%d.%d\n", TPA[0], TPA[1], TPA[2], TPA[3]);
    cprintf((char*)"\n");
}

BYTE* ARP::RequestFrom(NetworkAdapter& Device, DWORD IP)
{
    auto SenderAddress = IPv4::IPFind(&Device);
    if (SenderAddress == IPv4::AdapterIPAddressTable.end()) {return nullptr;}

    auto it = ARPTableFind(IP);
    if (it == ARPTable.end())
    {
        ARP* ARPFrame = new ARP();
        ARPFrame->Prepare(Request);
        ARPFrame->SetSenderHardwareAddress(Device.GetMACAddress());
        ARPFrame->SetSenderProtocolAddress(SenderAddress->IPAddress);
        ARPFrame->SetTargetProtocolAddress(IP);
        ARPFrame->Broadcast();
        ARPFrame->ToDevice(Device);
        delete ARPFrame;

        auto Delay = Tester::MaxDelay;
        while (Delay)
        {
            it = ARPTableFind(IP);
            if (it != ARPTable.end()) {break;}
            --Delay;
        }
        if (it == ARPTable.end()) {return nullptr;}
    }
    return it->MACAddress;
}

void ARP::Register()
{
    cprintf((char*)"[ARP] Registering...\n");
    time(&ARPTimestamp);
    cprintf((char*)"[ARP] Timestamp = %d\n", ARPTimestamp);
    initlock(&ARPLock, (char*)"ARP");
    cprintf((char*)"[ARP] DONE.\n");
}

void ARP::Main(NetworkAdapter* Device, const EthernetFrame& Frame)
{
    const ARP& ARPFrame = Frame;
    //ARPFrame.Print("ARP received: \n");
    if (!ARPFrame.IsValid())
    {
        cprintf((char*)"[ARP] Invalid ARP frame.");
        return;
    }

    // Testing
    if (Tester::ARPingIsTesting && ARPFrame.GetOperation() == ARP::Reply)
    {
        int SenderIP[4];
        IPv4::IPSplit(ARPFrame.GetSenderProtocolAddress(),
            SenderIP[0], SenderIP[1], SenderIP[2], SenderIP[3]);
        BYTE MACAddr[6];
        ARPFrame.GetSenderHardwareAddress(MACAddr);
        cprintf((char*)"%d bytes from %02x:%02x:%02x:%02x:%02x:%02x (%d.%d.%d.%d)\n",
                ARPFrame.Size(),
                MACAddr[0], MACAddr[1], MACAddr[2],
                MACAddr[3], MACAddr[4], MACAddr[5],
                SenderIP[0], SenderIP[1], SenderIP[2], SenderIP[3]);
        Tester::ARPingIsReceived = 1;
    }

    // Check and update if exist
    AcquireLock();
    auto ExistTable = ARPTableFind(ARPFrame.GetSenderProtocolAddress());
    if (ExistTable != ARPTable.end())
    {
        ARPFrame.GetSenderHardwareAddress(ExistTable->MACAddress);
    }
    ReleaseLock();

    // ARP packet handle
    auto AdapterIP = IPv4::IPFind(Device);
    if (AdapterIP != IPv4::AdapterIPAddressTable.end() &&
        AdapterIP->IPAddress == ARPFrame.GetTargetProtocolAddress())
    {
        if (ExistTable == ARPTable.end()) // Add sender's addresses
        {
            AcquireLock();
            ARPTableItem Item;
            Item.Address = ARPFrame.GetSenderProtocolAddress();
            Item.HWType = ARPFrame.GetHardwareType();
            Item.Flags = ARPTableItem::COM;
            ARPFrame.GetSenderHardwareAddress(Item.MACAddress);
            Item.Adapter = Device;
            ARPTable.push_back(Item);
            ReleaseLock();
        }

        if (ARPFrame.GetOperation() == ARP::Request) // If request, send a reply.
        {
            ARP* Rep = new ARP();
            Rep->Prepare(ARP::Reply);
            Rep->SetSenderHardwareAddress(Device->GetMACAddress());
            Rep->SetSenderProtocolAddress(AdapterIP->IPAddress);
            BYTE TargetMACAddress[6];
            ARPFrame.GetSenderHardwareAddress(TargetMACAddress);
            Rep->SetTargetHardwareAddress(TargetMACAddress);
            DWORD TargetIPAddress = ARPFrame.GetSenderProtocolAddress();
            Rep->SetTargetProtocolAddress(TargetIPAddress);
            Rep->SetDestination(TargetMACAddress);
            //Rep->Print("ARP send reply: \n");
            Rep->ToDevice(*Device);
            delete Rep;
        }
    }
}

// ------------------------------------------------------------------ //

// --------------------------- IP Protocol -------------------------- //

// ------------------------------ IPv4 ------------------------------ //

spinlock IPv4::IPLock;
ArrayList<IPv4::AdapterIPAddressItem, IPv4::IPTableSize> IPv4::AdapterIPAddressTable;
ArrayList<IPv4::RouteTableItem, IPv4::RouteTableSize> IPv4::RouteTable;
IPv4::IPv4Protocol IPv4::Protocols[ProtocolTableSize];

BYTE IPv4::GetVersion() const
{
    return (Mybase::GetDataAs<BYTE>(Version) & VersionMask) >> 4;
}

BYTE IPv4::GetInternetHeaderLength() const
{
    return (Mybase::GetDataAs<BYTE>(InternetHeaderLength) & IHLMask);
}

BYTE IPv4::GetDifferentiatedServicesCodePoint() const
{
    return (Mybase::GetDataAs<BYTE>(DifferentiatedServicesCodePoint) & DSCPMask) >> 2;
}

BYTE IPv4::GetExplicitCongestionNotification() const
{
    return (Mybase::GetDataAs<BYTE>(ExplicitCongestionNotification) & ECNMask);
}

WORD IPv4::GetTotalLength() const
{
    return Mybase::GetDataAs<WORD>(TotalLength);
}

WORD IPv4::GetIdentification() const
{
    return Mybase::GetDataAs<WORD>(Identification);
}

BYTE IPv4::GetFlags() const
{
    return BYTE((Mybase::GetDataAs<WORD>(Flags) & FlagsMask) >> 13);
}

WORD IPv4::GetFragmentOffset() const
{
    return (Mybase::GetDataAs<WORD>(FragmentOffset) & FragmentOffsetMask);
}

BYTE IPv4::GetTimeToLive() const
{
    return Mybase::GetDataAs<BYTE>(TimeToLive);
}

BYTE IPv4::GetProtocol() const
{
    return Mybase::GetDataAs<BYTE>(Protocol);
}

WORD IPv4::GetHeaderChecksum() const
{
    return Mybase::GetDataAs<WORD>(HeaderChecksum);
}

DWORD IPv4::GetSourceAddress() const
{
    return Mybase::GetDataAs<DWORD>(SourceAddress, 0);
}

DWORD IPv4::GetSourceAddressBE() const
{
    return Mybase::GetDataAs<DWORD>(SourceAddress);
}

DWORD IPv4::GetDestinationAddress() const
{
    return Mybase::GetDataAs<DWORD>(DestinationAddress, 0);
}

DWORD IPv4::GetDestinationAddressBE() const
{
    return Mybase::GetDataAs<DWORD>(DestinationAddress);
}

void IPv4::GetOptions(LPVOID Dst) const
{
    int OptionSize = GetInternetHeaderLength() * sizeof(DWORD) - HeaderSizeMin;
    Mybase::GetData(Dst, Options, OptionSize);
}

void IPv4::GetData(LPVOID Dst, int Start, int Size, BOOL Reverse) const
{
    int HeaderSize = GetInternetHeaderLength() * sizeof(DWORD);
    Mybase::GetData(Dst, HeaderSize + Start, Size, Reverse);
}

void IPv4::SetVersion(BYTE NewVersion)
{
    BYTE Field = GetDataAs<BYTE>(Version);
    NewVersion <<= 4;
    NewVersion &= VersionMask;
    Field &= ~VersionMask;
    Field |= NewVersion;
    Mybase::SetDataFrom<BYTE>(Field, Version);
}

void IPv4::SetInternetHeaderLength(BYTE NewIHL)
{
    BYTE Field = GetDataAs<BYTE>(InternetHeaderLength);
    NewIHL &= IHLMask;
    Field &= ~IHLMask;
    Field |= NewIHL;
    Mybase::SetDataFrom<BYTE>(Field, InternetHeaderLength);
}

void IPv4::SetDifferentiatedServicesCodePoint(BYTE NewDSCP)
{
    BYTE Field = GetDataAs<BYTE>(DifferentiatedServicesCodePoint);
    NewDSCP <<= 2;
    NewDSCP &= DSCPMask;
    Field &= ~DSCPMask;
    Field |= NewDSCP;
    Mybase::SetDataFrom<BYTE>(Field, Version);
}

void IPv4::SetExplicitCongestionNotification(BYTE NewECN)
{
    BYTE Field = GetDataAs<BYTE>(ExplicitCongestionNotification);
    NewECN &= ECNMask;
    Field &= ~ECNMask;
    Field |= NewECN;
    Mybase::SetDataFrom<BYTE>(Field, Version);
}

void IPv4::SetTotalLength(WORD NewTotalLength)
{
    Mybase::SetDataFrom<WORD>(NewTotalLength, TotalLength);
}

void IPv4::SetIdentification(WORD NewIdentification)
{
    Mybase::SetDataFrom<WORD>(NewIdentification, Identification);
}

void IPv4::SetFlags(WORD NewFlags)
{
    WORD Field = GetDataAs<WORD>(Flags);
    NewFlags <<= 13;
    NewFlags &= FlagsMask;
    Field &= ~FlagsMask;
    Field |= NewFlags;
    Mybase::SetDataFrom<WORD>(Field, Flags);
}

void IPv4::SetFragmentOffset(WORD NewFragmentOffset)
{
    WORD Field = GetDataAs<WORD>(FragmentOffset);
    NewFragmentOffset &= FragmentOffsetMask;
    Field &= ~FragmentOffsetMask;
    Field |= NewFragmentOffset;
    Mybase::SetDataFrom<WORD>(Field, FragmentOffset);
}

void IPv4::SetTimeToLive(BYTE NewTimeToLive)
{
    Mybase::SetDataFrom<BYTE>(NewTimeToLive, TimeToLive);
}

void IPv4::SetProtocol(BYTE NewProtocol)
{
    Mybase::SetDataFrom<BYTE>(NewProtocol, Protocol);
}

void IPv4::SetHeaderChecksum(WORD NewHeaderChecksum)
{
    Mybase::SetDataFrom<WORD>(NewHeaderChecksum, HeaderChecksum);
}

void IPv4::SetSourceAddress(DWORD NewSourceAddress)
{
    Mybase::SetDataFrom<DWORD>(NewSourceAddress, SourceAddress, 0);
}

void IPv4::SetDestinationAddress(DWORD NewDestinationAddress)
{
    Mybase::SetDataFrom<DWORD>(NewDestinationAddress, DestinationAddress, 0);
}

void IPv4::SetOptions(LPCVOID Src, int Size)
{
    int RemoveSize = GetInternetHeaderLength() * 4 - HeaderSizeMin;
    if (RemoveSize) {Mybase::EraseData(Options, RemoveSize);}
    Mybase::InsertData(Src, Options, Size);
    int FinalHeaderSize = (HeaderSizeMin + Size) / 4;
    if ((HeaderSizeMin + Size) % 4 != 0) {FinalHeaderSize += 1;}
    SetInternetHeaderLength(FinalHeaderSize);
}

WORD IPv4::VerifyChecksum(BOOL ComputeOnly)const
{
    DWORD InitAddition = 0;
    for (int i = 0; i < HeaderSizeMin / 2; ++i)
    {
        if (ComputeOnly && i == HeaderChecksum / 2) {continue;}
        InitAddition += Mybase::GetDataAs<WORD>(i * 2);
    }
    while(InitAddition >> 16)
    {
        InitAddition = (InitAddition & 0xFFFF) + (InitAddition >> 16);
    }
    return ~InitAddition;
}

BOOL IPv4::IsValid() const
{
    DWORD DataLength = Mybase::DataSize();
    auto IHL = GetInternetHeaderLength() * sizeof(DWORD);
    auto TotalLen = GetTotalLength();
    return (DataLength >= 20) && !VerifyChecksum() &&
        (GetVersion() == 4) && (DataLength >= IHL) &&
        (DataLength >= TotalLen) && (GetTimeToLive());
}

void IPv4::Resize(WORD NewSize)
{
    Mybase::Resize(Mybase::HeaderSize + NewSize);
    SetTotalLength(NewSize);
}

void IPv4::SetData(LPCVOID Src, int Start, int Size)
{
    Mybase::SetData(Src, GetInternetHeaderLength() * sizeof(DWORD) + Start, Size);
    SetTotalLength(Mybase::DataSize());
}

void IPv4::RouteTableAdd(RouteTableItem NewItem)
{
    RouteTable.push_back(NewItem);
}

decltype(IP::RouteTable)::iterator IPv4::RouteTableMatch(DWORD Target)
{
    ArrayList<decltype(IP::RouteTable)::iterator, RouteTableSize> MatchTable;
    auto First = RouteTable.begin();
    auto Last = RouteTable.end();
    for (; First != Last; ++First)
    {
        First = find_if(First, Last, [Target](decltype(RouteTable)::value_type v)
        {
            return (v.Destination & v.Genmask) == (Target & v.Genmask);
        });
        if (First != Last) {MatchTable.push_back(First);}
    }
    if (MatchTable.size() == 0) {return RouteTable.end();} // No matching routes
    if (MatchTable.size() == 1) {return MatchTable[0];} // Single result

    // If 2 or more results, select longest mask and smallest metric.
    qsort(MatchTable.begin(), MatchTable.end(),
        [](decltype(MatchTable)::value_type v1, decltype(MatchTable)::value_type v2)
    {
        return (MaskToNum(v1->Genmask) >= MaskToNum(v2->Genmask))
            && (v1->Metric < v2->Metric);
    });
    return *MatchTable.begin();
}

decltype(IPv4::AdapterIPAddressTable)::iterator IPv4::IPFind(const NetworkAdapter* Adapter)
{
    return find_if(AdapterIPAddressTable.begin(), AdapterIPAddressTable.end(),
        [Adapter](decltype(AdapterIPAddressTable)::value_type v)
    {
        return Adapter == v.Adapter;
    });
}

decltype(IPv4::AdapterIPAddressTable)::iterator IPv4::IPFind(DWORD Addr)
{
    return find_if(AdapterIPAddressTable.begin(), AdapterIPAddressTable.end(),
        [Addr](decltype(AdapterIPAddressTable)::value_type v)
    {
        return Addr == v.IPAddress;
    });
}

void IPv4::IPAllocate(const NetworkAdapter* Adapter, DWORD Addr, DWORD Mask)
{
    AdapterIPAddressTable.push_back(
    {
        .Adapter = Adapter,
        .IPAddress = Addr,
        .SubnetMask = Mask,
    });
}

void IPv4::IPDelete(const NetworkAdapter* Adapter)
{
    auto it = IP::IPFind(Adapter);
    if (it == IP::AdapterIPAddressTable.end())
    {
        cprintf((char*)"[IPv4] Cannot find device\n");
        return;
    }
    AdapterIPAddressTable.erase(it);
}

void IPv4::IPSplit(DWORD Addr, int& P1, int& P2, int& P3, int& P4)
{
    union {DWORD a; BYTE i[4];} Splitter;
    Splitter.a = Addr;
    P1 = Splitter.i[0];
    P2 = Splitter.i[1];
    P3 = Splitter.i[2];
    P4 = Splitter.i[3];
}

int IPv4::MaskToNum(DWORD Mask)
{
    if (!Mask) {return 32;}
    int N = 0;
    while(Mask)
    {
        Mask >>= 1;
        ++N;
    }
    return N;
}

DWORD IPv4::GetNetAddress(DWORD Addr, DWORD Mask)
{
    return Addr & Mask;
}

DWORD IPv4::GetBroadcastAddress(DWORD Addr, DWORD Mask)
{
    return Addr | (~Mask);
}

int IPv4::ToDevice(NetworkAdapter& Device, WORD ResizeTo)
{
    auto it = IPFind(&Device);
    if (it == AdapterIPAddressTable.end())
    {
        cprintf((char*)"[IPv4] No available addresses.\n");
        return -1;
    }

    BYTE* DstMACAddr = ARP::RequestFrom(Device, GetDestinationAddress());
    if (!DstMACAddr)
    {
        cprintf((char*)"[IPv4] Failed to find MAC address of destination.\n");
        return -2;
    }

    SetSourceAddress(it->IPAddress);
    SetDestination(DstMACAddr);

    if (!GetIdentification())
    {
        mt19937l* Engine = new mt19937l(time(nullptr));
        SetIdentification(Engine->Gen() & ~(WORD(0)));
        delete Engine;
    }
    else
    {
        SetIdentification(GetIdentification() + 1);
    }

    SetTimeToLive(128);

    if (ResizeTo)
    {
        Resize((ResizeTo < GetInternetHeaderLength() * sizeof(DWORD)) ?
            GetInternetHeaderLength() * sizeof(DWORD) : ResizeTo);
    }
    SetHeaderChecksum(VerifyChecksum(1));
    return Mybase::ToDevice(Device);
}

void IPv4::Print(const char* Title) const
{
    if (Title) {cprintf((char*)Title);}
    Mybase::Print("");
    cprintf((char*)"------------------------------\n");
    cprintf((char*)"  Version: %d\n", GetVersion());
    cprintf((char*)"      IHL: %d (%d)\n", GetInternetHeaderLength(), GetInternetHeaderLength() * sizeof(DWORD));
    cprintf((char*)"     DSCP: %x\n", GetDifferentiatedServicesCodePoint());
    cprintf((char*)"      ECN: %x\n", GetExplicitCongestionNotification());
    cprintf((char*)"      LEN: %d\n", GetTotalLength());
    cprintf((char*)"       ID: %d\n", GetIdentification());
    cprintf((char*)"   Offset: [Flags=%x, Offset=%d]\n", GetFlags(), GetFragmentOffset());
    cprintf((char*)"      TTL: %d\n", GetTimeToLive());
    cprintf((char*)" Protocol: %d\n", GetProtocol());
    cprintf((char*)" Checksum: 0x%x (Verified: 0x%x)\n", GetHeaderChecksum(), VerifyChecksum());
    int SRC[4], DST[4];
    IPSplit(GetSourceAddress(), SRC[0], SRC[1], SRC[2], SRC[3]);
    IPSplit(GetDestinationAddress(), DST[0], DST[1], DST[2], DST[3]);
    cprintf((char*)"      Src: %d.%d.%d.%d\n", SRC[0], SRC[1], SRC[2], SRC[3]);
    cprintf((char*)"      Dst: %d.%d.%d.%d\n", DST[0], DST[1], DST[2], DST[3]);
    cprintf((char*)"     Data: (len=%d)\n", DataSize());
}

void IPv4::Register()
{
    cprintf((char*)"[IP] Registering...\n");

    Protocols[P_ICMP] = {.Register = ICMP::Register, .InvokeMain = ICMP::Main};
    Protocols[P_TCP] = {.Register = TCP<4>::Register, .InvokeMain = TCP<4>::Main};
    Protocols[P_UDP] = {.Register = UDP<4>::Register, .InvokeMain = UDP<4>::Main};

    initlock(&IPLock, (char*)"IP");
    for (int i = 0; i < ProtocolTableSize; ++i)
    {
        if (Protocols[i].Register) {Protocols[i].Register();}
    }
    cprintf((char*)"[IP] DONE.\n");
}

void IPv4::Main(NetworkAdapter* Device, const EthernetFrame& Frame)
{
    const IPv4& IPv4Frame = Frame;
    //IPv4Frame.Print("IP received: \n");
    if (!IPv4Frame.IsValid())
    {
        cprintf((char*)"[IPv4] Invalid IP frame.\n");
        return;
    }

    // Filter
    auto DeviceIP = IPFind(Device);
    if (DeviceIP == AdapterIPAddressTable.end() ||
        DeviceIP->IPAddress != IPv4Frame.GetDestinationAddress())
    {
        return;
    }

    DWORD ProtocolNumber = IPv4Frame.GetProtocol();
    if (Protocols[ProtocolNumber].InvokeMain)
    {
        Protocols[ProtocolNumber].InvokeMain(Device, IPv4Frame);
    }
}

// ------------------------------------------------------------------ //

// ------------------------------ ICMP ------------------------------ //

BYTE ICMPv4::GetType() const
{
    return Mybase::GetDataAs<BYTE>(Type);
}

BYTE ICMPv4::GetCode() const
{
    return Mybase::GetDataAs<BYTE>(Code);
}

WORD ICMPv4::GetChecksum() const
{
    return Mybase::GetDataAs<WORD>(Checksum);
}

void ICMPv4::GetData(LPVOID Dst, int Start, int Size, BOOL Reverse) const
{
    Mybase::GetData(Dst, RestOfHeader + Start, Size, Reverse);
}

void ICMPv4::SetType(BYTE NewType)
{
    Mybase::SetDataFrom<BYTE>(NewType, Type);
}

void ICMPv4::SetCode(BYTE NewCode)
{
    Mybase::SetDataFrom<BYTE>(NewCode, Code);
}

void ICMPv4::SetChecksum(WORD NewChecksum)
{
    Mybase::SetDataFrom<WORD>(NewChecksum, Checksum);
}

WORD ICMPv4::VerifyChecksum(BOOL ComputeOnly) const
{
    int TotalSize = Mybase::DataSize();
    if ((TotalSize % 2) == 1) {TotalSize = TotalSize / 2 + 1;}
    else {TotalSize /= 2;}
    DWORD InitAddition = 0;
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

BOOL ICMPv4::IsValid() const
{
    return !VerifyChecksum();
}

void ICMPv4::SetData(LPCVOID Src, int Start, int Size)
{
    Mybase::SetData(Src, RestOfHeader + Start, Size);
}

namespace ICMPControllers
{
    BOOL PingEcho::IsTesting = 0;
    BOOL PingEcho::IsReceived = 0;
    WORD PingEcho::CurrentID = 0;
    WORD PingEcho::CurrentSN = 0;

    WORD PingEcho::GetIdentifier() const
    {
        return Mybase::GetDataAs<WORD>(Identifier);
    }

    void PingEcho::SetIdentifier(WORD NewIdentifier)
    {
        Mybase::SetDataFrom<WORD>(NewIdentifier, Identifier);
    }

    WORD PingEcho::GetSequenceNumber() const
    {
        return Mybase::GetDataAs<WORD>(SequenceNumber);
    }

    void PingEcho::SetSequenceNumber(WORD NewSequenceNumber)
    {
        Mybase::SetDataFrom<WORD>(NewSequenceNumber, SequenceNumber);
    }

    /*uint128_t PingEcho::GetTimestamp() const
    {
        return Mybase::GetDataAs<uint128_t>(Timestamp);
    }

    void PingEcho::SetTimestamp(uint128_t NewTimestamp)
    {
        Mybase::SetDataFrom<uint128_t>(NewTimestamp, Timestamp);
    }*/

    void PingEcho::GetData(LPVOID Dst, int Start, int Size) const
    {
        Mybase::GetData(Dst, Datagram + Start, Size);
    }

    void PingEcho::SetData(LPCVOID Src, int Start, int Size)
    {
        Mybase::SetData(Src, Datagram + Start, Size);
    }

    void PingEcho::Ping(DWORD TargetIPAddress)
    {
        int TargetIP[4];
        IPv4::IPSplit(TargetIPAddress, TargetIP[0], TargetIP[1], TargetIP[2], TargetIP[3]);
        /*cprintf((char*)"PING %d.%d.%d.%d\n",
            TargetIP[0], TargetIP[1], TargetIP[2], TargetIP[3]);*/

        // Init
        IsTesting = 1;
        IsReceived = 0;
        int PingSent = 0;
        int PingReceived = 0;
        int UnansweredCount = 0;

        PingEcho* Frame = new PingEcho();
        mt19937l* Engine = new mt19937l(time(nullptr));

        // Find IP
        decltype(IPv4::RouteTable)::iterator Route;
        decltype(IPv4::AdapterIPAddressTable)::iterator SenderAddress;
        NetworkAdapter* Device;
        Route = IPv4::RouteTableMatch(TargetIPAddress);
        if (Route == IPv4::RouteTable.end()) {goto EndTesting;}
        Device = (NetworkAdapter*)Route->Iface;
        SenderAddress = IPv4::IPFind(Device);
        if (SenderAddress == IPv4::AdapterIPAddressTable.end()) {goto EndTesting;}

        // Prepare send request
        CurrentID = Engine->Gen() & ~(WORD(0));
        Frame->SetType(TEchoRequest);
        Frame->SetIdentifier(CurrentID);
        Frame->SetData(DefaultDataUNIX, 0, DefaultDataUNIXSize);

        Frame->SetDestinationAddress(TargetIPAddress);

        cprintf((char*)"Sending 5, %d-byte ICMP Echoes to %d.%d.%d.%d\n",
            Frame->Size(), TargetIP[0], TargetIP[1], TargetIP[2], TargetIP[3]);
        for (int i = 0; i < 5; ++i)
        {
            int Delay = 0;
            CurrentSN = i + 1;
            Frame->SetSequenceNumber(CurrentSN);

            // Send
            Frame->ToDevice(*Device);
            ++PingSent;

            // Receive
            while(!IsReceived && MaxDelay > Delay) // check for multiple receptions
            {
                microdelay(10);
                ++Delay;
            }
            if (IsReceived)
            {
                cprintf((char*)"!");
                ++PingReceived;
                IsReceived = 0;
            }
            else
            {
                cprintf((char*)"*");
                ++UnansweredCount;
            }
        }

        cprintf((char*)"\n");

        EndTesting:
        delete Frame;
        delete Engine;

        /*int UnAns = (UnansweredCount * 100) / (Count);
        cprintf((char*)"\n--- %d.%d.%d.%d statistics ---\n",
                TargetIP[0], TargetIP[1], TargetIP[2], TargetIP[3]);
        cprintf((char*)"%d packets transmitted, %d packets received, %d% unanswered\n",
                PingSent, PingReceived, UnAns > 100 ? 100 : UnAns);*/
        cprintf((char*)"Success rate is %d percent (%d/%d)\n",
                (PingReceived * 100) / (PingSent), PingReceived, PingSent);

        IsTesting = 0;
        IsReceived = 0;
    }

    void SourceQuench::GetIPData(LPVOID Dst, int Start, int Size) const
    {
        Mybase::GetData(Dst, IPDatagram + Start, Size);
    }

    void SourceQuench::SetIPData(LPVOID Src, int Start, int Size)
    {
        Mybase::SetData(Src, IPDatagram + Start, Size);
    }

    DWORD Redirect::GetIPAddress() const
    {
        return Mybase::GetDataAs<DWORD>(IPAddress);
    }

    void Redirect::SetIPAddress(DWORD NewIPAddress)
    {
        Mybase::SetDataFrom<DWORD>(NewIPAddress, IPAddress);
    }

    void Redirect::GetIPData(LPVOID Dst, int Start, int Size) const
    {
        Mybase::GetData(Dst, IPDatagram + Start, Size);
    }

    void Redirect::SetIPData(LPVOID Src, int Start, int Size)
    {
        Mybase::SetData(Src, IPDatagram + Start, Size);
    }

    void TimeExceeded::GetIPData(LPVOID Dst, int Start, int Size) const
    {
        Mybase::GetData(Dst, IPDatagram + Start, Size);
    }

    void TimeExceeded::SetIPData(LPVOID Src, int Start, int Size)
    {
        Mybase::SetData(Src, IPDatagram + Start, Size);
    }

    WORD Timestamp::GetIdentifier()const
    {
        return Mybase::GetDataAs<WORD>(Identifier);
    }

    void Timestamp::SetIdentifier(WORD NewIdentifier)
    {
        Mybase::SetDataFrom<WORD>(NewIdentifier, Identifier);
    }

    WORD Timestamp::GetSequenceNumber() const
    {
        return Mybase::GetDataAs<WORD>(SequenceNumber);
    }

    void Timestamp::SetSequenceNumber(WORD NewSequenceNumber)
    {
        Mybase::SetDataFrom<WORD>(NewSequenceNumber, SequenceNumber);
    }

    DWORD Timestamp::GetOriginateTime() const
    {
        return Mybase::GetDataAs<DWORD>(OriginateTime);
    }

    void Timestamp::SetOriginateTime(DWORD NewOriginateTime)
    {
        Mybase::SetDataFrom<DWORD>(NewOriginateTime, OriginateTime);
    }

    DWORD Timestamp::GetReceiveTime() const
    {
        return Mybase::GetDataAs<DWORD>(ReceiveTime);
    }

    void Timestamp::SetReceiveTime(DWORD NewReceiveTime)
    {
        Mybase::SetDataFrom<DWORD>(NewReceiveTime, ReceiveTime);
    }

    DWORD Timestamp::GetTransmitTime() const
    {
        return Mybase::GetDataAs<DWORD>(TransmitTime);
    }

    void Timestamp::SetTransmitTime(DWORD NewTransmitTime)
    {
        Mybase::SetDataFrom<DWORD>(NewTransmitTime, TransmitTime);
    }

    WORD AddressMask::GetIdentifier() const
    {
        return Mybase::GetDataAs<WORD>(Identifier);
    }

    void AddressMask::SetIdentifier(WORD NewIdentifier)
    {
        Mybase::SetDataFrom<WORD>(NewIdentifier, Identifier);
    }

    WORD AddressMask::GetSequenceNumber() const
    {
        return Mybase::GetDataAs<WORD>(SequenceNumber);
    }

    void AddressMask::SetSequenceNumber(WORD NewSequenceNumber)
    {
        Mybase::SetDataFrom<WORD>(NewSequenceNumber, SequenceNumber);
    }

    DWORD AddressMask::GetSubnetMask() const
    {
        return Mybase::GetDataAs<DWORD>(SubnetMask);
    }

    void AddressMask::SetSubnetMask(DWORD NewSubnetMask)
    {
        Mybase::SetDataFrom<DWORD>(NewSubnetMask, SubnetMask);
    }

    BYTE DestinationUnreachable::GetLength() const
    {
        return Mybase::GetDataAs<BYTE>(Length);
    }

    void DestinationUnreachable::SetLength(BYTE NewLength)
    {
        Mybase::SetDataFrom<BYTE>(NewLength, Length);
    }

    WORD DestinationUnreachable::GetNextHopMTU() const
    {
        return Mybase::GetDataAs<WORD>(NextHopMTU);
    }

    void DestinationUnreachable::SetNextHopMTU(WORD NewNextHopMTU)
    {
        Mybase::SetDataFrom<WORD>(NewNextHopMTU, NextHopMTU);
    }

    void DestinationUnreachable::GetIPData(LPVOID Dst, int Start, int Size) const
    {
        Mybase::GetData(Dst, IPDatagram + Start, Size);
    }

    void DestinationUnreachable::SetIPData(LPVOID Src, int Start, int Size)
    {
        Mybase::SetData(Src, IPDatagram + Start, Size);
    }

    LPCSTR DestinationUnreachable::what() const noexcept
    {
        return Descriptions[GetCode()];
    }    
}

int ICMPv4::ToDevice(NetworkAdapter& Device)
{
    SetChecksum(VerifyChecksum(1));
    return Mybase::ToDevice(Device);
}

void ICMPv4::Print(const char *Title) const
{
    if (Title) {cprintf((char*)Title);}
    Mybase::Print("");
    cprintf((char*)"------------------------------\n");
    cprintf((char*)"     Type: %d\n", GetType());
    cprintf((char*)"     Code: %d\n", GetCode());
    cprintf((char*)" Checksum: 0x%x (Verified: 0x%x)\n", GetChecksum(), VerifyChecksum());
}

void ICMPv4::Register()
{
    // Do nothing...
}

void ICMPv4::Main(NetworkAdapter* Device, const Mybase& Frame)
{
    ICMP* ICMPFrame = new ICMP(Frame);
    //ICMPFrame->Print("ICMP received: \n");
    if (!ICMPFrame->IsValid())
    {
        cprintf((char*)"[ICMPv4] Invalid ICMP frame.\n");
        return;
    }

    auto FrameType = ICMPFrame->GetType();
    switch (FrameType)
    {
    case TEchoReply:
        if (ICMPControllers::PingEcho::IsTesting)
        {
            using namespace ICMPControllers;
            if ((((PingEcho*)ICMPFrame)->GetIdentifier() == PingEcho::CurrentID) &&
                (((PingEcho*)ICMPFrame)->GetSequenceNumber() == PingEcho::CurrentSN))
            {
                PingEcho::IsReceived = 1;
            }
        }
        break;

    case TEchoRequest:
        {
            ICMP* ResponseFrame = new ICMP(*ICMPFrame);
            ResponseFrame->SetType(TEchoReply);
            ResponseFrame->SetDestinationAddress(ICMPFrame->GetSourceAddress());
            //ResponseFrame->Print("ICMP response: \n");
            ResponseFrame->ToDevice(*Device);
            delete ResponseFrame;
        }
        break;

    case TDestinationUnreachable:
    case TSourceQuench:
    case TRedirectMessage:
    case TRouterAdvertisement:
    case TRouterSolicitation:
    case TTimeExceeded:
    case TBadIPheader:
    case TTimestamp:
    case TTimestampReply:
    case TInformationRequest:
    case TInformationReply:
    case TAddressMaskRequest:
    case TAddressMaskReply:
    case TTraceroute:
    case TExtendedEchoRequest:
    case TExtendedEchoReply:
    default:
        break;
    }

    delete ICMPFrame;
}

// ------------------------------------------------------------------ //

// ------------------------------- TCP ------------------------------ //

ArrayList<TCB<4>, TCP<4>::TCBTableSize> TCP<4>::TCBTable;

// ------------------------------------------------------------------ //



// ------------------------------- UDP ------------------------------ //

ArrayList<UDPController<4>, UDP<4>::UDPTableSize> UDP<4>::UDPTable;

// ------------------------------------------------------------------ //

_EXTERN_C

int argint(int n, int* ip);
int INet_ARPRequest()
{
    DWORD TargetIPAddress = 0;
    if (argint(0, (int*)&TargetIPAddress) < 0) {return -1;}

    return !ARP::Tester::ARPing(TargetIPAddress);
}

int INet_ShowIPAddress()
{
    LPCSTR Format1 = "dev%d: %x:%x\n    link/ether %02x:%02x:%02x:%02x:%02x:%02x brd %02x:%02x:%02x:%02x:%02x:%02x\n";
    LPCSTR Format2 = "    inet %d.%d.%d.%d/%d brd %d.%d.%d.%d\n";
    for (int i = 0; i < NetworkAdapterListSize; ++i)
    {
        auto Device = NetworkAdapterList[i];
        auto IPAddr = IPv4::IPFind(Device);
        const BYTE* MACAddress = NetworkAdapterList[i]->GetMACAddress();
        const BYTE* MACBrd = EthernetFrame::BroadcastAddr;
        cprintf((char*)Format1, i, Device->VendorID(), Device->DeviceID(),
            MACAddress[0], MACAddress[1], MACAddress[2],
            MACAddress[3], MACAddress[4], MACAddress[5],
            MACBrd[0], MACBrd[1], MACBrd[2],
            MACBrd[3], MACBrd[4], MACBrd[5]);
        if (IPAddr != IPv4::AdapterIPAddressTable.end())
        {
            int IP0, IP1, IP2, IP3, MaskN, Brd0, Brd1, Brd2, Brd3;
            IPv4::IPSplit(IPAddr->IPAddress, IP0, IP1, IP2, IP3);
            MaskN = IPv4::MaskToNum(IPAddr->SubnetMask);
            IPv4::IPSplit(IPv4::GetBroadcastAddress(IPAddr->IPAddress, IPAddr->SubnetMask)
                , Brd0, Brd1, Brd2, Brd3);
            cprintf((char*)Format2,
                IP0, IP1, IP2, IP3, MaskN, Brd0, Brd1, Brd2, Brd3);
        }
    }
    return 0;
}

int INet_SetIPAddress()
{
    int Index = 0;
    DWORD Addr = 0;
    DWORD Mask = 0;
    if (argint(0, (int*)&Index) < 0) {return -1;}
    if (argint(1, (int*)&Addr) < 0) {return -1;}
    if (argint(2, (int*)&Mask) < 0) {return -1;}

    if (NetworkAdapterListSize <= Index) {return 0xBAADF00D;}

    auto it = IPv4::IPFind(NetworkAdapterList[Index]);
    if (it != IPv4::AdapterIPAddressTable.end())
    {
        cprintf((char*)"Device %d already has IP address.\n", Index);
        return 0x0D06F00D;
    }
    IPv4::IPAllocate(NetworkAdapterList[Index], Addr, Mask);
    return 0;
}

int INet_PrintARPTable()
{
    //cprintf((char*)"%d in table\n", ARP::ARPTable.size());
    cprintf((char*)"IP address       HW type     Flags       HW address            Mask\n");
    for (auto i : ARP::ARPTable)
    {
        int IP0, IP1, IP2, IP3;
        IPv4::IPSplit(i.Address, IP0, IP1, IP2, IP3);
        cprintf((char*)"%d.%d.%d.%d      0x%x        0x%x        %02x:%02x:%02x:%02x:%02x:%02x     *   \n",
            IP0, IP1, IP2, IP3, i.HWType, i.Flags,
            i.MACAddress[0], i.MACAddress[1], i.MACAddress[2],
            i.MACAddress[3], i.MACAddress[4], i.MACAddress[5]);
    }
    return 0;
}

int INet_DelIPAddress()
{
    int Index = 0;
    if (argint(0, (int*)&Index) < 0) {return -1;}
    IP::IPDelete(NetworkAdapterList[Index]);
    return 0;
}

int INet_RTAddStatic()
{
    DWORD Dst = 0;
    DWORD Mask = 0;
    DWORD Via = 0;
    int Device = -1;
    if (argint(0, (int*)&Dst) < 0) {return -1;}
    if (argint(1, (int*)&Via) < 0) {return -3;}
    if (argint(2, (int*)&Mask) < 0) {return -2;}
    if (argint(3, (int*)&Device) < 0) {return -4;}

    if (!(Dst || Mask || Via)) {return 1;}

    IP::RouteTableItem Route;
    Route.Destination = Dst;
    Route.Gateway = Via;
    Route.Genmask = Mask;
    Route.Flags = 0;
    if (Route.Gateway)
    {
        if (Route.Destination)
        {
            auto it = IP::RouteTableFind([Route](IP::RouteTableItem Item)
            {
                return !Item.Destination && Item.Gateway == Route.Gateway;
            });
            if (it == IP::RouteTable.end()) {return 0xDEADBEEF;}
            Route.Iface = it->Iface;
        }
        else
        {
            if (Device < 0 || NetworkAdapterListSize <= Device) {return 0xBAADF00D;}
            Route.Iface = NetworkAdapterList[Device];
        }
        Route.Destination &= Route.Genmask;
        Route.Flags |= IPv4::RouteTableFlags::RTF_GATEWAY;
    }
    else
    {
        if (Device < 0 || NetworkAdapterListSize <= Device) {return 0xBAADF00D;}
        Route.Iface = NetworkAdapterList[Device];
        Route.Genmask = ~(DWORD(0));
        Route.Flags |= IPv4::RouteTableFlags::RTF_HOST;
    }
    Route.Metric = 0;
    Route.Ref = 0;
    Route.Use = 0;
    Route.Flags |= IPv4::RouteTableFlags::RTF_UP;
    IP::RouteTableAdd(Route);
    return 0;
}

int INet_RTPrint()
{
    cprintf((char*)"Kernel IP routing table\n"
        "Destination     Gateway         Genmask         Flags Metric Ref    Use Iface\n");
    for (auto i : IP::RouteTable)
    {
        int IP0, IP1, IP2, IP3;
        IPv4::IPSplit(i.Destination, IP0, IP1, IP2, IP3);
        int GW0, GW1, GW2, GW3;
        IPv4::IPSplit(i.Gateway, GW0, GW1, GW2, GW3);
        int Mask0, Mask1, Mask2, Mask3;
        IPv4::IPSplit(i.Genmask, Mask0, Mask1, Mask2, Mask3);
        cprintf((char*)"%d.%d.%d.%d      %d.%d.%d.%d      %d.%d.%d.%d      ",
            IP0, IP1, IP2, IP3, GW0, GW1, GW2, GW3, Mask0, Mask1, Mask2, Mask3);
        /*for (int j = 0; j < sizeof(WORD); ++j)
        {
            if (i.Flags & (1 << j)) {cprintf((char*)"%c", IP::RouteTableFlags::Symbols[j]);}
        }*/
        cprintf((char*)"0x%x   %d    %d        %d ", i.Flags, i.Metric, i.Ref, i.Use);
        BOOL OK = 0;
        for (int j = 0; j < NetworkAdapterListSize && !OK; ++j)
        {
            if (NetworkAdapterList[j] == i.Iface)
            {
                cprintf((char*)"eth%d", j);
                OK = 1;
            }
        }
        if (!OK) {cprintf((char*)"(nullptr)");}
        cprintf((char*)"\n");
    }
    return 0;
}

int INet_RTDelete()
{
    return 0;
}

int INet_Ping()
{
    DWORD TargetIPAddress = 0;
    if (argint(0, (int*)&TargetIPAddress) < 0) {return -1;}
    ICMPControllers::PingEcho::Ping(TargetIPAddress);
    return 0;
}

void RegisterProtocols()
{
    for (int i = 0; ProtocolInvokers[i].Register && ProtocolInvokers[i].InvokeMain; ++i)
    {
        ProtocolInvokers[i].Register();
    }
}

_END_EXTERN_C
