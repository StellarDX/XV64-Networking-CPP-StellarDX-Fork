#include "UDef.hh"
#include "UNetworkAdapter.hh"
#include "UEtherFrame.hh"

_EXTERN_C
_ADD_PANIC
#include "assert.h"
_ADD_KERN_PRINT_FUNC
_ADD_KALLOC
_ADD_DELAY
_ADD_PICENABLE
_ADD_IOAPICENABLE
_END_EXTERN_C

struct NetworkAdapterMatchCase
{
    DWORD VendorID;
    DWORD DeviceID;
    BOOL (*Matcher)(NetworkAdapterBase::PCIFuncCPointer PCIFunction);
    NetworkAdapter* (*StartFunc)(NetworkAdapterBase::PCIFuncCPointer PCIFunction);
}NetworkAdapterMatches[]
{
    {
        .VendorID  = Intel8254xNetworkAdapter::Vendor,
        .DeviceID  = Intel8254xNetworkAdapter::Device,
        .Matcher   = Intel8254xNetworkAdapter::Detect,
        .StartFunc = Intel8254xNetworkAdapter::Start
    },
    {0, 0, nullptr, nullptr} // End iterator
};

int NetworkAdapterListSize = 0;
NetworkAdapter** NetworkAdapterList = nullptr;

// ------------- Member functions of NetworkAdapterBase ------------- //

void NetworkAdapterBase::LoadFromPCI(PCIFuncCPointer PCIFunction)
{
    LoadBaseAddresses(PCIFunction);
    LoadInterruptRequests(PCIFunction);
}

void NetworkAdapterBase::LoadBaseAddresses(PCIFuncCPointer PCIFunction)
{
    cprintf((char*)"[NetworkAdapter] Resolving MMIO base address...\n");
    // 0: 0xFEBC0000 - 0x20000 -> MMIO
    // 1: 0x0000C000 - 0x00040 -> port
    // 2 - 5: all 0
    for (int i = 0; i < 6; ++i)
    {
        QWORD BaseAddress = PCIFunction->reg_base[i];
        DWORD Size = PCIFunction->reg_size[i];
        cprintf((char*)"[NetworkAdapter] Scanning: %x(%x)\n",
                BaseAddress, Size);
        // IO port number are 16-bits
        if (BaseAddress <= 0xFFFF)
        {
            assert(Size == 64);
            PortBaseAddress = PortBaseAddrType(BaseAddress);
            break;
        }
        else if (BaseAddress > 0)
        {
            assert(Size == (1 << 17));
            MMIOBaseAddress = MMIOBaseAddrType(BaseAddress);
            continue;
        }
    }
    assert(PortBaseAddress);
    assert(MMIOBaseAddress);
    cprintf((char*)"[NetworkAdapter] "
        "Base addresses is located on: Port: %x, MMIO: %x\n",
        PortBaseAddress, MMIOBaseAddress);
}

void NetworkAdapterBase::LoadInterruptRequests(PCIFuncCPointer PCIFunction)
{
    this->IRQLine = PCIFunction->irq_line;
    cprintf((char*)"[NetworkAdapter] IRQ Line is located on: %d\n", this->IRQLine);
}

extern int ncpu;

void NetworkAdapterBase::RegisterIRQLine()
{
    picenable(IRQLine);
    ioapicenable(IRQLine, ncpu - 1);
}

// ---------- Member functions of Intel8254xNetworkAdapter ---------- //

int Intel8254xNetworkAdapter::Open()
{
    cprintf((char*)"[NetworkAdapter] Device starting...\n");
    EnableInterrupts();
    GetRegister(EthernetControllerRegisters::Interrupt::ICR);
    EnableTransmission();
    EnableReception();
    EnableAutoSpeed();
    cprintf((char*)"[NetworkAdapter] Device started.\n");
    return 0;
}

