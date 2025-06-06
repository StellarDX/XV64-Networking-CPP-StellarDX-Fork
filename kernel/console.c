// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include <stdarg.h>

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "vfs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "acpi.h"
#include "vga.h"
#include "vga_modes.h"
#include "irq.h"
#include "kernel/string.h"
#include "console.h"

static void consputc(int, uint32);

#define CGA_BLACK         0x0
#define CGA_BLUE          0x1
#define CGA_GREEN         0x2
#define CGA_CYAN          0x3
#define CGA_RED           0x4
#define CGA_MAGENTA       0x5
#define CGA_BROWN         0x6
#define CGA_LIGHT_GRAY    0x7
#define CGA_DARK_GRAY     0x8
#define CGA_LIGHT_BLUE    0x9
#define CGA_LIGHT_GREEN   0xA
#define CGA_LIGHT_CYAN    0xB
#define CGA_LIGHT_RED     0xC
#define CGA_LIGHT_MAGENTA 0xD
#define CGA_YELLOW        0xE
#define CGA_WHITE         0xF

#define VGA_BLACK         0x0
#define VGA_BLUE          0x1
#define VGA_GREEN         0x2
#define VGA_CYAN          0x3
#define VGA_RED           0x4
#define VGA_MAGENTA       0x5
#define VGA_BROWN         0x14
#define VGA_LIGHT_GRAY    0x7
#define VGA_DARK_GRAY     0x38
#define VGA_LIGHT_BLUE    0x39
#define VGA_LIGHT_GREEN   0x3A
#define VGA_LIGHT_CYAN    0x3B
#define VGA_LIGHT_RED     0x3C
#define VGA_LIGHT_MAGENTA 0x3D
#define VGA_YELLOW        0x3E
#define VGA_WHITE         0x3F

#define TAB_SIZE 8

#define CGA_FONT_COLOR(foreground, background) ((background << 4) + foreground)
#ifdef VGA_GRAPHICS
#define DEFAULT_CONSOLE_COLOR CGA_FONT_COLOR(VGA_LIGHT_GRAY, VGA_BLACK)
#else
#define DEFAULT_CONSOLE_COLOR CGA_FONT_COLOR(CGA_LIGHT_GRAY, CGA_BLACK)
#endif
#define CGA_GET_FONT_BACKGROUND_COLOR(color) ((color & 0xFF00) >> 4)

#define COLUMNS 80

static int panicked = 0;

static struct {
	struct spinlock lock;
	int locking;
} cons;

//static char digits[] = "0123456789abcdef";

/*static void printptr(uintp x, uint32 color) {
	int i;
	for (i = 0; i < (sizeof(uintp) * 2); i++, x <<= 4)
		consputc(digits[x >> (sizeof(uintp) * 8 - 4)], color);
}*/

/*static void printint(int xx, int base, int sign, uint32 color){
	char buf[16];
	int i;
	uint x;

	if (sign && (sign = xx < 0))
		x = -xx;
	else
		x = xx;

	i = 0;
	do {
		buf[i++] = digits[x % base];
	} while ((x /= base) != 0);

	if (sign)
		buf[i++] = '-';

	while (--i >= 0)
		consputc(buf[i], color);
}*/

// ---------- Printf by PandaX ----------

static void putch(int ch, int *cnt)
{
    uint32 color = DEFAULT_CONSOLE_COLOR;
    consputc(ch, color);
    (void)*cnt++;
}

/*
 * Print a number (base <= 16) in reverse order,
 * using specified putch function and associated pointer putdat.
 */
static void
printnum(void (*putch)(int, void*), void *putdat,
         unsigned long long num, unsigned base, int width, int padc)
{
    // first recursively print all preceding (more significant) digits
    if (num >= base) {
        printnum(putch, putdat, num / base, base, width - 1, padc);
    } else {
        // print any needed pad characters before first digit
        while (--width > 0)
            putch(padc, putdat);
    }

    // then print this (the least significant) digit
    putch("0123456789abcdef"[num % base], putdat);
}

// Get an unsigned int of various possible sizes from a varargs list,
// depending on the lflag parameter.
/*static unsigned long long
getuint(va_list *ap, int lflag)
{
    if (lflag >= 2)
        return va_arg(*ap, unsigned long long);
    else if (lflag)
        return va_arg(*ap, unsigned long);
    else
        return va_arg(*ap, unsigned int);
}*/

