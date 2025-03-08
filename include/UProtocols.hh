#pragma once

#ifndef UPROTOCOL_H
#define UPROTOCOL_H

#ifdef __cplusplus

#include "UEtherFrame.hh"
#include "UArrayList.tcc"

_EXTERN_C
#include "spinlock.h"
_END_EXTERN_C

// ---------- Layer-2 protocols ---------- //

class ARP : public EthernetFrame
{
public:
    using Mybase = EthernetFrame;

    static const auto EtherType          = 0x0806;
    static const auto ARPTableSize       = 4096;
    static const auto ARPTimeout         = 300;

    static const auto HeaderSize         = 8;
    static const auto HardwareType       = 0; // 0 - 1
    static const auto ProtocolType       = 2; // 2 - 3
    static const auto HardwareLength     = 4; // 4
    static const auto ProtocolLength     = 5; // 5
    static const auto Operation          = 6; // 6 - 7

    static const auto SenderHardwareAddr = HeaderSize + 0; // 0 - 5
    static const auto SenderProtocolAddr = HeaderSize + 6; // 6 - 9
    static const auto TargetHardwareAddr = HeaderSize + 10; // 10 - 15
    static const auto TargetProtocolAddr = HeaderSize + 16; // 16 - 19

    static time_t ARPTimestamp;
    static spinlock ARPLock;

    struct ARPTableItem
    {
        enum ATF : BYTE // ARP Flag values.
        {
            COM         = 0b00000010, // completed entry (ha valid)
            PERM        = 0b00000100, // permanent entry
            PUBL        = 0b00001000, // publish entry
            USETRAILERS = 0b00010000, // has requested trailers
            NETMASK     = 0b00100000, // want to use a netmask (only for proxy entries)
            DONTPUB     = 0b01000000, // don't answer this addresses

            Incomplete  = 0b00000000,
            ManuallySet = 0b00000110,
        };

        DWORD Address;
        DWORD HWType;
        ATF   Flags;
        BYTE  MACAddress[6];
        /*__declspec(deprecated(
            // Reference: https://superuser.com/questions/1737928/arp-cache-what-does-a-mask-value-of-represent
            "This functionality was removed in Linux 2.1.79. Since then, the column "
            "always says * and any attempts to create a proxy-ARP entry with a netmask "
            "different from 255.255.255.255 are rejected."))
        DWORD Mask = 0xFFFFFFFF;*/
        const NetworkAdapter* Adapter;
    };

    static ArrayList<ARPTableItem, ARPTableSize> ARPTable;

    enum Operations {Request = 1, Reply = 2};

    ARP() : Mybase() {SetEtherType(EtherType);}
    ARP(const Mybase& Frame) : Mybase(Frame) {}

    // Getters and Setters
    WORD GetHardwareType()const;
    WORD GetProtocolType()const;
    BYTE GetHardwareLength()const;
    BYTE GetProtocolLength()const;
    WORD GetOperation()const;
    void GetSenderHardwareAddress(BYTE* MACAddrDst)const;
    DWORD GetSenderProtocolAddress()const;
    void GetTargetHardwareAddress(BYTE* MACAddrDst)const;
    DWORD GetTargetProtocolAddress()const;

    void SetHardwareType(WORD NewHardwareType);
    void SetProtocolType(WORD NewProtocolType);
    void SetHardwareLength(BYTE NewHardwareLength);
    void SetProtocolLength(BYTE NewProtocolLength);
    void SetOperation(WORD NewOperation);
    void SetSenderHardwareAddress(const BYTE* MACAddr);
    void SetSenderProtocolAddress(DWORD SenderProtocolAddress);
    void SetTargetHardwareAddress(const BYTE* MACAddr);
    void SetTargetProtocolAddress(DWORD TargetProtocolAddress);

    BOOL IsValid()const;
    void Prepare(Operations Op);

    static void AcquireLock();
    static void ReleaseLock();

    // ARP Table operations
    static void ARPTableAdd(ARPTableItem NewItem);
    static void ARPTableRemove(DWORD IP);
    static void ARPTableUpdate(DWORD IP, ARPTableItem NewItem);
    static decltype(ARPTable)::iterator ARPTableFind(DWORD IP);

    // arping tester
    struct Tester
    {
        static const auto MaxDelay = 30000000;

        static BOOL ARPingIsTesting;
        static BOOL ARPingIsReceived;

