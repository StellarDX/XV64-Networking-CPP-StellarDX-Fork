// Routines to let C code use special amd64 instructions.
// See: http://ref.x86asm.net/coder64.html

static inline uchar
inb(ushort port)
{
  uchar data;

  asm volatile("in %1,%0" : "=a" (data) : "d" (port));
  return data;
}

static inline void
amd64_nop(){
    asm volatile("nop");
}


#define amd64_pause() asm volatile("pause")

#define amd64_mem_barrier() asm volatile("lock; addl $0,0(%%rsp)" : : : "memory")

#define amd64_mem_read32(addr) (*(volatile uint32 *)(addr))
#define amd64_mem_read64(addr) (*(volatile uint64 *)(addr))

static volatile inline uint32
amd64_spinread32(uint64 baseAddr, uint32 offset){
    int16 total = 0;
    int16 same = 0;
    uint32 val;
    while(total < 1000 && same < 100){
        amd64_mem_barrier();
        uint32 newVal = amd64_mem_read32(baseAddr + offset);
        if(val == newVal){
            same++;
        }else{
            same = 0;
        }
        val = newVal;
        total++;
    }
    return val;
}

static volatile inline uint64
amd64_spinread64(volatile uint64 *baseAddr, uint32 offset){
    int16 total = 0;
    int16 same = 0;
    uint64 val;
    while(total < 1000 && same < 100){
        amd64_mem_barrier();
        uint64 newVal = amd64_mem_read32(baseAddr + offset);
        if(val == newVal){
            same++;
        }else{
            same = 0;
        }
        val = newVal;
        total++;
    }
    return val;
}

#define amd64_hlt() hlt()
#define amd64_cli() cli()

static inline void
insl(int port, void *addr, int cnt)
{
  asm volatile("cld; rep insl" :
               "=D" (addr), "=c" (cnt) :
               "d" (port), "0" (addr), "1" (cnt) :
               "memory", "cc");
}

static inline void amd64_out8(uint16 port, uint8 data) {
  asm volatile("out %0,%1" : : "a" (data), "d" (port));
}

static inline uint16 amd64_in16(uint16 port){
    uint16 result;
    asm volatile( "inw %1, %0"
                  : "=a"(result) : "Nd"(port) );
    return result;
}

static inline void amd64_out16(uint16 port, uint16 data) {
  asm volatile("out %0,%1" : : "a" (data), "d" (port));
}

static inline uint32 amd64_in32(uint16 port){
    uint32 result;
    asm volatile( "inl %1, %0"
                  : "=a"(result) : "Nd"(port) );
    return result;
}

static inline void amd64_out32(uint16 port, uint32 data){
    asm volatile("outl %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint8 amd64_in8(uint16 port){
    uint8 result;
    asm volatile( "inb %1, %0"
                  : "=a"(result) : "Nd"(port) );
    return result;
}


static inline void
outsl(int port, const void *addr, int cnt)
{
  asm volatile("cld; rep outsl" :
               "=S" (addr), "=c" (cnt) :
               "d" (port), "0" (addr), "1" (cnt) :
               "cc");
}

static inline void
stosb(void *addr, int data, int cnt)
{
  asm volatile("cld; rep stosb" :
               "=D" (addr), "=c" (cnt) :
               "0" (addr), "1" (cnt), "a" (data) :
               "memory", "cc");
}

static inline void
stosl(void *addr, int data, int cnt)
{
  asm volatile("cld; rep stosl" :
               "=D" (addr), "=c" (cnt) :
               "0" (addr), "1" (cnt), "a" (data) :
               "memory", "cc");
}

struct segdesc;

static inline void
lgdt(struct segdesc *p, int size)
{
  volatile ushort pd[5];

  pd[0] = size-1;
  pd[1] = (uintp)p;
  pd[2] = (uintp)p >> 16;
#if X64
  pd[3] = (uintp)p >> 32;
  pd[4] = (uintp)p >> 48;
#endif
  asm volatile("lgdt (%0)" : : "r" (pd));
}

struct gatedesc;

static inline void
lidt(struct gatedesc *p, int size)
{
  volatile ushort pd[5];

  pd[0] = size-1;
  pd[1] = (uintp)p;
  pd[2] = (uintp)p >> 16;
#if X64
  pd[3] = (uintp)p >> 32;
  pd[4] = (uintp)p >> 48;
#endif
  asm volatile("lidt (%0)" : : "r" (pd));
}

static inline void
ltr(ushort sel)
{
  asm volatile("ltr %0" : : "r" (sel));
}

static inline uintp
readeflags(void)
{
  uintp eflags;
  asm volatile("pushf; pop %0" : "=r" (eflags));
  return eflags;
}

static inline void
loadgs(ushort v)
{
  asm volatile("movw %0, %%gs" : : "r" (v));
}

static inline void
cli(void)
{
  asm volatile("cli");
}

static inline void
sti(void)
{
  asm volatile("sti");
}

static inline void
hlt(void)
{
  asm volatile("hlt");
}

static inline uint
xchg(volatile uint *addr, uintp newval)
{
  uint result;

  // The + in "+m" denotes a read-modify-write operand.
  asm volatile("lock; xchgl %0, %1" :
               "+m" (*addr), "=a" (result) :
               "1" (newval) :
               "cc");
  return result;
}

static inline uintp
rcr2(void)
{
  uintp val;
  asm volatile("mov %%cr2,%0" : "=r" (val));
  return val;
}

static inline void
lcr3(uintp val)
{
  asm volatile("mov %0,%%cr3" : : "r" (val));
}

static inline void
amd64_cpuid(uint ax, uint32 *p)
{
	asm volatile("cpuid"
			 : "=a" (p[0]), "=b" (p[1]), "=c" (p[2]), "=d" (p[3])
			 :  "0" (ax));
}

//PAGEBREAK: 36
// Layout of the trap frame built on the stack by the
// hardware and by trapasm.S, and passed to trap().
#if X64
// lie about some register names in 64bit mode to avoid
// clunky ifdefs in proc.c and trap.c.
struct trapframe {
  uint64 eax;      // rax
  uint64 rbx;
  uint64 rcx;
  uint64 rdx;
  uint64 rbp;
  uint64 rsi;
  uint64 rdi;
  uint64 r8;
  uint64 r9;
  uint64 r10;
  uint64 r11;
  uint64 r12;
  uint64 r13;
  uint64 r14;
  uint64 r15;

  uint64 trapno;
  uint64 err;

  uint64 eip;     // rip
  uint64 cs;
  uint64 eflags;  // rflags
  uint64 esp;     // rsp
  uint64 ds;      // ss
};
#else
struct trapframe {
  // registers as pushed by pusha
  uint edi;
  uint esi;
  uint ebp;
  uint oesp;      // useless & ignored
  uint ebx;
  uint edx;
  uint ecx;
  uint eax;

  // rest of trap frame
  ushort gs;
  ushort padding1;
  ushort fs;
  ushort padding2;
  ushort es;
  ushort padding3;
  ushort ds;
  ushort padding4;
  uint trapno;

  // below here defined by x86 hardware
  uint err;
  uint eip;
  ushort cs;
  ushort padding5;
  uint eflags;

  // below here only when crossing rings, such as from user to kernel
  uint esp;
  ushort ss;
  ushort padding6;
};
#endif
