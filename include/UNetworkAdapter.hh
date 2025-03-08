/****************************************************************************
* UNetwork.h -- Network adapter                                             *                         *
****************************************************************************/

#pragma once

#ifndef UNETWORK_H
#define UNETWORK_H

#include "UDef.hh"

#ifdef __cplusplus
_EXTERN_C
#endif

#include "pci.h"

#define Intel8254xInterruptCode 11

void Intel8254xInterrupt();

int NetworkAdapterSetup(struct pci_func* PCIFunction);

#ifdef __cplusplus
_END_EXTERN_C
#endif

#ifdef __cplusplus

class EthernetFrame;

__interface NetworkAdapter
{
    virtual DWORD VendorID()const = 0;
    virtual DWORD DeviceID()const = 0;

    virtual const BYTE* GetMACAddress()const = 0;

    virtual int Open() = 0;
    virtual int Close() = 0;
    virtual BOOL HasInterrupt() = 0;
    virtual void ClearInterrupt() = 0;
    virtual int Transmit(EthernetFrame& Frame) = 0;
    virtual int Receive(EthernetFrame* FrameBuffer) = 0;
};

class NetworkAdapterBase
{
public:
    using PCIFunctionType  = pci_func;
    using PCIFuncPointer   = PCIFunctionType*;
    using PCIFuncCPointer  = const PCIFuncPointer;

    using PortBaseAddrType = QWORD;
    using MMIOBaseAddrType = QWORD;
    using Address64Type    = volatile DWORD*;

    using IRQLineType      = BYTE;
    using IRQPinType       = BYTE;

    PortBaseAddrType         PortBaseAddress;
    MMIOBaseAddrType         MMIOBaseAddress;

    BYTE                     MACAddress[6];

    IRQLineType              IRQLine;
    //IRQPinType               IRQPin;

    // PCI loader
    void LoadFromPCI(PCIFuncCPointer PCIFunction);

    // Address loader
    void LoadBaseAddresses(PCIFuncCPointer PCIFunction);

    // Interrupt request loder
    void LoadInterruptRequests(PCIFuncCPointer PCIFunction);

    // Interrupt Register
    void RegisterIRQLine();

    // MAC Address Loader
    virtual void LoadMACAddress() = 0;
};

// ---------- Implementation of Network Adapters ---------- //

class Intel8254xNetworkAdapter final : public NetworkAdapterBase, public NetworkAdapter
{
    // Reference:
    // Intel. pci-pci-x-family-gbe-controllers-software-dev-manual
    // Adapt for 82540EP/EM, 82541xx, 82544GC/EI, 82545GM/EM, 82546GB/EB, and 82547xx
public:
    using Mybase            = NetworkAdapterBase;

    using RegisterAddrType  = WORD;
    using RegisterValueType = DWORD;

    static const DWORD Vendor = 0x8086;
    static const DWORD Device = 0x100E;

    // Table 13-2. Ethernet Controller Register Summary
    struct EthernetControllerRegisters
    {
        enum General
        {
            CTRL         = 0x00000, // Device Control
            EERD         = 0x00014  // EEPROM Read
        };

        enum Interrupt
        {
            ICR          = 0x000C0, // Interrupt Cause Read
            IMS          = 0x000D0, // Interrupt Mask Set/Read
            IMC          = 0x000D8  // Interrupt Mask Clear
        };

        enum Receive
        {
            RCTL         = 0x00100, // Receive Control
            RDBAL        = 0x02800, // Receive Descriptor Base Low
            RDBAH        = 0x02804, // Receive Descriptor Base High
            RDLEN        = 0x02808, // Receive Descriptor Length
            RDH          = 0x02810, // Receive Descriptor Head
            RDT          = 0x02818, // Receive Descriptor Tail
            MTA          = 0x05200, // Multicast Table Array (n)
            RAL0         = 0x05400, // Receive Address Low
            RAH0         = 0x05404  // Receive Address High
        };

        enum Transmit
        {
            TCTL         = 0x00400, // Transmit Control
            TDBAL        = 0x03800, // Transmit Descriptor Base Low
            TDBAH        = 0x03804, // Transmit Descriptor Base High
            TDLEN        = 0x03808, // Transmit Descriptor Length
            TDH          = 0x03810, // Transmit Descriptor Head
            TDT          = 0x03818  // Transmit Descriptor Tail
        };
    };

    // Table 13-3. Device Control Register
    // CTRL (00000h; R/W)
    struct DeviceControlRegister
    {
        enum
        {
            ASDE         = (1 << 5), // Auto-Speed Detection Enable.
            SLU          = (1 << 6), // Set Link Up
            RST          = (1 << 26) // Device Reset
        };
    };

    // Table 13-65. Interrupt Mask Set/Read Register
    // IMS (000D0h; R/W)
    struct InterruptMaskSetReadRegister
    {
        enum
        {
            TXQE         = (1 << 1), // Sets mask for Transmit Queue Empty.
            RXSEQ        = (1 << 3), // Sets mask for Receive Sequence Error.
            RXO          = (1 << 6), // Sets mask for on Receiver FIFO Overrun.
            RXT0         = (1 << 7), // Sets mask for Receiver Timer Interrupt.
        };
    };