        enum ReturnValue
        {
            Passed = 0,
            IPNotFound = 1,
            Timeout = 2,
        };

        static int ARPing(DWORD TargetIPAddress);
    };

    // Main function
    void Print(const char* Title)const;
    static BYTE* RequestFrom(NetworkAdapter& Device, DWORD IP);
    static void Register();
    static void Main(NetworkAdapter* Device, const EthernetFrame& Frame);
};

// ---------- Layer-3 protocols ---------- //

typedef class IPv4 : public EthernetFrame
{
public:
    using Mybase = EthernetFrame;

    static const auto EtherType                       = 0x0800;
    static const auto IPTableSize                     = 100;
    static const auto RouteTableSize                  = 30;
    static const auto ProtocolTableSize               = 0xFF;

    static const auto CurrentNetwork                  = 0x00000000; // 0.0.0.0
    static const auto Localhost                       = 0x0000007F; // 127.0.0.0 (LE)
    static const auto LocalhostMask                   = 0x000000FF; // 255.0.0.0 (LE)
    static const auto Broadcast                       = 0xFFFFFFFF; // 255.255.255.255

    static const auto HeaderSizeMin                   = 20;
    static const auto HeaderSizeMax                   = 60;
    static const auto Version                         = 0; // 0 - 3bit
    static const auto VersionMask                     = 0b11110000;
    static const auto InternetHeaderLength            = 0; // 4bit - 7bit (1 byte)
    static const auto IHLMask                         = 0b00001111;
    static const auto DifferentiatedServicesCodePoint = 1; // 8bit - 13bit
    static const auto DSCPMask                        = 0b11111100;
    static const auto ExplicitCongestionNotification  = 1; // 14bit - 15bit (2 byte)
    static const auto ECNMask                         = 0b00000011;
    static const auto TotalLength                     = 2; // 2 - 3
    static const auto Identification                  = 4; // 4 - 5
    static const auto Flags                           = 6; // 6byte + 3bit
    static const auto FlagsMask                       = 0b1110000000000000;
    static const auto FragmentOffset                  = 6; // 6byte + 4bit - 7
    static const auto FragmentOffsetMask              = 0b0001111111111111;
    static const auto TimeToLive                      = 8; // 8
    static const auto Protocol                        = 9; // 9
    static const auto HeaderChecksum                  = 10; // 10 - 11
    static const auto SourceAddress                   = 12; // 12 - 15
    static const auto DestinationAddress              = 16; // 16 - 19
    static const auto Options                         = 20; // 20 - ???

    enum FragmentFlags : BYTE
    {
        DF = 0b010, MF = 0b001
    };

    enum IPProtocols : DWORD
    {
        P_ICMP  = 1,   // Internet Control Message Protocol    (RFC 792)
        P_IGMP  = 2,   // Internet Group Management Protocol   (RFC 1112)
        P_TCP   = 6,   // Transmission Control Protocol        (RFC 793)
        P_UDP   = 17,  // User Datagram Protocol               (RFC 768)
        P_ENCAP = 41,  // IPv6 Encapsulation (6to4 and 6in4)   (RFC 2473)
        P_OSPF  = 89,  // Open Shortest Path First             (RFC 2328)
        P_SCTP  = 132  // Stream Control Transmission Protocol (RFC 4960)
    };

    static struct IPv4Protocol
    {
        using InitFunctionType = void(*)();
        using MainFuncType = void(*)(NetworkAdapter*, const IPv4&);

        //DWORD ProtocolType;
        InitFunctionType Register = nullptr;
        MainFuncType InvokeMain = nullptr;
    }Protocols[ProtocolTableSize];

    static spinlock IPLock;

    // Address Table
    struct AdapterIPAddressItem
    {
        const NetworkAdapter* Adapter = nullptr;
        // IPv4
        DWORD IPAddress;
        DWORD SubnetMask;
    };
    static ArrayList<AdapterIPAddressItem, IPTableSize> AdapterIPAddressTable;

    // Route table
    struct RouteTableFlags // Compatibility with IPv6
    {
        enum : WORD
        {
            RTF_UP        = 0b0000000000000001, // route usable
            RTF_GATEWAY   = 0b0000000000000010, // destination is a gateway
            RTF_HOST      = 0b0000000000000100, // host entry (net otherwise)
            RTF_REINSTATE = 0b0000000000001000, // reinstate route after tmout
            RTF_DYNAMIC   = 0b0000000000010000, // created dyn. (by redirect)
            RTF_MODIFIED  = 0b0000000000100000, // modified dyn. (by redirect)
            RTF_MTU       = 0b0000000001000000, // specific MTU for this route
            RTF_MSS       = RTF_MTU,            // Compatibility :(
            RTF_WINDOW    = 0b0000000010000000, // per route window clamping
            RTF_IRTT      = 0b0000000100000000, // Initial round trip time
            RTF_REJECT    = 0b0000001000000000  // Reject route
        };

        constexpr static const auto Symbols = "UGHRDM   !      ";
    };

    struct RouteTableItem
    {
        DWORD Destination;
        DWORD Gateway;
        DWORD Genmask;
        WORD  Flags;
        DWORD Metric;
        DWORD Ref;
        DWORD Use;
        NetworkAdapter* Iface = nullptr;
    };
    static ArrayList<RouteTableItem, RouteTableSize> RouteTable;

    IPv4() : Mybase()
    {
        SetEtherType(EtherType);
        SetVersion(4);
        SetInternetHeaderLength(5);
        SetFlags(FragmentFlags::DF);
        SetTotalLength(DataSize());
    }
    IPv4(const Mybase& Frame) : Mybase(Frame) {}

    // IPv4 Getters and Setters
    BYTE GetVersion()const;
    BYTE GetInternetHeaderLength()const;
    BYTE GetDifferentiatedServicesCodePoint()const;
    BYTE GetExplicitCongestionNotification()const;
    WORD GetTotalLength()const;
    WORD GetIdentification()const;
    BYTE GetFlags()const;
    WORD GetFragmentOffset()const;
    BYTE GetTimeToLive()const;
    BYTE GetProtocol()const;
    WORD GetHeaderChecksum()const;
    DWORD GetSourceAddress()const;
    DWORD GetSourceAddressBE()const;
    DWORD GetDestinationAddress()const;
    DWORD GetDestinationAddressBE()const;
    void GetOptions(LPVOID Dst)const;
    void GetData(LPVOID Dst, int Start, int Size, BOOL Reverse = 0)const;

    template<typename Tp>
    Tp GetDataAs(int Start, BOOL Reverse = 1)const
    {
        union {Tp i; BYTE b[sizeof(Tp)];} Splitter;
        IPv4::GetData(Splitter.b, Start, sizeof(Tp), Reverse);
        return Splitter.i;
    }

    void SetVersion(BYTE NewVersion);
    void SetDifferentiatedServicesCodePoint(BYTE NewDSCP);
    void SetExplicitCongestionNotification(BYTE NewECN);
    void SetIdentification(WORD NewIdentification);
    void SetFlags(WORD NewFlags);
    void SetFragmentOffset(WORD NewFragmentOffset);
    void SetTimeToLive(BYTE NewTimeToLive);
    void SetProtocol(BYTE NewProtocol);
    void SetSourceAddress(DWORD NewSourceAddress);
    void SetDestinationAddress(DWORD NewSourceAddress);
    void SetData(LPCVOID Src, int Start, int Size);

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
        IPv4::SetData(Splitter.b, Start, sizeof(Tp));
    }

