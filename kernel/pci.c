#include "types.h"
#include "defs.h"
#include "x86.h"
#include "pci.h"
#include "assert.h"
#include "ahci.h"
#include "kernel/string.h"

// StellarDX Headers
#include "UNetworkAdapter.hh"

#define	PCI_CLASS_BRIDGE 0x06
#define	PCI_SUBCLASS_BRIDGE_PCI 0x04


#define ARRAY_SIZE(a)   (sizeof(a) / sizeof(a[0]))

// Flag to do "lspci" at bootup
static int pci_show_devs = 1;
static int pci_show_addrs = 1;

// PCI "configuration mechanism one"
static uint32 pci_conf1_addr_ioport = 0x0cf8;
static uint32 pci_conf1_data_ioport = 0x0cfc;

// Forward declarations
static int pci_bridge_attach(struct pci_func* pcif);

// PCI driver table
struct pci_driver {
	uint32 key1, key2;
	int (* attachfn) (struct pci_func* pcif);
};

// pci_attach_class matches the class and subclass of a PCI device
struct pci_driver pci_attach_class[] = {
	{ PCI_DEV_CLASS_BRIDGE, PCI_SUBCLASS_BRIDGE_PCI, &pci_bridge_attach },
	{ 0, 0, 0 },
};

// pci_attach_vendor matches the vendor ID and device ID of a PCI device. key1
// and key2 should be the vendor ID and device ID respectively
struct pci_driver pci_attach_vendor[] = {
	{ 0, 0, 0 },
};

static void pci_conf1_set_addr(uint32 bus, uint32 dev, uint32 func, uint32 offset){
	assert(bus < 256);
	assert(dev < 32);
	assert(func < 8);
	assert(offset < 256);
	assert((offset & 0x3) == 0);

	uint32 v = (1 << 31) |  // config-space
	           (bus << 16) | (dev << 11) | (func << 8) | (offset);
	amd64_out32(pci_conf1_addr_ioport, v);
}

static uint32 pci_conf_read(struct pci_func* f, uint32 off){
	pci_conf1_set_addr(f->bus->busno, f->dev, f->func, off);
	return amd64_in32(pci_conf1_data_ioport);
}

static void pci_conf_write(struct pci_func* f, uint32 off, uint32 v){
	pci_conf1_set_addr(f->bus->busno, f->dev, f->func, off);
	amd64_out32(pci_conf1_data_ioport, v);
}

static int pci_attach_match(uint32 key1, uint32 key2, struct pci_driver* list, struct pci_func* pcif){
	uint32 i;

	for (i = 0; list[i].attachfn; i++) {
		if (list[i].key1 == key1 && list[i].key2 == key2) {
			int r = list[i].attachfn(pcif);
			if (r > 0)
				return r;
			if (r < 0)
				cprintf("pci_attach_match: attaching "
				        "%x.%x (%p): e\n",
				        key1, key2, list[i].attachfn, r);
		}
	}
	return 0;
}

static void pci_attach_storage_dev(struct pci_func* f){
	ahci_try_setup_device(f->bus->busno, f->dev, f->func);
}

static int pci_fallback_attach(struct pci_func* f){ //TODO: remove in favor of dev class specific functions
	return
	        pci_attach_match(PCI_CLASS(f->dev_class),
	                         PCI_SUBCLASS(f->dev_class),
	                         &pci_attach_class[0], f) ||
	        pci_attach_match(PCI_VENDOR(f->dev_id),
	                         PCI_PRODUCT(f->dev_id),
	                         &pci_attach_vendor[0], f);
}

static void pci_try_attach(struct pci_func *f){
	uint16 devClass = PCI_CLASS(f->dev_class);
	switch(devClass) {
	case PCI_DEV_CLASS_STORAGE:
		pci_attach_storage_dev(f);
		break;
	case PCI_DEV_CLASS_BRIDGE:
		pci_attach_storage_dev(f);
		break;
    case PCI_DEV_CLASS_NETWORKING:
        NetworkAdapterSetup(f);
        break;
	default:
		//non-supported/unknown device
		pci_fallback_attach(f);         //one last ditch attempt
		break;
	}
}

static const char* pci_class[] =
{
	[0x0] = "Unknown",
	[PCI_DEV_CLASS_STORAGE]       = "Storage controller",
	[PCI_DEV_CLASS_NETWORKING]    = "Network controller",
	[PCI_DEV_CLASS_DISPLAY]       = "Display controller",
	[PCI_DEV_CLASS_MULTIMEDIA]    = "Multimedia device",
	[PCI_DEV_CLASS_MEMCONTROLLER] = "Memory controller",
	[PCI_DEV_CLASS_BRIDGE]        = "Bridge device",
};

static void pci_print_func(struct pci_func* f){
	const char* class = pci_class[0];
	uint32 bar5addr = 0x0;
	if (PCI_CLASS(f->dev_class) < ARRAY_SIZE(pci_class)) {
		class = pci_class[PCI_CLASS(f->dev_class)];
		if(PCI_CLASS(f->dev_class) == 0x1) {
			bar5addr = pci_conf_read(f, 0x24);
		}
	}

	cprintf("PCI: %x:%x.%d: %x:%x: class: %x.%x ",
	        f->bus->busno, f->dev, f->func,
	        PCI_VENDOR(f->dev_id), PCI_PRODUCT(f->dev_id),
	        PCI_CLASS(f->dev_class), PCI_SUBCLASS(f->dev_class));
	cprintf("(%s) irq: %d", class, f->irq_line);
	if(bar5addr != 0x0) {
		cprintf(" bar5: %x", bar5addr);
	}
	cprintf("\n");
}