    // Table 13-67. Receive Control Register
    // RCTL (00100h; R/W)
    struct ReceiveControlRegister
    {
        enum
        {
            EN           = (1 << 1), // Receiver Enable
            SBP          = (1 << 2), // Store Bad Packets
            UPE          = (1 << 3), // Unicast Promiscuous Enabled
            MPE          = (1 << 4), // Multicast Promiscuous Enabled
            LPE          = (1 << 5), // Long Packet Reception Enable
            // Receive Descriptor Minimum Threshold Size
            RDMTSHalf    = (0b00 << 8), // Free Buffer threshold is set to 1/2 of RDLEN.
            RDMTSQuarter = (0b01 << 8), // Free Buffer threshold is set to 1/4 of RDLEN.
            RDMTSEighth  = (0b10 << 8), // Free Buffer threshold is set to 1/8 of RDLEN.

            BAM          = (1 << 15), // Broadcast Accept Mode
            BSEX         = (1 << 25), // Buffer Size Extension

            // Receive Buffer Size (BSEX = 0)
            BSIZE2K      = (0b00 << 16),
            BSIZE1K      = (0b01 << 16),
            BSIZE512B    = (0b10 << 16),
            BSIZE256B    = (0b11 << 16),
            // Receive Buffer Size (BSEX = 1)
            BSIZE16K     = (0b01 << 16) | BSEX,
            BSIZE8K      = (0b10 << 16) | BSEX,
            BSIZE4K      = (0b11 << 16) | BSEX,

            SECRC        = (1 << 26), // Strip Ethernet CRC from incoming packet
        };
    };

    // Table 13-76. Transmit Control Register
    // TCTL (00400h; R/W)
    struct TransmitControlRegister
    {
        enum
        {
            EN           = (1 << 1), // Transmit Enable
            PSP          = (1 << 3)  // Pad Short Packets
        };
    };

    static const int RDescLayoutMaxSize = 16;//4096 / sizeof(ReceiveDescriptorLayout) / 2;
    static const int TDescLayoutMaxSize = 16;//4096 / sizeof(TransmitDescriptorLayout) / 2;

    // Datasheet 3.2.3 - Table 3.1
    struct ReceiveDescriptorLayout
    {
        QWORD BufferAddress  = 0;
        WORD  Length         = 0;
        WORD  PacketCheckSum = 0;
        BYTE  Status         = 0;
        BYTE  Errors         = 0;
        WORD  Special        = 0;
    }__declspec(packed) *RDescLayout;

    // Datasheet 3.3.3.1 - Table 3-10
    struct ReceiveDescriptorStatusField
    {
        enum
        {
            DD    = (1 << 0),
            EOP   = (1 << 1),
            IXSM  = (1 << 2),
            VP    = (1 << 3),
            RSV   = (1 << 4),
            TCPCS = (1 << 5),
            IPCS  = (1 << 6),
            PIF   = (1 << 7)
        };
    };

    // Datasheet 3.3.3 - Table 3.7
    struct TransmitDescriptorLayout
    {
        QWORD BufferAddress      = 0;
        WORD  Length        : 16 = 0;
        BYTE  Checksum      : 8  = 0;
        BYTE  Command       : 8  = 0;
        BYTE  Status        : 4  = 0;
        BYTE  Reserved      : 4  = 0;
        BYTE  CheckSumStart : 8  = 0;
        WORD  Special       : 16 = 0;
    }__declspec(packed) *TDescLayout;

    // Datasheet 3.3.3.1 - Table 3-10
    struct TransmitDescriptorCommandField
    {
        enum
        {
            EOP  = (1 << 0),
            IFCS = (1 << 1),
            IC   = (1 << 2),
            RS   = (1 << 3),
            RSV  = (1 << 4),
            DEXT = (1 << 5),
            VLE  = (1 << 6),
            IDE  = (1 << 7)
        };
    };

    // NetworkAdapter interface
    DWORD VendorID()const override {return Vendor;}
    DWORD DeviceID()const override {return Device;}
    const BYTE* GetMACAddress()const override {return MACAddress;}
    int Open()override;
    int Close()override;
    BOOL HasInterrupt()override;
    void ClearInterrupt()override;
    int Transmit(EthernetFrame& Frame)override;
    int Receive(EthernetFrame* FrameBuffer)override;

    // NetworkAdapterBase abstract class
    void LoadMACAddress()override;

    // Getters and Setters
    RegisterValueType GetRegister(RegisterAddrType Register)const;
    void SetRegister(RegisterAddrType Register, RegisterValueType Value)const;

    // Member-functions
    void Reset();
    void EnableAutoSpeed();
    void DisableAutoSpeed();
    void SetupPacketTransmission();
    void SetupPacketReception();
    void EnableTransmission();
    void EnableReception();
    void DisableTransmission();
    void DisableReception();
    void AllocateReceiveDescrBuffer();
    void EnableInterrupts();
    void DisableInterrupts();
    void InitMulticastTableArray();

    void RegisterInterruptHandler();

    // Static member functions
    static BOOL Detect(PCIFuncCPointer PCIFunction);
    static NetworkAdapter* Start(PCIFuncCPointer PCIFunction);
};

/*class RealtekRTL8139NetworkAdapter final : public NetworkAdapter
{
    SINGLE_INSTANCE(RealtekRTL8139NetworkAdapter)
};*/

/*class IntelAX201NetworkAdapter final : public NetworkAdapter
{
    SINGLE_INSTANCE(IntelAX201NetworkAdapter)
};*/

using NetworkAdapterListType = NetworkAdapter*;

extern int NetworkAdapterListSize;
static const int NetworkAdapterListFullSize = 4096 / sizeof(NetworkAdapter);
extern NetworkAdapterListType* NetworkAdapterList;

#endif

#endif // UNETWORK_H