protected:
    void SetInternetHeaderLength(BYTE NewIHL);
    void SetTotalLength(WORD NewTotalLength);
    void SetOptions(LPCVOID Src, int Size);

private:
    void SetHeaderChecksum(WORD NewHeaderChecksum);
    WORD VerifyChecksum(BOOL ComputeOnly = 0) const;

public:
    BOOL IsValid()const;
    int DataSize()const{return GetTotalLength() - GetInternetHeaderLength() * sizeof(DWORD);}
    void Resize(WORD NewSize);

    // Route Table operations
    static void RouteTableAdd(RouteTableItem NewItem);

    template<typename Pred>
    static void RouteTableRemove(Pred Pr)
    {
        auto it = RouteTableFind(Pr);
        if (it == RouteTable.end()) {return;}
        RouteTable.erase(it);
    }

    template<typename Pred>
    static void RouteTableUpdate(Pred Pr, RouteTableItem NewItem)
    {
        auto it = RouteTableFind(Pr);
        if (it == RouteTable.end()) {return;}
        *it = NewItem;
    }

    template<typename Pred>
    static decltype(RouteTable)::iterator RouteTableFind(Pred Pr)
    {
        return find_if(RouteTable.begin(), RouteTable.end(), Pr);
    }

    static decltype(RouteTable)::iterator RouteTableMatch(DWORD Target);

    // IP allocations
    static decltype(AdapterIPAddressTable)::iterator IPFind(const NetworkAdapter* Adapter);
    static decltype(AdapterIPAddressTable)::iterator IPFind(DWORD Addr);
    static void IPAllocate(const NetworkAdapter* Adapter, DWORD Addr, DWORD Mask);
    static void IPDelete(const NetworkAdapter* Adapter);

    // Tools
    static void IPSplit(DWORD Addr, int& P1, int& P2, int& P3, int& P4);
    static int MaskToNum(DWORD Mask);
    static DWORD GetNetAddress(DWORD Addr, DWORD Mask);
    static DWORD GetBroadcastAddress(DWORD Addr, DWORD Mask);

    // Main functions
    int ToDevice(NetworkAdapter& Device, WORD ResizeTo = 0);
    void Print(const char* Title)const;
    static void Register();
    static void Main(NetworkAdapter* Device, const EthernetFrame& Frame);
}IP;