int Intel8254xNetworkAdapter::Close()
{
    cprintf((char*)"[NetworkAdapter] Device stopping...\n");
    DisableInterrupts();
    GetRegister(EthernetControllerRegisters::Interrupt::ICR);
    DisableTransmission();
    DisableReception();
    DisableAutoSpeed();
    cprintf((char*)"[NetworkAdapter] Device stopped.\n");
    return 0;
}

BOOL Intel8254xNetworkAdapter::HasInterrupt()
{
    return GetRegister(EthernetControllerRegisters::Interrupt::ICR)
        & InterruptMaskSetReadRegister::RXT0;
}

void Intel8254xNetworkAdapter::ClearInterrupt()
{
    GetRegister(EthernetControllerRegisters::Interrupt::ICR);
}

int Intel8254xNetworkAdapter::Transmit(EthernetFrame& Frame)
{
    DWORD TransmitTail = GetRegister(EthernetControllerRegisters::Transmit::TDT);
    TDescLayout[TransmitTail].BufferAddress = VirtualAddressToPhysical(Frame.Get());
    TDescLayout[TransmitTail].Length = Frame.Size();
    TDescLayout[TransmitTail].Status = 0;
    TDescLayout[TransmitTail].Command =
        TransmitDescriptorCommandField::EOP |
        TransmitDescriptorCommandField::RS;
    SetRegister(EthernetControllerRegisters::Transmit::TDT,
        (TransmitTail + 1) % TDescLayoutMaxSize);
    while (!(TDescLayout[TransmitTail].Status & 0xF)) {microdelay(10);}
    /*cprintf((char*)"[Intel8254xNetworkAdapter] Transmitted %d bytes data...\n",
        TDescLayout[TransmitTail].Length);*/
    return TDescLayout[TransmitTail].Length;
}

int Intel8254xNetworkAdapter::Receive(EthernetFrame* FrameBuffer)
{
    int BufferSize = 0;
    while (1)
    {
        DWORD ReceiveTail = GetRegister(EthernetControllerRegisters::Receive::RDT);
        //cprintf((char*)"[Intel8254xNetworkAdapter] Last index: %d.\n", ReceiveTail);
        ReceiveTail = (ReceiveTail + 1) % RDescLayoutMaxSize;
        if (!(RDescLayout[ReceiveTail].Status & ReceiveDescriptorStatusField::DD)) {break;}

        if (RDescLayout[ReceiveTail].Length < 60)
        {
            cprintf((char*)"[Intel8254xNetworkAdapter] Short packet (%d bytes).\n",
                RDescLayout[ReceiveTail].Length);
            return 0xBABABABA;
        }
        if (!(RDescLayout[ReceiveTail].Status & ReceiveDescriptorStatusField::EOP))
        {
            cprintf((char*)"[Intel8254xNetworkAdapter] NOT EOP!\n");
            return 0xBAADF00D;
        }
        if (RDescLayout[ReceiveTail].Errors)
        {
            cprintf((char*)"[Intel8254xNetworkAdapter] Error occoured in reception: 0x%x\n",
                RDescLayout[ReceiveTail].Errors);
            return -RDescLayout[ReceiveTail].Errors;
        }
        /*cprintf((char*)"[Intel8254xNetworkAdapter] %d bytes data received.\n",
            RDescLayout[ReceiveTail].Length);*/
        FrameBuffer[BufferSize] = EthernetFrame(
            PhysicalAddressToVirtual(RDescLayout[ReceiveTail].BufferAddress),
            RDescLayout[ReceiveTail].Length);
        ++BufferSize;

        RDescLayout[ReceiveTail].Status = 0;
        SetRegister(EthernetControllerRegisters::Receive::RDT, ReceiveTail);
    }
    //cprintf((char*)"[Intel8254xNetworkAdapter] DONE.\n");
    return BufferSize;
}

