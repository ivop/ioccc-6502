#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curses.h>

int d;                          // program counter
int C, Z, I, D, B, V, S;        // flags
unsigned char a, x, y, k;       // registers, k=sp

int i = 0xc000;                 // fetched instruction
int t, s, o, h, z, O = 256, n = 0xffff, l = 128, f = 255, e, w;
unsigned char *p, m[65536], *u;


/* j is 528 characters  */
unsigned char *j =
" ./  p/ 7 ] . 6 6 p     t7      r(0)1*+2,4WgcovGn^f_NVO>F?T\\swldiHZYI9QJ"
"RCKSL[b<D8AP:;a@`BXq3j=- HZYI9QJRCKSL[b<D8AP:;a@`BX   57  ;  ;      ;   ;   "
"     ;                                                                      "
"                                )<   <% ><%  <% '<   <% +<   <% 7$  ($A @$A "
"($A &$   $A *$   $A C2   2; =2; 62; '2   2; +2   2; D#   #B ?#B 6#B &#   #B "
"*#   #B  F  HFG 1 L HFG 'F  HFG NFM  F  :89 :89 J8I :89 &8  :89 +8K :89 .,  "
".,/ 5,0 .,/ ',   ,/ +,   ,/ -E  -E3 4E  -E3 &E   E3 *E   E3 2133024425660788";

// calculate Zero, and Sign flags
void R(int x) {
    Z = x ? 0 : 2;
    S = x & 0x80;
}

// calculate Zero, Sign, and Carry flags
void K(int x) {
    R(x - *p);
    C = x < *p ? 0 : 1;
}

