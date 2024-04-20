/* MOS 6502 Emulator
 *
 * Written by Stephen Sykes
 * Winner of Best Emulator IOCCC 2005
 * https://www.ioccc.org/years.html#2005_sykes
 *
 * Deobfuscated by Ed Spittles (2014) and Ivo van Poorten (2024)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curses.h>
#include <time.h>

int d;                      // program counter
int C, Z, I, D, B, V, S;    // flags
unsigned char a, x, y, k;   // registers, k=sp

int i = 0xc000;     // fetched instruction
int n = 0xffff;     // address mask, to keep index into memory sane 
int l = 0x80;       // sign mask
int f = 0xff;       // LSB mask
int O = 0x0100;     // one page, also stack base
int e;              // effective address
int w;              // inverse PET flag, w==0 means PET (and has PIA1)
int t;              // global temporary variable
int h;              // key press
int z;              // inverse monitor flag, if 0, set diagnostic sense
int s;              // another temporary, keyb. rows, load file, write screen
int o;              // instruction counter
unsigned char *p, m[65536], *u;

// Convert pressed key to PET scan code
unsigned char *j =
" ./  p/ 7 ] . 6 6 p     t7      r(0)1*+2,4WgcovGn^f_NVO>F?T\\swldiHZYI9QJ"
"RCKSL[b<D8AP:;a@`BXq3j=- HZYI9QJRCKSL[b<D8AP:;a@`BX   57  ;  ;      ;   ;   "
"     ;                                                                      "
"                                ";

unsigned char insns[256] = {
     6, 25, -1, -1, -1, 25,  2, -1, 27, 25,  2, -1, -1, 25,  2, -1,
     4, 25, -1, -1, -1, 25,  2, -1,  8, 25, -1, -1, -1, 25,  2, -1,
    20,  1, -1, -1,  5,  1, 30, -1, 29,  1, 30, -1,  5,  1, 30, -1,
     3,  1, -1, -1, -1,  1, 30, -1,  7,  1, -1, -1, -1,  1, 30, -1,
    32, 15, -1, -1, -1, 15, 24, -1, 26, 15, 24, -1, 19, 15, 24, -1,
     4, 15, -1, -1, -1, 15, 24, -1,  8, 15, -1, -1, -1, 15, 24, -1,
    33,  0, -1, -1, -1,  0, 31, -1, 28,  0, 31, -1, 19,  0, 31, -1,
     3,  0, -1, -1, -1,  0, 31, -1,  7,  0, -1, -1, -1,  0, 31, -1,
    -1, 35, -1, -1, 37, 35, 36, -1, 14, -1, 41, -1, 37, 35, 36, -1,
     4, 35, -1, -1, 37, 35, 36, -1, 43, 35, 42, -1, -1, 35, -1, -1,
    23, 21, 22, -1, 23, 21, 22, -1, 39, 21, 38, -1, 23, 21, 22, -1,
     3, 21, -1, -1, 23, 21, 22, -1,  8, 21, 40, -1, 23, 21, 22, -1,
    11,  9, -1, -1, 11,  9, 12, -1, 18,  9, 13, -1, 11,  9, 12, -1,
     4,  9, -1, -1, -1,  9, 12, -1,  8,  9, -1, -1, -1,  9, 12, -1,
    10, 34, -1, -1, 10, 34, 16, -1, 17, 34, -1, -1, 10, 34, 16, -1,
     3, 34, -1, -1, -1, 34, 16, -1,  7, 34, -1, -1, -1, 34, 16, -1,
};

// calculate Zero, and Sign flags
void R(int x) {
    Z = x ? 0 : 2;
    S = x & 0x80;
}

// compare x with *p (value) and calculate  Zero, Sign, and Carry flags
void K(int x) {
    R(x - *p);
    C = x < *p ? 0 : 1;
}

// pull P from stack and set registers
void A(void) {
    t = m[n & ++k + O];
    C = t & 1;
    Z = t & 2;
    I = t & 4;
    D = t & 8;
    B = 0;          /* bugfix: we model B as always zero so IRQ is detected as such by the handler */
    V = t & 64;
    S = t & 128;
}

// push program counter d to stack
void X(void) {
    m[n & k-- + O] = d / O;
    m[n & k-- + O] = d;
}

#define c argc
#define v argv
#define g filehandle

int visited[0x10000];
int ttt;                        /* instructions executed */
int stderrlines;

int N(void) {
#ifdef NOISY
    fprintf(stderr, "\r\nhelper N for BRK/IRQ at time %08d and PC %04x\r\n",
            ttt, d);
#endif
    X();                                                // push PC
    m[n & k-- + O] = C | Z | I | D | B | V | S | 32;    // push P
    I = 4;                                              // set I flag
    d = m[0xfffe] + m[0xffff]*256;                      // PC = IRQ vector
}

// c is related to the interval at which the events are scheduled
// (reading keyboard, triggering IRQ)