void Intel8254xNetworkAdapter::LoadMACAddress()
{
    cprintf((char*)"[Intel8254xNetworkAdapter] Loading MAC address...\n");
    RegisterValueType MACAddressLow =
        GetRegister(EthernetControllerRegisters::Receive::RAL0);
    RegisterValueType MACAddressHigh =
        GetRegister(EthernetControllerRegisters::Receive::RAH0);
    *(DWORD*)MACAddress = MACAddressLow;
    *(WORD*)(MACAddress + 4) = WORD(MACAddressHigh);
    cprintf((char*)"[Intel8254xNetworkAdapter] MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
        MACAddress[0], MACAddress[1], MACAddress[2],
        MACAddress[3], MACAddress[4], MACAddress[5]);
}

Intel8254xNetworkAdapter::RegisterValueType
Intel8254xNetworkAdapter::GetRegister(RegisterAddrType Register)const
{
    return *(Address64Type)MMIOToVirtualAddress(MMIOBaseAddress + Register);
}

// Datasheet 13.5.2
void Intel8254xNetworkAdapter::SetRegister(RegisterAddrType Register, RegisterValueType Value)const
{
    *(Address64Type)MMIOToVirtualAddress(MMIOBaseAddress + Register) = Value;
}

void Intel8254xNetworkAdapter::Reset()
{
    RegisterValueType CtrlParam =
        GetRegister(EthernetControllerRegisters::General::CTRL);
    CtrlParam |= DeviceControlRegister::RST;
    cprintf((char*)"[Intel8254xNetworkAdapter] Reseting device... (%x)\n", CtrlParam);
    SetRegister(EthernetControllerRegisters::General::CTRL, CtrlParam);
    bool IsReady = 0;
    while (!IsReady)
    {
        RegisterValueType FinalCtrlParam =
            GetRegister(EthernetControllerRegisters::General::CTRL);
        microdelay(10);
        IsReady = !(FinalCtrlParam & DeviceControlRegister::RST);
        if (IsReady)
        {
            cprintf((char*)"[Intel8254xNetworkAdapter] DONE(%x)\n", FinalCtrlParam);
        }
    }
}

// Datasheet 14.3
void Intel8254xNetworkAdapter::EnableAutoSpeed()
{
    RegisterValueType CtrlParam = GetRegister(EthernetControllerRegisters::General::CTRL);
    CtrlParam |= DeviceControlRegister::ASDE;
    CtrlParam |= DeviceControlRegister::SLU;
    SetRegister(EthernetControllerRegisters::General::CTRL, CtrlParam);
}

void Intel8254xNetworkAdapter::DisableAutoSpeed()
{
    RegisterValueType CtrlParam = GetRegister(EthernetControllerRegisters::General::CTRL);
    CtrlParam &= ~DeviceControlRegister::SLU;
    SetRegister(EthernetControllerRegisters::General::CTRL, CtrlParam);
}

void Intel8254xNetworkAdapter::SetupPacketTransmission()
{
    cprintf((char*)"[Intel8254xNetworkAdapter] Initializing packet transmission...\n");
    TDescLayout = decltype(TDescLayout)(kalloc()); // Extra-4KB space
    for (int i = 0; i < TDescLayoutMaxSize; ++i)
    {
        TDescLayout[i] = TransmitDescriptorLayout();
    }
    IntegerSplitter TAddress(VirtualAddressToPhysical(TDescLayout));
    SetRegister(EthernetControllerRegisters::Transmit::TDBAH, TAddress.Hi);
    SetRegister(EthernetControllerRegisters::Transmit::TDBAL, TAddress.Lo);
    SetRegister(EthernetControllerRegisters::Transmit::TDLEN,
        TDescLayoutMaxSize * sizeof(TransmitDescriptorLayout));
    SetRegister(EthernetControllerRegisters::Transmit::TDH, 0);
    SetRegister(EthernetControllerRegisters::Transmit::TDT, 0);
    RegisterValueType CtrlParams = GetRegister(EthernetControllerRegisters::Transmit::TCTL);
    CtrlParams |= TransmitControlRegister::PSP;
    SetRegister(EthernetControllerRegisters::Transmit::TCTL, CtrlParams);
    EnableTransmission();
}