class IPv6 : public EthernetFrame
{
public:
    using Mybase = EthernetFrame;

    static const auto EtherType           = 0x86DD;
    static const auto IPTableSize         = 100;
    static const auto RouteTableSize      = 30;

    static const auto Unspecified6        = uint128_t(0x0);
    static const auto Localhost6          = uint128_t(0x1);
    static const auto Mapped4to6          = uint128_t(0xFFFF) << 32;
    static const auto Translated4to6      = uint128_t(0xFFFF) << 48;
    static const auto DiscardPrefix6      = uint128_t(0x100) << 112;
    __declspec(deprecated)
    static const auto Scheme6to4          = uint128_t(0x2002) << 112;
    static const auto UniqueLocalhost6    = uint128_t(0xFC00) << 112;
    static const auto LinkLocalhost6      = uint128_t(0xFE80) << 112;
    __declspec(deprecated)
    static const auto SiteLocalhost6      = uint128_t(0xFEC0) << 112;
    static const auto Multicast6          = uint128_t(0xFF00) << 112;

    enum MulticastAddresses6 : uint128_t
    {
        Broadcast6 = uint128_t(0x1) << 112,
        Routers6   = uint128_t(0x2) << 112,
    };

    static const auto HeaderSize6         = 40;
    static const auto Version6            = 0; // First 3 fields in IPv6 header is not aligned to bytes
    static const auto VersionMask6        = 0b11110000000000000000000000000000;
    static const auto TrafficClass6       = 0;
    static const auto TrafficClassMask6   = 0b00001111111100000000000000000000;
    static const auto FlowLabel6          = 0;
    static const auto FlowLabelMask6      = 0b00000000000011111111111111111111;
    static const auto PayloadLength6      = 4;
    static const auto NextHeader6         = 6;
    static const auto HopLimit6           = 7;
    static const auto SourceAddress6      = 8;
    static const auto DestinationAddress6 = 24;

    static spinlock IPv6Lock;

    struct AdapterIPAddressItem
    {
        const NetworkAdapter* Adapter = nullptr;
        // IPv6
        uint128_t IPAddress6;
        WORD      Prefix6;
        uint128_t DefGateway6;
    };
    static ArrayList<AdapterIPAddressItem, IPTableSize> AdapterIPAddressTable;

    struct RouteTableFlags
    {
        enum : DWORD
        {
            RTF_DEFAULT    = 0b0000000000000001U << 16, // default - learned via ND
            RTF_ALLONLINK  = 0b0000000000000010U << 16, // (deprecated and will be removed) fallback, no routers on link
            RTF_ADDRCONF   = 0b0000000000000100U << 16, // addrconf route - RA
            RTF_PREFIX_RT  = 0b0000000000001000U << 16, // A prefix only route - RA
            RTF_ANYCAST    = 0b0000000000010000U << 16, // Anycast

            RTF_NONEXTHOP  = 0b0000000000100000U << 16, // route with no nexthop
            RTF_EXPIRES    = 0b0000000001000000U << 16,

            RTF_ROUTEINFO  = 0b0000000010000000U << 16, // route information - RA