static int pci_scan_bus(struct pci_bus* bus){
	int totaldev = 0;
	struct pci_func df;
	memset(&df, 0, sizeof(df));
	df.bus = bus;

	for (df.dev = 0; df.dev < 32; df.dev++) {
		uint32 bhlc = pci_conf_read(&df, PCI_BHLC_REG);
		if (PCI_HDRTYPE_TYPE(bhlc) > 1) // Unsupported or no device
			continue;

		totaldev++;

		struct pci_func f = df;
		for (f.func = 0; f.func < (PCI_HDRTYPE_MULTIFN(bhlc) ? 8 : 1);
		     f.func++) {
			struct pci_func af = f;

			af.dev_id = pci_conf_read(&f, PCI_ID_REG);
			if (PCI_VENDOR(af.dev_id) == 0xffff)
				continue;

			uint32 intr = pci_conf_read(&af, PCI_INTERRUPT_REG);
			af.irq_line = PCI_INTERRUPT_LINE(intr);

			af.dev_class = pci_conf_read(&af, PCI_CLASS_REG);
			if (pci_show_devs)
				pci_print_func(&af);
			pci_try_attach(&af);
		}
	}

	return totaldev;
}

static int pci_bridge_attach(struct pci_func* pcif){
	uint32 ioreg = pci_conf_read(pcif, PCI_BRIDGE_STATIO_REG);
	uint32 busreg = pci_conf_read(pcif, PCI_BRIDGE_BUS_REG);

	if (PCI_BRIDGE_IO_32BITS(ioreg)) {
		cprintf("PCI: %02x:%02x.%d: 32-bit bridge IO not supported.\n",
		        pcif->bus->busno, pcif->dev, pcif->func);
		return 0;
	}

	struct pci_bus nbus;
	memset(&nbus, 0, sizeof(nbus));
	nbus.parent_bridge = pcif;
	nbus.busno = (busreg >> PCI_BRIDGE_BUS_SECONDARY_SHIFT) & 0xff;

	if (pci_show_devs)
		cprintf("PCI: %02x:%02x.%d: bridge to PCI bus %d--%d\n",
		        pcif->bus->busno, pcif->dev, pcif->func,
		        nbus.busno,
		        (busreg >> PCI_BRIDGE_BUS_SUBORDINATE_SHIFT) & 0xff);

	pci_scan_bus(&nbus);
	return 1;
}

// External PCI subsystem interface

void pci_func_enable(struct pci_func* f){
	pci_conf_write(f, PCI_COMMAND_STATUS_REG,
	               PCI_CMD_IO_ENABLE |
	               PCI_CMD_MEM_ENABLE |
	               PCI_CMD_MASTER_ENABLE);

	uint32 bar_width;
	uint32 bar;
	for (bar = PCI_BAR0_OFFSET; bar <= PCI_BAR5_OFFSET; bar += bar_width) {
		uint32 oldv = pci_conf_read(f, bar);

		bar_width = 4;
		pci_conf_write(f, bar, 0xffffffff);
		uint32 rv = pci_conf_read(f, bar);

		if (rv == 0)
			continue;

		int regnum = PCI_MAPREG_NUM(bar);
		uint32 base, size;
		if (PCI_MAPREG_TYPE(rv) == PCI_MAPREG_TYPE_MEM) {
			if (PCI_MAPREG_MEM_TYPE(rv) == PCI_MAPREG_MEM_TYPE_64BIT)
				bar_width = 8;

			size = PCI_MAPREG_MEM_SIZE(rv);
			base = PCI_MAPREG_MEM_ADDR(oldv);
			if (pci_show_addrs)
				cprintf("  mem region %d: %d bytes at 0x%x\n",
				        regnum, size, base);
		} else {
			size = PCI_MAPREG_IO_SIZE(rv);
			base = PCI_MAPREG_IO_ADDR(oldv);
			if (pci_show_addrs)
				cprintf("  io region %d: %d bytes at 0x%x\n",
				        regnum, size, base);
		}

		pci_conf_write(f, bar, oldv);
		f->reg_base[regnum] = base;
		f->reg_size[regnum] = size;

		if (size && !base)
			cprintf("PCI device %02x:%02x.%d (%04x:%04x) "
			        "may be misconfigured: "
			        "region %d: base 0x%x, size %d\n",
			        f->bus->busno, f->dev, f->func,
			        PCI_VENDOR(f->dev_id), PCI_PRODUCT(f->dev_id),
			        regnum, base, size);
	}

    cprintf("PCI function %x:%x.%d (%x:%x) enabled\n",
	        f->bus->busno, f->dev, f->func,
	        PCI_VENDOR(f->dev_id), PCI_PRODUCT(f->dev_id));
}

static int pci_init(void){
	static struct pci_bus root_bus;
	memset(&root_bus, 0, sizeof(root_bus));

	return pci_scan_bus(&root_bus);
}

void pciinit(void) {
	cprintf("probing PCI...\n");
    (void)pci_init();
}