// pull P from stack and set registers
void A(void) {
    R(t = m[n & ++k + O]);
    C = t & 1;
    Z = t & 2;
    I = t & 4;
    D = t & 8;
    B = 0;                      /* bugfix: we model B as always zero so IRQ is detected as such by the handler */
    V = t & 64;
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

N() {

#ifdef NOISY
    fprintf(stderr, "\r\nhelper N for BRK/IRQ at time %08d and PC %04x\r\n",
            ttt, d);
#endif

    X();
    m[n & k-- + O] = C | Z | I | D | B | V | S | 32;
    I = 4;
    d = (t = n - 1, d += 0, m[n & t] + m[n & t + 1] * O);
    /*  fprintf(stderr, "\r\nhelper N new PC is %04x fetched from %04x\r\n", d, t);
     */
}

mainloop(int c) {
    FILE *g;

    for (;;) {

        // o is counter to which events are synchronized
        // advance counter, check keyboard input, put in location 0xa2
        if (!(o++ % (c * 4))) {
            h = wgetch(stdscr);
            if (h >= 0) {
                m[0xa2] = h | 0x80;     // new key with bit 7 set
            }
        }

        if (!w) {
            /* 0xe810 is PIA 1 */
            if (z) {
                m[0xe810] |= 0x80;
            }
            s = m[0xe810] & 15;
            if (h+1) {
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
                m[0xe812] = 0xff;
            }
        }

        /* interrupt from timer */
        /*
           if(ttt>65530){
           fprintf(stderr, "\r\ndebug: o is %04x and c is %04x\r\n", o, c);
           }
         */

#ifdef INTERRUPTANNOUNCE
        o % c | I
            || fprintf(stderr, "\r\nInterrupt at time=%08d, PC=%04x\r\n", ttt,
                       d);
#endif
#ifndef TIMERINTERRUPT
        o % c | I || N();       /* no bugfix - we've fiddled B to be always zero */
#endif

        p = m + O + O + m[n & l - 9];
        t = d % 65493;          /* 0xffd5 is LOAD */
        i = m[n & d++];         /* i is the fetched instruction */
#ifdef QUIET
        if ((ttt++ > 1000123000) || (stderrlines > 0) || (d - 1 == 0xc5fb)
            || (visited[d - 1]++ > 999999)) {
            fprintf(stderr,
                    "\r\ntime is %08d, PC is %04x, ifetch of %02x, A is %02x, X is %02x, Y is %02x, P is %02x, S is 01%02x ",
                    ttt, d - 1, i, a, x, y, (C | Z | I | D | B | V | S | 32),
                    k);
            if (stderrlines++ > 39) {
                fprintf(stderr, "\r\n");
                exit(1);
            }
        }
#endif
        if (w + t < 4) {
            if (*p && (u = strchr(++p, 34))) {
                *u = 0;
                m[n & l - 9] = u - m + 1;
                if (g = fopen(p, t ? j + 61 : j + 32)) {
                    i = 1025 - t;
                    if (!t) {
                        (s = fgetc(g));
                        for ((s = fgetc(g)); (s = fgetc(g)) + 1;
                             m[n & i++] = s);
                        for (p = m + 42; p < m + 47;) {
                            *p++ = i % O;
                            *p++ = i / O;
                        }
                    }
                    for (; t; fputc(m[n & i], g))
                        t = m[n & ++i] ? 3 : t - 1;
                    fclose(g);
                }
            }
            i = 96;             /* fake an RTS */
        }

        /* j is a multi-purpose lookup table */
        t = j[i / 2 & 14 | i & 1 | O + O] & 15;

        if (stderrlines > 0) {
            fprintf(stderr, "first lookup, t=%d ", t);
        }

        /* poor man's case statement, switching on t */
        /* e must be effective address and we must be switching on addressing modes */
        e = t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? (t = d, d += 2, m[n & t] + m[n & t + 1] * O) + (i - 190 ? x : y)    /* 190 is BE - uniquely abs,Y */
            : (t = d, d += 2, m[n & t] + m[n & t + 1] * O) + y : f & m[n & d++] + (i - 150 && i - 182 ? x : y)  /* the two zp,Y opcodes */
            : (t = m[n & d], d += 1, m[n & t] + m[n & t + 1] * O) + y
            : (t = d, d += 2, m[n & t] + m[n & t + 1] * O)
            : m[n & d++]
            /* t==2 */ : d++
            : (t = m[n & d] + x & f, d += 1, m[n & t] + m[n & t + 1] * O)
            : &a - m;

        if (stderrlines > 0) {
            fprintf(stderr, "first lookup, eff=%04x ", e);
        }

        p = e + m;
        s = i >> 6;
        t = j[i + O] - 35;

        if (stderrlines > 0) {
            fprintf(stderr, "second lookup, t=%04x ", t);
        }

        /* another case statement - the main instruction emulation */
        t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? t-- ? 0 : R(a = y) : (k = x) : (R(a = x)) : (R(x = k)) : (R(y = a)) : (R(x = a)) : (*p = y)   /* STY */
            : (*p = x)
            : (*p = a)
            : (t = a - *p - 1 + C, C = t & O ? 0 : 1, V =
               ((a ^ *p) & (a ^ t) & l) / 2, R(a = t & f))
            : (d =
               m[n & ++k + O],
               d +=
               m[n & ++k +
                 O] * O +
               1) : (A(), d =
                     m[n & ++k +
                       O], d |=
                     m[n & ++k +
                       O] * O) : (t = *p, R(*p = *p / 2 | C * l), C = t & 1)
            : (t = *p, R(*p = *p * 2 | C), C = t / l) : A() : R(a = m[n & ++k + O]) : (m[n & k-- + O] = C | Z | I | D | B | V | S | 48) /* PHP bugfix - must push B bit as 1 */
            : (m[n & k-- + O] = a) : (R(a |= *p)) : (C =
                                                     *p & 1,
                                                     R(*p /= 2)) : (R(y =
                                                                      *p)) :
            R(x = *p) : R(a = *p) : (s =
                                     (t = --d, d +=
                                      1, m[n & t] + m[n & t + 1] * O), X(), d =
                                     s) : (d = i & 32 ? (t = e, d +=
                                                         0,
                                                         m[n & t] + m[n & t +
                                                                      1] *
                                                         O) : e) : R(++y) :
            R(++x) : R(++*p) : R(a ^=
                                 *p) : R(--y) : R(--x) : R(--*p) : K(y) :
            K(x) : K(a) : (*(s ? s - 1 ? s - 2 ? &D : &V : &I : &C) = 0)
            : ((i == 0xf8) ? fprintf(stderr, "\n\rSED at PC=%04x\n\r", d), exit(1) : 1, (*(s ? s - 1 ? s - 2 ? &D : &V : &I : &C) = (s ? s - 1 ? s - 2 ? 8 : 8 : 4 : 1)))       /* set/clear flags DVIC */
#ifdef OTHERBRK
            : /* t==6 */ (d, B = 16, fprintf(stderr, "\r\nBRK at PC=%04x\r\n", d - 1), N())     /* bugfix: BRK is a 2 byte instruction */
#endif
            : /* t==6 */ (d, B = 16, fprintf(stderr, "\r\nBRK at PC=%04x\r\n", d - 1), N())     /* bugfix: BRK is a 2 byte instruction, and B must be set */
            : (R(a & *p), V = *p & 64, S = *p & l)
            : !(s ? s - 1 ? s - 2 ? Z : C : V : S)
            && (d += *p & l ? *p - O : *p)
            : (s ? s - 1 ? s - 2 ? Z : C : V : S)
            && (d += *p & l ? *p - O : *p)
            : (C = *p / l, R(*p *= 2))
            : R(a &= *p)
      :                        /* t==0 */ (t = a + *p + C, C = t & O ? 1 : 0, V =
                                            (~(a ^ *p) & (a ^ t) & l) / 2,
                                            R(a = t & f));

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
        d = m[0xfffc] + m[0xfffd] * 256;        // run address from reset vector
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