            RTF_CACHE      = 0b0000000100000000U << 16, // read-only: can not be set by user
            RTF_FLOW	   = 0b0000001000000000U << 16, // flow significant route
            RTF_POLICY     = 0b0000010000000000U << 16, // policy route

            RTF_PREF_MASK  = 0b0001100000000000U << 16,

            RTF_PCPU       = 0b0100000000000000U << 16, // read-only: can not be set by user
            RTF_LOCAL      = 0b1000000000000000U << 16,
        };

        static DWORD RTF_PREF(DWORD pref) {return ((pref) << 27) & RTF_PREF_MASK;}
    };

    struct RouteTableItem
    {
        uint128_t Destination;
        WORD      Prefix;
        uint128_t NextHop;
        DWORD     Flags;
        DWORD     Metric;
        DWORD     Ref;
        DWORD     Use;
        const NetworkAdapter* Iface = nullptr;
    };
    static ArrayList<RouteTableItem, RouteTableSize> RouteTable;

    IPv6() : Mybase() {}
    IPv6(const Mybase& Frame) : Mybase(Frame) {}

    // IPv6 Getters and Setters
    // TODO...
};

template<typename Signature>
concept IPProtocols = requires
{
    IsSame<Signature, IPv4>::value &&
    IsSame<Signature, IPv6>::value;
};

typedef class ICMPv4 : public IPv4
{
public:
    using Mybase = IPv4;

    static const auto ProtocolNumber = 1;

    static const auto HeaderSize     = 8;
    static const auto Type           = 0;
    static const auto Code           = 1;
    static const auto Checksum       = 2; // 2 - 3
    static const auto RestOfHeader   = 4; // 4 - 7

    // Field assignments
    enum Types : BYTE
    {
        TEchoReply              = 0,
        TDestinationUnreachable = 3,
        TSourceQuench           = 4, // deprecated
        TRedirectMessage        = 5,
        TEchoRequest            = 8,
        TRouterAdvertisement    = 9,
        TRouterSolicitation     = 10,
        TTimeExceeded           = 11,
        TBadIPheader            = 12,
        TTimestamp              = 13,
        TTimestampReply         = 14,
        TInformationRequest     = 15, // deprecated
        TInformationReply       = 16, // deprecated
        TAddressMaskRequest     = 17, // deprecated
        TAddressMaskReply       = 18, // deprecated
        TTraceroute             = 30, // deprecated
        TExtendedEchoRequest    = 42,
        TExtendedEchoReply      = 43
    };

    ICMPv4() : Mybase() {SetProtocol(ProtocolNumber);}
    ICMPv4(const Mybase& Frame) : Mybase(Frame) {}

    // Getters and Setters
    BYTE GetType()const;
    BYTE GetCode()const;
    WORD GetChecksum()const;
    void GetData(LPVOID Dst, int Start, int Size, BOOL Reverse = 0)const;

    template<typename Tp>
    Tp GetDataAs(int Start, BOOL Reverse = 1)const
    {
        union {Tp i; BYTE b[sizeof(Tp)];} Splitter;
        ICMPv4::GetData(Splitter.b, Start, sizeof(Tp), Reverse);
        return Splitter.i;
    }

    void SetType(BYTE NewType);
    void SetCode(BYTE NewCode);
    void SetData(LPCVOID Src, int Start, int Size);

private:
    void SetChecksum(WORD NewChecksum);
    WORD VerifyChecksum(BOOL ComputeOnly = 0)const;

public:
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
        ICMPv4::SetData(Splitter.b, Start, sizeof(Tp));
    }

    BOOL IsValid()const;

    int ToDevice(NetworkAdapter& Device);
    void Print(const char* Title)const;
    static void Register();
    static void Main(NetworkAdapter* Device, const Mybase& Frame);
}ICMP;

namespace ICMPControllers
{
    class PingEcho : public ICMP
    {
    public:
        using Mybase = ICMP;

        static const auto RequestType    = TEchoRequest;
        static const auto ReplyType      = TEchoReply;

        static const auto Identifier     = 0;
        static const auto SequenceNumber = 2;
        static const auto Timestamp      = 4; // Option, depend on OS
        static const auto Datagram       = 4;

        // Default data (已读乱回)
        constexpr static const auto DefaultDataUNIX =
            R"pre( !"#$%&'()*+,-./01234567)pre";
        static const auto DefaultDataUNIXSize = 40;