// Same as getuint but signed - can't use getuint
// because of sign extension
/*static long long
getint(va_list *ap, int lflag)
{
    if (lflag >= 2)
        return va_arg(*ap, long long);
    else if (lflag)
        return va_arg(*ap, long);
    else
        return va_arg(*ap, int);
}*/

int strnlen(const char *s, int size)
{
    int n;

    for (n = 0; size > 0 && *s != '\0'; s++, size--)
        n++;
    return n;
}

void vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap)
{
    register const char *p;
    register int ch;//, err;
    unsigned long long num;
    int base, lflag, width, precision, altflag;
    char padc;

    while (1) {
        while ((ch = *(unsigned char *) fmt++) != '%') {
            if (ch == '\0')
                return;
            putch(ch, putdat);
        }

        // Process a %-escape sequence
        padc = ' ';
        width = -1;
        precision = -1;
        lflag = 0;
        altflag = 0;
    reswitch:
        switch (ch = *(unsigned char *) fmt++) {

        // flag to pad on the right
        case '-':
            padc = '-';
            goto reswitch;

            // flag to pad with 0's instead of spaces
        case '0':
            padc = '0';
            goto reswitch;

            // width field
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            for (precision = 0; ; ++fmt) {
                precision = precision * 10 + ch - '0';
                ch = *fmt;
                if (ch < '0' || ch > '9')
                    break;
            }
            goto process_precision;

        case '*':
            precision = va_arg(ap, int);
            goto process_precision;

        case '.':
            if (width < 0)
                width = 0;
            goto reswitch;

        case '#':
            altflag = 1;
            goto reswitch;

        process_precision:
            if (width < 0)
                width = precision, precision = -1;
            goto reswitch;

            // long flag (doubled for long long)
        case 'l':
            lflag++;
            goto reswitch;

            // character
        case 'c':
            putch(va_arg(ap, int), putdat);
            break;

            // error message
        /*case 'e':
            err = va_arg(ap, int);
            if (err < 0)
                err = -err;
            if (err >= MAXERROR || (p = error_string[err]) == NULL)
                printfmt(putch, putdat, "error %d", err);
            else
                printfmt(putch, putdat, "%s", p);
            break;*/

            // string
        case 's':
            if (!(p = va_arg(ap, char *)))
                p = "(null)";
            if (width > 0 && padc != '-')
                for (width -= strnlen(p, precision); width > 0; width--)
                    putch(padc, putdat);
            for (; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--)
                if (altflag && (ch < ' ' || ch > '~'))
                    putch('?', putdat);
                else
                    putch(ch, putdat);
            for (; width > 0; width--)
                putch(' ', putdat);
            break;

            // (signed) decimal
        case 'd':
            //num = getint(&ap, lflag);
            if (lflag >= 2)
                num = va_arg(ap, long long);
            else if (lflag)
                num =  va_arg(ap, long);
            else
                num =  va_arg(ap, int);
            if ((long long) num < 0) {
                putch('-', putdat);
                num = -(long long) num;
            }
            base = 10;
            goto number;

            // unsigned decimal
        case 'u':
            //num = getuint(&ap, lflag);
            if (lflag >= 2)
                num = va_arg(ap, unsigned long long);
            else if (lflag)
                num = va_arg(ap, unsigned long);
            else
                num = va_arg(ap, unsigned int);
            base = 10;
            goto number;

            // (unsigned) octal
        case 'o':
            // Replace this with your code.
            putch('X', putdat);
            putch('X', putdat);
            putch('X', putdat);
            break;

            // pointer
        case 'p':
            putch('0', putdat);
            putch('x', putdat);
            num = (unsigned long long)
                (uintp) va_arg(ap, void *);
            base = 16;
            goto number;

            // (unsigned) hexadecimal
        case 'x':
            //num = getuint(&ap, lflag);
            if (lflag >= 2)
                num = va_arg(ap, unsigned long long);
            else if (lflag)
                num = va_arg(ap, unsigned long);
            else
                num = va_arg(ap, unsigned int);
            base = 16;
        number:
            printnum(putch, putdat, num, base, width, padc);
            break;

            // escaped '%' character
        case '%':
            putch(ch, putdat);
            break;

            // unrecognized escape sequence - just print it literally
        default:
            putch('%', putdat);
            for (fmt--; fmt[-1] != '%'; fmt--)
                /* do nothing */;
            break;
        }
    }
}