void Intel8254xNetworkAdapter::SetupPacketReception()
{
    cprintf((char*)"[Intel8254xNetworkAdapter] Initializing packet reception...\n");
    AllocateReceiveDescrBuffer();
    IntegerSplitter TAddress(VirtualAddressToPhysical(RDescLayout));
    SetRegister(EthernetControllerRegisters::Receive::RDBAH, TAddress.Hi);
    SetRegister(EthernetControllerRegisters::Receive::RDBAL, TAddress.Lo);
    SetRegister(EthernetControllerRegisters::Receive::RDLEN,
        RDescLayoutMaxSize * sizeof(ReceiveDescriptorLayout));
    SetRegister(EthernetControllerRegisters::Receive::RDH, 0);
    SetRegister(EthernetControllerRegisters::Receive::RDT, RDescLayoutMaxSize - 1);
    RegisterValueType CtrlParams = GetRegister(EthernetControllerRegisters::Receive::RCTL);
    CtrlParams |= ReceiveControlRegister::SBP;
    CtrlParams |= ReceiveControlRegister::UPE;
    CtrlParams |= ReceiveControlRegister::MPE;
    CtrlParams |= ReceiveControlRegister::LPE;
    CtrlParams |= ReceiveControlRegister::RDMTSHalf;
    CtrlParams |= ReceiveControlRegister::BAM;
    CtrlParams |= ReceiveControlRegister::BSIZE2K;
    CtrlParams |= ReceiveControlRegister::SECRC;
    SetRegister(EthernetControllerRegisters::Receive::RCTL, CtrlParams);
    EnableReception();
}

void Intel8254xNetworkAdapter::EnableTransmission()
{
    RegisterValueType CtrlParams = GetRegister(EthernetControllerRegisters::Transmit::TCTL);
    CtrlParams |= TransmitControlRegister::EN;
    SetRegister(EthernetControllerRegisters::Transmit::TCTL, CtrlParams);
}

void Intel8254xNetworkAdapter::EnableReception()
{
    RegisterValueType CtrlParams = GetRegister(EthernetControllerRegisters::Receive::RCTL);
    CtrlParams |= ReceiveControlRegister::EN;
    SetRegister(EthernetControllerRegisters::Receive::RCTL, CtrlParams);
}

void Intel8254xNetworkAdapter::DisableTransmission()
{
    RegisterValueType CtrlParams = GetRegister(EthernetControllerRegisters::Transmit::TCTL);
    CtrlParams &= ~TransmitControlRegister::EN;
    SetRegister(EthernetControllerRegisters::Transmit::TCTL, CtrlParams);
}

void Intel8254xNetworkAdapter::DisableReception()
{
    RegisterValueType CtrlParams = GetRegister(EthernetControllerRegisters::Receive::RCTL);
    CtrlParams &= ~ReceiveControlRegister::EN;
    SetRegister(EthernetControllerRegisters::Receive::RCTL, CtrlParams);
}

void Intel8254xNetworkAdapter::AllocateReceiveDescrBuffer()
{
    RDescLayout = decltype(RDescLayout)(kalloc()); // Extra-4KB space
    for (int i = 0; i < RDescLayoutMaxSize; ++i)
    {
        RDescLayout[i] = ReceiveDescriptorLayout();
        // This is the eaziest allocation of buffer address. (By. PandaX381)
        // A more space-saving method is put 2 buffers in 1 page. (By. yas-nyan)
        RDescLayout[i].BufferAddress = VirtualAddressToPhysical(kalloc());
    }
}

void Intel8254xNetworkAdapter::EnableInterrupts()
{
    RegisterValueType InterruptMask = GetRegister(EthernetControllerRegisters::Interrupt::IMS);
    //InterruptMask |= InterruptMaskSetReadRegister::TXQE;
    //InterruptMask |= InterruptMaskSetReadRegister::RXSEQ;
    //InterruptMask |= InterruptMaskSetReadRegister::RXO;
    InterruptMask |= InterruptMaskSetReadRegister::RXT0;
    SetRegister(EthernetControllerRegisters::Interrupt::IMS, InterruptMask);
}