        constexpr static const auto DefaultDataWin =
            R"pre(abcdefghijklmnopqrstuvwabcdefghi)pre";
        static const auto DefaultDataWinSize = 32;

        static BOOL IsTesting;
        static WORD CurrentID;
        static WORD CurrentSN;
        static BOOL IsReceived;
        static const auto MaxDelay = 300000000;

        PingEcho() : Mybase() {SetCode(0);}
        PingEcho(const Mybase& Frame) : Mybase(Frame) {}

        WORD GetIdentifier()const;
        void SetIdentifier(WORD NewIdentifier);
        WORD GetSequenceNumber()const;
        void SetSequenceNumber(WORD NewSequenceNumber);
        //uint128_t GetTimestamp()const;
        //void SetTimestamp(uint128_t NewTimestamp);
        void GetData(LPVOID Dst, int Start, int Size)const;
        void SetData(LPCVOID Src, int Start, int Size);

        static void Ping(DWORD TargetIPAddress);
    };

    class SourceQuench : public ICMP
    {
    public:
        using Mybase = ICMP;

        static const auto Mytype     = TSourceQuench;

        static const auto IPDatagram = 4;

        SourceQuench() : Mybase()
        {
            SetType(Mytype);
            SetCode(0);
        }
        SourceQuench(const Mybase& Frame) : Mybase(Frame) {}