static int vcprintf(const char *fmt, va_list ap)
{
    int cnt = 0;
    vprintfmt((void*)putch, &cnt, fmt, ap);
    return cnt;
}

void cprintf(char* fmt, ...){
	va_list ap;
    int /*i, c,*/ locking;
    //char* s;

	locking = cons.locking;
	if (locking)
		acquire(&cons.lock);
/*
	if (fmt == 0)
		panic("null fmt");

	uint32 color = DEFAULT_CONSOLE_COLOR;
	for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
		if (c != '%') {
			consputc(c, color);
			continue;
		}
		c = fmt[++i] & 0xff;
		if (c == 0)
			break;
		switch (c) {
		case 'd':
			printint(va_arg(ap, int), 10, 1, color);
			break;
		case 'x':
			printint(va_arg(ap, int), 16, 0, color);
			break;
		case 'p':
			printptr(va_arg(ap, uintp), color);
			break;
		case 's':
			if ((s = va_arg(ap, char*)) == 0)
				s = "(null)";
			for (; *s; s++)
				consputc(*s, color);
			break;
		case '%':
			consputc('%', color);
			break;
		default:
			// Print unknown % sequence to draw attention.
			consputc('%', color);
			consputc(c, color);
			break;
		}
	}
*/
    va_start(ap, fmt);
    vcprintf(fmt, ap);
    va_end(ap);

	if (locking)
		release(&cons.lock);
}

void panic(char* s){
	uintp pcs[10];

	cli();
	cons.locking = 0;
	cprintf("\n\nPANIC on cpu %d\n ", cpu->id);
	cprintf(s);
	if(cpu->proc) {
		cprintf("\nPROC: %s\n", cpu->proc->name);
		cprintf("\tlast sys call: %d\n", cpu->proc->lastsyscall);
	}
	cprintf("\nSTACK:\n");
	getcallerpcs(&s, pcs);
	for (int i = 0; i < 10 && pcs[i] != 0x0; i++) {
		cprintf(" [%d] %p\n",i, pcs[i]);
	}
	cprintf("HLT\n");
	halt();
}