void mainloop(int c) {
    FILE *g;

    for (;;) {

        // o is an instruction counter to which events are synchronized
        // advance counter, check keyboard input, put in location 0xa2
        if (!(o++ % (c * 4))) {
            h = wgetch(stdscr);
            if (h >= 0) {
                m[0xa2] = h | 0x80;     // new key with bit 7 set
            }
        }

        // convert pressed key to scan code, or 0xff when no key is pressed
        if (!w) {                   // pet.rom was loaded
            /* 0xe810 is PIA 1 */
            if (z) {                // if bit not OR'd, enter monitor
                m[0xe810] |= 0x80;  // PORTA: 7 Diagnostic sense
            }
            s = m[0xe810] & 15;     // PORTA: 3-0 Keyboard row select
            if (h+1) {              // PORTB: 7-0 Contents of keyboard row
                if (!(s ^ 8)) {
                    m[0xe812] = j[h | 0x80] & 1;
                } else {
                    m[0xe812] = 0;
                }
                if (!(j[h] / 8 - 5 - s)) {
                    m[0xe812] |= 1 << j[h] % 8;
                }
                m[0xe812] ^= 0xff;
            } else {
                m[0xe812] = 0xff;   // no key pressed
            }
        }

        if (!(o % c | I)) {     // trigger IRQ, only when I is clear
            N();
        }

        p = m + O + O + m[n & l - 9];   // pointer to "string" in memory
        t = d % 0xffd5;                 // 0xffd5 is LOAD/SAVE

        i = m[n & d++];     // fetch instruction

        if (w + t < 4) {                // intercept LOAD/SAVE
            if (*p && (u = strchr(++p, 34))) {
                *u = 0;
                m[0x77] = u - m + 1;        // move TXTBUF ptr LSB to EOL
                if (g = fopen(p, t ? "w" : "r")) {
                    i = 1025 - t;           // re-use i as temp variable
                    if (!t) {               // LOAD
                        (s = fgetc(g));
                        for ((s = fgetc(g)); (s = fgetc(g))+1; m[n & i++] = s);
                        for (p = m + 42; p < m + 47;) {
                            *p++ = i % O;
                            *p++ = i / O;
                        }
                    }
                    for (; t; fputc(m[n & i], g))       // SAVE
                        t = m[n & ++i] ? 3 : t - 1;
                    fclose(g);
                }
            }
            i = 0x60;   // reset i to instruction, fake RTS
        }

        // addressing modes, determine effective address or address of operand

        switch(i / 2 & 14 | i & 1) {
        case 4:         // imp
        case 12:
            e = &a - m;
            break;
        case 1:         // (ind,x)
            t = (m[d & 0xffff] + x) & 0xff;
            d++;
            e = m[t] + m[(t+1) & 0xffff] * 256;
            break;
        case 0:         // imm
        case 5:
        case 8:
            e = d++;
            break;
        case 2:         // zp
        case 3:
            e = m[d++ & 0xffff];
            break;
        case 6:         // abs
        case 7:
            t = d;
            d += 2;
            e = m[t & 0xffff] + m[(t+1) & 0xffff] * 256;
            break;
        case 9:         // (ind),y
            t = m[d & 0xffff];
            d++;
            e = m[t & 0xffff] + m[(t+1) & 0xffff] * 256 + y;
            break;
        case 10:        // zp,x and zp,y
        case 11:
            if (i == 0x96 || i == 0xb6) {     // the two zp,y opcodes
                e = m[d & 0xffff] + y;
            } else {
                e = m[d & 0xffff] + x;
            }
            d++;
            e &= 0xff;
            break;
        case 13:        // abs,y
            t = d;
            d += 2;
            e = m[t & 0xffff] + m[(t+1) & 0xffff] * 256 + y;
            break;
        case 14:        // abs,x and one abs,y
        case 15:
            t = d;
            d += 2;
            e = m[t & 0xffff] + m[(t+1) & 0xffff] * 256;
            if (i == 0xbe) {
                e += y;
            } else {
                e += x;
            }
            break;
        }

        p = &m[e];              // set pointer to value at e

        s = i >> 6;             // s is two top bits

        t = insns[i];

        /* another switch statement - the main instruction emulation */
        /* 44 cases */
        switch (t) {
        case 0: // ADC
            t = a + *p + C;
            C = t & O ? 1 : 0;
            V = (~(a ^ *p) & (a ^ t) & l) / 2;
            R(a = t & f);
            break;
        case 1: // AND
            R(a &= *p);
            break;
        case 2: // ASL
            C = *p / l;
            R(*p *= 2);
            break;
        case 3: // branch on flag
            (s ? s - 1 ? s - 2 ? Z : C : V : S) && (d += *p & l ? *p - O : *p);
            break;
        case 4: // branch on !flag
            !(s ? s - 1 ? s - 2 ? Z : C : V : S) && (d += *p & l ? *p - O : *p);
            break;
        case 5: // BIT
            R(a & *p);
            V = *p & 64;
            S = *p & l;
            break;
        case 6: // BRK
            d;
            B = 16;
            fprintf(stderr, "\r\nBRK at PC=%04x\r\n", d - 1);
            N();  /* bugfix: BRK is a 2 byte instruction, and B must be set */
            break;
        case 7: // SED,SEI,SEC (SEV does not exist)
            *(s ? s - 1 ? s - 2 ? &D : &V : &I : &C) = (s ? s - 1 ? s - 2 ? 8 : 8 : 4 : 1);
            break;
        case 8: // CLD,CLV,CLI,CLC
            *(s ? s - 1 ? s - 2 ? &D : &V : &I : &C) = 0;
            break;
        case 9: // CMP
            K(a);
            break;
        case 10: // CPX
            K(x);
            break;
        case 11: // CPY
            K(y);
            break;
        case 12: // DEC
            R(--*p);
            break;
        case 13: // DEX
            R(--x);
            break;
        case 14: // DEY
            R(--y);
            break;
        case 15: // EOR
            R(a ^= *p);
            break;
        case 16: // INC
            R(++*p);
            break;
        case 17: // INX
            R(++x);
            break;
        case 18: // INY
            R(++y);
            break;
        case 19: // JMP, 0x4c abs, 0x6c (ind)
            d = i & 32 ? (t = e, d += 0, m[n & t] + m[n & t + 1] * O) : e;
            break;
        case 20: // JSR
            s = (t = --d, d += 1, m[n & t] + m[n & t + 1] * O), X(), d = s;
            break;
        case 21: // LDA
            R(a = *p);
            break;
        case 22: // LDX
            R(x = *p);
            break;
        case 23: // LDY
            (R(y = *p));
            break;
        case 24: // LSR
            C = *p & 1, R(*p /= 2);
            break;
        case 25: // ORA
            R(a |= *p);
            break;
        case 26: // PHA
            m[n & k-- + O] = a;
            break;
        case 27: // PHP
            (m[n & k-- + O] = C | Z | I | D | B | V | S | 48); /* PHP bugfix - must push B bit as 1 */
            break;
        case 28: // PLA
            R(a = m[n & ++k + O]);
            break;
        case 29: // PLP
            A();
            break;
        case 30: // ROL
            t = *p, R(*p = *p * 2 | C), C = t / l;
            break;
        case 31: // ROR
            t = *p, R(*p = *p / 2 | C * l), C = t & 1;
            break;
        case 32: // RTI
            A(), d = m[n & ++k + O], d |= m[n & ++k + O] * O;
            break;
        case 33: // RTS
            d = m[n & ++k + O], d += m[n & ++k + O] * O + 1;
            break;
        case 34: // SBC
            t = a - *p - 1 + C, C = t & O ? 0 : 1, V = ((a ^ *p) & (a ^ t) & l) / 2, R(a = t & f);
            break;
        case 35: // STA
            *p = a;
            break;
        case 36: // STX
            *p = x;
            break;
        case 37: // STY
            *p = y;
             break;
        case 38: // TAX
             R(x = a);
             break;
        case 39: // TAY
             (R(y = a));
             break;
        case 40: // TSX
             (R(x = k));
             break;
        case 41: // TXA
             (R(a = x));
             break;
        case 42: // TXS
             (k = x);
             break;
        case 43: // TYA
             R(a = y);
             break;
        default:
            break;
        }

        t = e ^ O * O / 2;
        if (t < 1000) {         /* conditionally update screen */
            s = *p % l;
            i = ((1U) << ((10) + 8));
            *p > s ? wattr_on(stdscr, (attr_t) (i), ((void *)0))
                : wattr_off(stdscr, (attr_t) (i), ((void *)0));
            (wmove(stdscr, t / 40, t % 40) == (-1)
             ? (-1)
             : waddch(stdscr, s + w < 32 ? s + 64 : s > 95 + w ? s - 32 : s));
        }
    }
}

int main(int c, char *v[]) {
    FILE *g = NULL;

    m[0] = time(0);             // location 0 is random seed
    if (c > 1)
        g = fopen(v[1], "r");   // open file if specified
    if (!g) {                   // error on no file, or open failed
        perror(*v);
        exit(1);
    }

    t = 0xc000;                 // temp = load address
    while ((s = fgetc(g)) + 1) {
        m[t++ & 0xffff] = s;
    }

    w = t & n;                  // w == 0 if loaded exactly to memtop

    if (!w) {
        d = m[0xfffc] + m[0xfffd] * 256;    // run address from reset vector
    } else {
        d = 0xc000;             // run address = load address
    }

    if (c > 2) {                // speed
        z = atoi(v[2]) + 1;
    } else {
        z = 4;
    }

    c = z ? n * z / 4 : n;

    nodelay(initscr(), 1);
    curs_set(I);
    cbreak();
    noecho();

    mainloop(c);
}

// vim: filetype=c sw=4 ts=4 et