        void GetIPData(LPVOID Dst, int Start, int Size)const;
        __declspec(deprecated)
        void SetIPData(LPVOID Src, int Start, int Size);
    }__declspec(deprecated(
        R"(Since research suggested that "ICMP Source Quench was an ineffective )"
        R"((and unfair) antidote for congestion", routers' creation of source )"
        "quench messages was deprecated in 1995 by RFC 1812. Furthermore, "
        "forwarding of and any kind of reaction to (flow control actions) "
        "source quench messages was deprecated from 2012 by RFC 6633."));

    class Redirect : public ICMP
    {
    public:
        using Mybase = ICMP;

        static const auto Mytype     = TRedirectMessage;

        static const auto IPAddress  = 0;
        static const auto IPDatagram = 4;

        enum Code
        {
            Network     = 0,
            Host        = 1,
            NetworkServ = 2,
            HostServ    = 3
        };

        Redirect() : Mybase() {SetType(Mytype);}
        Redirect(const Mybase& Frame) : Mybase(Frame) {}

        DWORD GetIPAddress()const;
        void SetIPAddress(DWORD NewIPAddress);
        void GetIPData(LPVOID Dst, int Start, int Size)const;
        void SetIPData(LPVOID Src, int Start, int Size);
    };

    class TimeExceeded : public ICMP
    {
    public:
        using Mybase = ICMP;

        static const auto Mytype     = TTimeExceeded;

        static const auto IPDatagram = 4;

        enum Code
        {
            Transit     = 0,
            Fragment    = 1,
        };

        TimeExceeded() : Mybase() {SetType(Mytype);}
        TimeExceeded(const Mybase& Frame) : Mybase(Frame) {}

        void GetIPData(LPVOID Dst, int Start, int Size)const;
        void SetIPData(LPVOID Src, int Start, int Size);
    };

    class Timestamp : public ICMP
    {
    public:
        using Mybase = ICMP;

        static const auto RequestType    = TTimestamp;
        static const auto ReplyType      = TTimestampReply;

        static const auto Identifier     = 0;
        static const auto SequenceNumber = 2;
        static const auto OriginateTime  = 4;
        static const auto ReceiveTime    = 8;
        static const auto TransmitTime   = 12;

        Timestamp() : Mybase() {SetCode(0);}
        Timestamp(const Mybase& Frame) : Mybase(Frame) {}

        WORD GetIdentifier()const;
        void SetIdentifier(WORD NewIdentifier);
        WORD GetSequenceNumber()const;
        void SetSequenceNumber(WORD NewSequenceNumber);
        DWORD GetOriginateTime()const;
        void SetOriginateTime(DWORD NewOriginateTime);
        DWORD GetReceiveTime()const;
        void SetReceiveTime(DWORD NewReceiveTime);
        DWORD GetTransmitTime()const;
        void SetTransmitTime(DWORD NewTransmitTime);
    };

    class AddressMask : public ICMP
    {
    public:
        using Mybase = ICMP;

        static const auto RequestType    = TAddressMaskRequest;
        static const auto ReplyType      = TAddressMaskReply;

        static const auto Identifier     = 0;
        static const auto SequenceNumber = 2;
        static const auto SubnetMask     = 4;

        AddressMask() : Mybase() {SetCode(0);}
        AddressMask(const Mybase& Frame) : Mybase(Frame) {}

        WORD GetIdentifier()const;
        void SetIdentifier(WORD NewIdentifier);
        WORD GetSequenceNumber()const;
        void SetSequenceNumber(WORD NewSequenceNumber);
        DWORD GetSubnetMask()const;
        void SetSubnetMask(DWORD NewSubnetMask);
    };

    typedef class DestinationUnreachable : public ICMP
    {
    public:
        using Mybase = ICMP;

        static const auto Mytype      = TDestinationUnreachable;

        static const auto Length      = 1;
        static const auto NextHopMTU  = 2;
        static const auto IPDatagram  = 4;

        enum Code
        {
            NetworkUnreachable        = 0,
            HostUnreachable           = 1,
            ProtocolUnreachable       = 2, // the designated transport protocol is not supported
            PortUnreachable           = 3, // the designated protocol is unable to inform the host of the incoming message
            DatagramTooBig            = 4, // The datagram is too big. Packet fragmentation is required but the 'don't fragment' (DF) flag is on.
            SourceRouteFailed         = 5,
            NetworkUnknown            = 6,
            HostUnknown               = 7,
            SourceHostIsolated        = 8,
            NetworkProhibited         = 9,
            HostProhibited            = 10,
            NetworkUnreachableTOS     = 11,
            HostUnreachableTOS        = 12,
            CommunicationProhibited   = 13, // administrative filtering prevents packet from being forwarded
            HostPrecedenceViolation   = 14, // indicates the requested precedence is not permitted for the combination of host or network and port
            PrecedenceCutoff          = 15, // precedence of datagram is below the level set by the network administrators
        };

        constexpr static const LPCSTR Descriptions[] =
        {
            [NetworkUnreachable]      = "Network unreachable error.",
            [HostUnreachable]         = "Host unreachable error.",
            [ProtocolUnreachable]     = "The designated transport protocol is not supported",
            [PortUnreachable]         = "The designated protocol is unable to inform the host of the incoming message",
            [DatagramTooBig]          = R"(The datagram is too big. Packet fragmentation is required but the "don't fragment" (DF) flag is on.)",
            [SourceRouteFailed]       = "Source route failed error.",
            [NetworkUnknown]          = "Destination network unknown error.",
            [HostUnknown]             = "Destination host unknown error.",
            [SourceHostIsolated]      = "Source host isolated error.",
            [NetworkProhibited]       = "The destination network is administratively prohibited.",
            [HostProhibited]          = "The destination host is administratively prohibited.",
            [NetworkUnreachableTOS]   = "The network is unreachable for Type Of Service.",
            [HostUnreachableTOS]      = "The host is unreachable for Type Of Service.",
            [CommunicationProhibited] = "Communication administratively prohibited",
            [HostPrecedenceViolation] = "The requested precedence is not permitted for the combination of host or network and port",
            [PrecedenceCutoff]        = "Precedence of datagram is below the level set by the network administrators"
        };

        DestinationUnreachable() : Mybase() {SetType(Mytype);}
        DestinationUnreachable(const Mybase& Frame) : Mybase(Frame) {}

        BYTE GetLength()const;
        WORD GetNextHopMTU()const;
        void SetNextHopMTU(WORD NewNextHopMTU);
        void GetIPData(LPVOID Dst, int Start, int Size)const;
        void SetIPData(LPVOID Src, int Start, int Size);

    protected:
        void SetLength(BYTE NewLength);

    public:
        LPCSTR what() const noexcept;
    }ICMPException;
}

class ICMPv6 : public IPv6
{
    // TODO...
};

// ---------- Layer-4 protocols ---------- //

//#include "UProtocols2.tcc"

#endif

#ifdef __cplusplus
_EXTERN_C
#endif

// System calls
int INet_ARPRequest();
int INet_ShowIPAddress();
int INet_SetIPAddress();
int INet_PrintARPTable();
int INet_DelIPAddress();
int INet_RTAddStatic();
int INet_RTPrint();
int INet_RTDelete();
int INet_Ping();

void RegisterProtocols();

#ifdef __cplusplus
_END_EXTERN_C
#endif

#endif // UPROTOCOL_H