void halt() {
	panicked = 1; // freeze other CPU
    //acpi_halt();
	for (;;)
		;
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static uint16* crt = (uint16*)P2V(VGA_TEXT_MEM);

#ifdef VGA_GRAPHICS
static void console_setbackgroundcolor(uint32 color){
	memset(crt, color, 0xFA00);
}
#endif

static void cgaputc(int c, uint32 color){
	int pos;

	// Cursor position: col + 80*row.
	amd64_out8(CRTPORT, 14);
	pos = inb(CRTPORT + 1) << 8;
	amd64_out8(CRTPORT, 15);
	pos |= inb(CRTPORT + 1);

	if (c == '\n') {
		pos += COLUMNS - pos % COLUMNS;
	} else if (c == BACKSPACE) {
		if (pos > 0) --pos;
	} else if (c == '\t') {
		int offset = TAB_SIZE - (pos % TAB_SIZE);
		pos += offset;
	} else {
		crt[pos++] = (c & 0xFF) | (color << 8);
	}

	if ((pos / COLUMNS) >= 24) { // Scroll up.
		memmove(crt, crt + COLUMNS, sizeof(crt[0]) * 23 * COLUMNS);
		pos -= COLUMNS;
		memset(crt + pos, 0, sizeof(crt[0]) * (24 * COLUMNS - pos));
	}

	amd64_out8(CRTPORT, 14);
	amd64_out8(CRTPORT + 1, pos >> 8);
	amd64_out8(CRTPORT, 15);
	amd64_out8(CRTPORT + 1, pos);
	crt[pos] = ' ' | 0x0700;
}

static void consputc(int c, uint32 color){
	if (panicked) {
		cli();
		for (;;)
			;
	}

	if (c == BACKSPACE) {
		uartputc('\b');
		uartputc(' ');
		uartputc('\b');
	} else {
		uartputc(c);
	}
	cgaputc(c, color);
}

#define INPUT_BUF 128
struct {
	struct spinlock lock;
	char buf[INPUT_BUF];
	uint r; // Read index
	uint w; // Write index
	uint e; // Edit index
} input;

#define C(x)  ((x) - '@')  // Control-x

void consoleintr(int (*getc)(void)){
	int c;

	acquire(&input.lock);
	while ((c = getc()) >= 0) {
		switch (c) {
		case C('Z'): // reboot
			lidt(0, 0);
			break;
		case C('Q'): // halt
			halt();
			break;
		case C('P'): // Process listing.
			procdump();
			break;
		case C('U'): // Kill line.
			while (input.e != input.w &&
			       input.buf[(input.e - 1) % INPUT_BUF] != '\n') {
				input.e--;
				consputc(BACKSPACE, DEFAULT_CONSOLE_COLOR);
			}
			break;
		case C('H'): case '\x7f': // Backspace
			if (input.e != input.w) {
				input.e--;
				consputc(BACKSPACE, DEFAULT_CONSOLE_COLOR);
			}
			break;
		default:
			if (c != 0 && input.e - input.r < INPUT_BUF) {
				c = (c == '\r') ? '\n' : c;
				input.buf[input.e++ % INPUT_BUF] = c;
				consputc(c, DEFAULT_CONSOLE_COLOR);
				if (c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF) {
					input.w = input.e;
					wakeup(&input.r);
				}
			}
			break;
		}
	}
	release(&input.lock);
}

int consoleread(struct inode* ip, char* dst, int n){
	uint target;
	int c;

	iunlock(ip);
	target = n;
	acquire(&input.lock);
	while (n > 0) {
		while (input.r == input.w) {
			if (proc->killed) {
				release(&input.lock);
				ilock(ip);
				return -1;
			}
			sleep(&input.r, &input.lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if (c == C('D')) { // EOF
			if (n < target) {
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if (c == '\n')
			break;
	}
	release(&input.lock);
	ilock(ip);

	return target - n;
}

int consolewrite(struct inode* ip, char* buf, int n){
	int i;

	iunlock(ip);
	acquire(&cons.lock);
	for (i = 0; i < n; i++)
		consputc(buf[i] & 0xff, DEFAULT_CONSOLE_COLOR);
	release(&cons.lock);
	ilock(ip);

	return n;
}


#ifdef VGA_GRAPHICS
static void vga_init(){
	uint8 registers[] = VGA_80X25_TEXT_MODE;
	vga_write_regs(registers);


	uint8 font[VGA_8X16_FONT_SIZE] = VGA_8X16_FONT;
	for (uint16 i = 0; i < VGA_8X16_FONT_SIZE; i += 16) {
		for (uint16 j = 0; j < 16; j++) {
			((char *) KERNBASE + 0xa0000)[2*i+j] = font[i+j];
		}
	}
}
#endif

void sys_kconsole_info(struct winsize *winsz, struct termios *termios) {
	//TODO: when we support more resolutions, etc.,
	//update this method accordingly.
	if(winsz != 0) {
		winsz->ws_row = 80;
		winsz->ws_col = 25;
		winsz->ws_xpixel = 640;
		winsz->ws_ypixel = 200;
	}
	if(termios != 0) {
		termios->c_iflag = 0;
		termios->c_oflag = 0;
		termios->c_cflag = 0;
		termios->c_lflag = 0;
		//???
	}
}

void consoleinit(void){
	initlock(&cons.lock, "console");
	initlock(&input.lock, "input");

	devsw[TTY0].write = consolewrite;
	devsw[TTY0].read = consoleread;
	cons.locking = 1;

	picenable(IRQ_KBD);
	ioapicenable(IRQ_KBD, 0);

	uint32 backColor = CGA_GET_FONT_BACKGROUND_COLOR(DEFAULT_CONSOLE_COLOR);
    #ifdef VGA_GRAPHICS
	vga_init();
	console_setbackgroundcolor(VGA_BLACK);
	cprintf("VGA ");
	consputc('C', CGA_FONT_COLOR(VGA_RED, backColor));
	consputc('O', CGA_FONT_COLOR(VGA_MAGENTA, backColor));
	consputc('L', CGA_FONT_COLOR(VGA_LIGHT_GREEN, backColor));
	consputc('O', CGA_FONT_COLOR(VGA_YELLOW, backColor));
	consputc('R', CGA_FONT_COLOR(VGA_GREEN, backColor));
    #else
	cprintf("CGA ");
	consputc('C', CGA_FONT_COLOR(CGA_RED, backColor));
	consputc('O', CGA_FONT_COLOR(CGA_MAGENTA, backColor));
	consputc('L', CGA_FONT_COLOR(CGA_LIGHT_GREEN, backColor));
	consputc('O', CGA_FONT_COLOR(CGA_YELLOW, backColor));
	consputc('R', CGA_FONT_COLOR(CGA_GREEN, backColor));
    #endif

	cprintf(" Console\n");
}