void Intel8254xNetworkAdapter::DisableInterrupts()
{
    RegisterValueType InterruptMask = GetRegister(EthernetControllerRegisters::Interrupt::IMC);
    InterruptMask |= InterruptMaskSetReadRegister::RXT0;
    SetRegister(EthernetControllerRegisters::Interrupt::IMC, InterruptMask);
}

void Intel8254xNetworkAdapter::InitMulticastTableArray()
{
    for (int i = 0; i < 128; ++i)
    {
        SetRegister(EthernetControllerRegisters::Receive::MTA + (i << 2), 0);
    }
}

void Intel8254xNetworkAdapter::RegisterInterruptHandler()
{
    cprintf((char*)"[Intel8254xNetworkAdapter] Registering interrupt handler...\n");
    EnableInterrupts();
    RegisterValueType InterruptMask = GetRegister(EthernetControllerRegisters::Interrupt::IMS);
    cprintf((char*)"[Intel8254xNetworkAdapter] Interrupt enabled mask: %x\n", InterruptMask);
    RegisterIRQLine();
    cprintf((char*)"[Intel8254xNetworkAdapter] DONE.\n");
}

BOOL Intel8254xNetworkAdapter::Detect(PCIFuncCPointer PCIFunction)
{
    WORD VendID = PCI_VENDOR(PCIFunction->dev_id);
    WORD DevID = PCI_PRODUCT(PCIFunction->dev_id);
    return VendID == Vendor && DevID == Device;
}

NetworkAdapter* Intel8254xNetworkAdapter::Start(PCIFuncCPointer PCIFunction)
{
    cprintf((char*)"[Intel8254xNetworkAdapter] Starting...\n");

    pci_func_enable(PCIFunction);
    Intel8254xNetworkAdapter* HInstance = new Intel8254xNetworkAdapter();
    HInstance->LoadFromPCI(PCIFunction);
    HInstance->Reset();
    HInstance->EnableAutoSpeed();
    HInstance->LoadMACAddress();
    HInstance->RegisterInterruptHandler();
    HInstance->InitMulticastTableArray();
    HInstance->SetupPacketTransmission();
    HInstance->SetupPacketReception();

    cprintf((char*)"[Intel8254xNetworkAdapter] Started.\n");
    return HInstance;
}

// ------------------------------------------------------------------ //

_EXTERN_C

void Intel8254xInterrupt()
{
    for (int i = 0; i < NetworkAdapterListSize; ++i)
    {
        auto Device = NetworkAdapterList[i];
        if (Device->VendorID() == Intel8254xNetworkAdapter::Vendor &&
            Device->DeviceID() == Intel8254xNetworkAdapter::Device)
        {
            if (Device->HasInterrupt())
            {
                EtherFrameBufferCurrentSize = Device->Receive(GlobalEtherFrameBuffer);
                Device->ClearInterrupt();
                if (EtherFrameBufferCurrentSize == 0xBABABABA ||
                    EtherFrameBufferCurrentSize == 0xBAADF00D ||
                    EtherFrameBufferCurrentSize < 0) {return;}
                FrameBufferHandler(Device, GlobalEtherFrameBuffer, EtherFrameBufferCurrentSize);
            }
        }
    }
}

int NetworkAdapterSetup(struct pci_func* PCIFunction)
{
    if (!NetworkAdapterList) {NetworkAdapterList = decltype(NetworkAdapterList)(kalloc());}
    cprintf((char*)"Loading network adapters...\n");
    NetworkAdapterMatchCase* Detector = NetworkAdapterMatches;
    while (Detector->Matcher && Detector->StartFunc)
    {
        if (Detector->Matcher(PCIFunction))
        {
            cprintf((char*)"Found network device: %x:%x\n",
                Detector->VendorID, Detector->DeviceID);
            NetworkAdapter* Adapter = Detector->StartFunc(PCIFunction);
            NetworkAdapterList[NetworkAdapterListSize] = Adapter;
            ++NetworkAdapterListSize;
        }
        ++Detector;
    }
    return 0;
}

_END_EXTERN_C
