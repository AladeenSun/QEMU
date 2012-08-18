/*
 * Instruction printing code for the UniCore64
 *   Copyright 2012, by Guan Xuetao <gxt@mprc.pku.edu.cn>
 *
 * This file is part of libopcodes.
 */
#include "dis-asm.h"
#include "elf.h"

/*
 * Opcode tables: UniCore64.  All three are partially ordered:
 * they must be searched linearly from the top to obtain a correct match.

   print_insn_uc64_interal recognizes the following format control codes:

   %%            %

   %a            print address for ldr/str instruction
   %s            print address for ldr/str halfword/signextend instruction
   %b            print branch destination
   %c            print condition code for b/bl instruction  (bits 25-28)
   %m            print register mask for ldm/stm instruction
   %o            print operand2 (immediate or register + shift)
   %p            print 'p' iff bits 12-15 are 15
   %t            print 't' iff bit 21 set and bit 24 clear
   %B            print unicore64 BLX(1) destination
   %C            print the PSR sub type.
   %U            print barrier type.
   %P            print address for pli instruction.

   %<bitfield>r        print as an UC64 register
   %<bitfield>d        print the bitfield in decimal
   %<bitfield>W         print the bitfield plus one in decimal
   %<bitfield>x        print the bitfield in hex
   %<bitfield>X        print the bitfield as 1 hex digit without leading "0x"

   %<bitfield>'c    print specified char iff bitfield is all ones
   %<bitfield>`c    print specified char iff bitfield is all zeroes
   %<bitfield>?ab...    select from array of values in big endian order

   %e            print unicore64 SMI operand (bits 0..7,8..19).
   %E            print the LSB and WIDTH fields of a BFI or BFC instruction.
   %V            print the 16-bit immediate field of a MOVT or
                 MOVW instruction.
   */

struct opcode32 {
    unsigned long arch;         /* Architecture defining this insn.  */
    unsigned long value, mask;  /* Recognise insn if (op&mask)==value.  */
    const char *assembler;      /* How to disassemble this insn.  */
};

static const struct opcode32 unicore64_opcodes[] = {
    {1, 0x00000000, 0xffffffff, "nop\t\t\t(mov r0,r0)"},
    {1, 0x00000000, 0xff400000, "and%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x01000000, 0xff400000, "xor%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x02000000, 0xff400000, "sub%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x03000000, 0xff400000, "rsub%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x04000000, 0xff400000, "add%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x05000000, 0xff400000, "addc%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x06000000, 0xff400000, "subc%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x07000000, 0xff400000, "rsubc%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x08800000, 0xffc00000, "cmpand.a\t%11-15r, %o"},
    {1, 0x09800000, 0xffc00000, "cmpxor.a\t%11-15r, %o"},
    {1, 0x0a800000, 0xffc00000, "cmpsub.a\t%11-15r, %o"},
    {1, 0x0b800000, 0xffc00000, "cmpadd.a\t%11-15r, %o"},
    {1, 0x0c000000, 0xff400000, "or%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x0d000000, 0xff400000, "mov%23'.%23'a\t%16-20r, %o"},
    {1, 0x0e000000, 0xff400000, "andn%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x0f000000, 0xff400000, "not%23'.%23'a\t%16-20r, %o"},
    {1, 0x00400000, 0xff400000, "dand%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x01400000, 0xff400000, "dxor%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x02400000, 0xff400000, "dsub%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x03400000, 0xff400000, "drsub%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x04400000, 0xff400000, "dadd%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x05400000, 0xff400000, "daddc%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x06400000, 0xff400000, "dsubc%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x07400000, 0xff400000, "drsubc%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x08c00000, 0xffc00000, "dcmpand.a\t%11-15r, %o"},
    {1, 0x09c00000, 0xffc00000, "dcmpxor.a\t%11-15r, %o"},
    {1, 0x0ac00000, 0xffc00000, "dcmpsub.a\t%11-15r, %o"},
    {1, 0x0bc00000, 0xffc00000, "dcmpadd.a\t%11-15r, %o"},
    {1, 0x0c400000, 0xff400000, "dor%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x0d400000, 0xff400000, "dmov%23'.%23'a\t%16-20r, %o"},
    {1, 0x0e400000, 0xff400000, "dandn%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x0f400000, 0xff400000, "dnot%23'.%23'a\t%16-20r, %o"},

    {1, 0x10000000, 0xff400000, "lsl%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x11000000, 0xff400000, "lsr%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x12000000, 0xff400000, "asr%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x10400000, 0xff400000, "dlsl%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x11400000, 0xff400000, "dlsr%23'.%23'a\t%16-20r, %11-15r, %o"},
    {1, 0x12400000, 0xff400000, "dasr%23'.%23'a\t%16-20r, %11-15r, %o"},

    {1, 0x18000000, 0xfc400000, "cntl%23?zo\t%16-20r, %11-15r"},
    {1, 0x18400000, 0xfc400000, "dcntl%23?zo\t%16-20r, %11-15r"},

    {1, 0x1d000000, 0xff400000, "cmov%12-15c%23'.%23'a\t%16-20r, %o"},
    {1, 0x1d400000, 0xff400000, "dcmov%12-15c%23'.%23'a\t%16-20r, %o"},
    {1, 0x1f000000, 0xff400000, "cnot%12-15c%23'.%23'a\t%16-20r, %o"},
    {1, 0x1f400000, 0xff400000, "dcnot%12-15c%23'.%23'a\t%16-20r, %o"},

    {1, 0x20000000, 0xf0000000, "mov\t%16-20r, %26?ab%27?fsr"},
    {1, 0x30000000, 0xf0000000, "mov\t%26?ab%27?fsr, %o"},

    {1, 0x40000000, 0xf8400000, "mul%23'.%23'a\t%16-20r, %11-15r, %6-10r"},
    {1, 0x48000000, 0xf8400000, "mula%23'.%23'a\t%16-20r, %11-15r, %6-10r"},
    {1, 0x40400000, 0xf8400000, "dmul%23'.%23'a\t%16-20r, %11-15r, %6-10r"},
    {1, 0x48400000, 0xf8400000, "dmula%23'.%23'a\t%16-20r, %11-15r, %6-10r"},
    {1, 0x50000000, 0xf8400000, "divs%23'.%23'a\t%16-20r, %11-15r, %6-10r"},
    {1, 0x58000000, 0xf8400000, "divu%23'.%23'a\t%16-20r, %11-15r, %6-10r"},
    {1, 0x50400000, 0xf8400000, "ddivs%23'.%23'a\t%16-20r, %11-15r, %6-10r"},
    {1, 0x58400000, 0xf8400000, "ddivu%23'.%23'a\t%16-20r, %11-15r, %6-10r"},

    {1, 0x60000000, 0xefc00000, "prefetch\t#%16-20x, %a"},

    {1, 0x68000000, 0xffe00000, "sync\t%y"},
    {1, 0x61000000, 0xe3c00000, "stb%w\t%16-20r, %a"},
    {1, 0x60800000, 0xe3c00000, "sth%w\t%16-20r, %a"},
    {1, 0x61800000, 0xe3c00000, "stw%w\t%16-20r, %a"},
    {1, 0x61400000, 0xe3c00000, "stsb%w\t%16-20r, %a"},
    {1, 0x60c00000, 0xe3c00000, "stsh%w\t%16-20r, %a"},
    {1, 0x61c00000, 0xe3c00000, "stsw%w\t%16-20r, %a"},
    {1, 0x60400000, 0xe3c00000, "std%w\t%16-20r, %a"},
    {1, 0x63000000, 0xe3c00000, "ldb%w\t%16-20r, %a"},
    {1, 0x62800000, 0xe3c00000, "ldh%w\t%16-20r, %a"},
    {1, 0x63800000, 0xe3c00000, "ldw%w\t%16-20r, %a"},
    {1, 0x63400000, 0xe3c00000, "ldsb%w\t%16-20r, %a"},
    {1, 0x62c00000, 0xe3c00000, "ldsh%w\t%16-20r, %a"},
    {1, 0x63c00000, 0xe3c00000, "ldsw%w\t%16-20r, %a"},
    {1, 0x62400000, 0xe3c00000, "ldd%w\t%16-20r, %a"},
    {1, 0x80000000, 0xe3000000, "st%22?dw.%u\t%16-20r, %a"},
    {1, 0x82000000, 0xe3000000, "ld%22?dw.%u\t%16-20r, %a"},
    {1, 0x81000000, 0xe3800000, "sc%22?dw%26'.%26'w\t%16-20r, %a"},
    {1, 0x83000000, 0xe3800000, "ll%22?dw%26'.%26'w\t%16-20r, %a"},

    {1, 0x81a00000, 0xffe00000, "swapb\t%16-20r, [%11-15r], %6-10r"},
    {1, 0x81800000, 0xffe00000, "swapw\t%16-20r, [%11-15r], %6-10r"},
    {1, 0x81c00000, 0xffe00000, "swapd\t%16-20r, [%11-15r], %6-10r"},

    {1, 0xaf400000, 0xffe00000, "direct\t%16-20r, %I"},

    {1, 0xa0000000, 0xf0000000, "%C"},
    {1, 0xe0000000, 0xf0000000, "%C"},
    {1, 0xf0000000, 0xf0000000, "%C"},
    {1, 0xbe000000, 0xff000000, "call\t%i"},
    {1, 0xbf000000, 0xff000000, "call.r\t%11-15r"},
    {1, 0xc2400000, 0xffe007ff, "mff\t%16-20r, %11-15f"},
    {1, 0xc0400000, 0xffe007ff, "mtf\t%16-20r, %11-15f"},
    {1, 0xc6400000, 0xffe007ff, "cff\t%16-20r"},
    {1, 0xc4400000, 0xffe007ff, "ctf\t%16-20r"},

    /* UniCore 2D */
    {1, 0xe0000e20, 0xfd003fe0, "TSLL.%25?WH\t%14-18f, %19-23f, #%0-4d"},
    {1, 0xe1000e20, 0xfd003fe0, "TSLL.%25?WH\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe4000e20, 0xfd003fe0, "TSRL.%25?WH\t%14-18f, %19-23f, #%0-4d"},
    {1, 0xe5000e20, 0xfd003fe0, "TSRL.%25?WH\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe8000e20, 0xfd003fe0, "TSRA.%25?WH\t%14-18f, %19-23f, #%0-4d"},
    {1, 0xe9000e20, 0xfd003fe0, "TSRA.%25?WH\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000c00, 0xff003fff, "TMTF\t%14-18r, %19-23f"},
    {1, 0xe1000c00, 0xff003fff, "TMFF\t%14-18r, %19-23f"},
    {1, 0xe2000c00, 0xff003fff, "TCTF\t%14-18r, %19-23S"},
    {1, 0xe3000c00, 0xff003fff, "TCFF\t%14-18r, %19-23S"},
    {1, 0xe0000c20, 0xf0003fe0, "T%27'SADD%26?SU.%24-25p\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000c60, 0xf0003fe0, "T%27'SSUB%26?SU.%24-25p\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000ca0, 0xf0003fe0, "T%27'SMULL%26?SU.%24-25p\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000ce0, 0xf0003fe0, "T%27'SMULH%26?SU.%24-25p\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000d20, 0xf0003fe0, "TCMP.EQ.%24-25p\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000d60, 0xf0003fe0, "TCMP.GT.%24-25p\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000da0, 0xf0003fe0, "TMAX%26?SU.%24-25p\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000de0, 0xf0003fe0, "TMIN%26?SU.%24-25p\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000e60, 0xf0003fe0, "TMAL.%24-25p\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000ea0, 0xf0003fe0, "TAVG.%24-25p\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000ee0, 0xf8003fe0, "TSHUF.H\t%14-18f, %0-4f, %19-26x"},
    {1, 0xe8000ee0, 0xf8003fe0, "TSHUFZ.H\t%14-18f, %0-4f, %19-26x"},
    {1, 0xe8000f60, 0xf8003fe0, "TINSZ.B\t %14-18f, %0-4f, %19-26x"},
    {1, 0xe4000f60, 0xff003fe0, "TOR\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe5000f60, 0xff003fe0, "TAND\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe6000f60, 0xff003fe0, "TXOR\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe7000f60, 0xff003fe0, "TANDN\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000f60, 0xff003fe0, "TSADU.B\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000ee0, 0xf0003fe0, "TSWAP.W\t%14-18f, %0-4f"},
    {1, 0xe0000f20, 0xf0003fe0, "T%27'SP2%26?SU.%24-25p\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000fa0, 0xf0003fe0, "TUPH.%24-25p\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0000fe0, 0xf0003fe0, "TUPL.%24-25p\t%14-18f, %19-23f, %0-4f"},

    /* UniCore 3D */
    {1, 0xe1001000, 0xff003c3f, "TFMFF\t%14-18r, %19-23f"},
    {1, 0xe3001000, 0xff003c3f, "TFCFF\t%14-18r, %19-23S"},
    {1, 0xe9001000, 0xff003c3f, "TFMFFC.%6-9n\t%14-18r, %19-23f"},
    {1, 0xe0001000, 0xff003fff, "TFMTF\t%14-18r, %19-23f"},
    {1, 0xe2001000, 0xff003fff, "TFCTF\t%14-18r, %19-23S"},
    {1, 0xe40010e0, 0xff003fe0, "TFSWAP.W\t%14-18f, %0-4f"},
    {1, 0xe0001020, 0xff003fe0, "TFADD\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0001060, 0xff003fe0, "TFSUB\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0001260, 0xff003fe0, "TFSUBR\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0001120, 0xff003fe0, "TFACC\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0001360, 0xff003fe0, "TFNACC\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe0001160, 0xff003fe0, "TFPNACC\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe00010a0, 0xff003fe0, "TFMUL\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe40011a0, 0xff003fe0, "TFSQRT\t%14-18f, %19-23f"},
    {1, 0xe00011e0, 0xff003fe0, "TFMAX\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe00013e0, 0xff003fe0, "TFMIN\t%14-18f, %19-23f, %0-4f"},
    {1, 0xec001020, 0xff003c20, "TFCMP.%6-9n\t%14-18f, %19-23f, %0-4f"},
    {1, 0xe8001120, 0xff003fe0, "TF2IW\t%14-18f, %0-4f"},
    {1, 0xea001020, 0xff003fe0, "TI2FW\t%14-18f, %0-4f"},
    {1, 0xe8001320, 0xff003fe0, "TF2IH\t%14-18f, %0-4f"},
    {1, 0xea001220, 0xff003fe0, "TI2FH\t%14-18f, %0-4f"},
    {1, 0xc0000015, 0xfc00003f, "FABS.%25-26F\t%16-20f, %6-10f"},
    {1, 0xc0000001, 0xfc00003f, "FADD.%25-26F\t%16-20f, %11-15f, %6-10f"},
    {1, 0xd8000001, 0xfc000003, "FCMP.%2-5n.%25-26F\t%11-15f, %6-10f"},
    {1, 0xd0000005, 0xf800003f, "FCVT.D.%25-26F\t%16-20f, %6-10f"},
    {1, 0xd0000001, 0xf800003f, "FCVT.S.%25-26F\t%16-20f, %6-10f"},
    {1, 0xd0000011, 0xf800003f, "FCVT.W.%25-26F\t%16-20f, %6-10f"},
    {1, 0xc2000002, 0xe2000003, "LWF%W\t%16-20f, %A"},
    {1, 0xc0000002, 0xe2000003, "SWF%W\t%16-20f, %A"},
    {1, 0xc2000003, 0xe2000003, "LDWF%W\t%16-20f, %A"},
    {1, 0xc0000003, 0xe2000003, "SDWF%W\t%16-20f, %A"},
    {1, 0xc0000019, 0xf800003f, "FMOV.%25-26F\t%16-20f, %6-10f"},
    {1, 0xc0000009, 0xfc00003f, "FMUL.%25-26F\t%16-20f, %11-15f, %6-10f"},
    {1, 0xc000001d, 0xfc00003f, "FNEG.%25-26F\t%16-20f, %6-10f"},
    {1, 0xc0000005, 0xfc00003f, "FSUB.%25-26F\t%16-20f, %11-15f, %6-10f"},
    {1, 0xc0000011, 0xfc00003f, "FDIV.%25-26F\t%16-20f, %11-15f, %6-10f"},
    {1, 0xc0000025, 0xf800083f, "FMOV.F.%25-26F\t%16-20f, %6-10f"},
    {1, 0xc0000825, 0xf800083f, "FMOV.T.%25-26F\t%16-20f, %6-10f"},
    {1, 0xc2000000, 0xfe000003, "movc\t%16-20r, p%21-24d.c%11-15d, %X"},
    {1, 0xc0000000, 0xfe000003, "movc\tp%21-24d.c%16-20d, %11-15r, %X"},
    {1, 0x00000000, 0x00000000, "undefined instruction %0-31x"},
    {1, 0x00000000, 0x00000000, 0}
};

static const char *unicore64_conditional[] = {
    "eq", "ne", "ea", "ub", "fs", "ns", "fv", "nv",
    "ua", "eb", "eg", "sl", "sg", "el", "al", "na"
};

static const char *unicore64_2D_fmt[] = { "B", "H", "W", "reserved" };

static const char *unicore64_shift[] = { "<<", ">>", "|>", "<>" };

typedef struct {
    const char *name;
    const char *description;
    const char *reg_names[32];
} unicore64_regname;

static const unicore64_regname regnames[] = {
    {"raw", "Select raw register names",
     {
      "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
      "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
      "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"}
     },
    {"gcc", "Select register names used by GCC",
     {
      "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
      "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
      "r24", "r25", "sl", "fp", "ip", "sp", "lr", "pc"}
     },
    {"std", "Select register names used in UNICORE's ISA documentation",
     {
      "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
      "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
      "r24", "r25", "r26", "r27", "r28", "sp", "lr", "pc"}
     },
};

static const char *unicore64_Ffmt[] = { "S", "D", "W", "?" };

static const char *unicore64_Fcond[] = {
    "F", "UN", "EQ", "UEQ", "OLT", "ULT", "OLE", "ULE",
    "SF", "NGLE", "SEQ", "NGL", "LT", "NGE", "LE", "NGT"
};

static const char *unicore64_Fregister[] = {
    "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
    "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
    "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
    "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31"
};

static const char *unicore64_FSregister[] = {
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15",
    "s16", "s17", "s18", "s19", "s20", "s21", "s22", "s23",
    "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31"
};

/* Default to GCC register name set.  */
#define unicore64_regnames      regnames[1].reg_names

/* Decode a bitfield of the form matching regexp (N(-N)?,)*N(-N)?.
   Returns pointer to following character of the format string and
   fills in *VALUEP and *WIDTHP with the extracted value and number of
   bits extracted.  WIDTHP can be NULL. */
static const char *unicore64_decode_bitfield(const char *ptr,
        unsigned long insn, unsigned long *valuep, int *widthp)
{
    unsigned long value = 0;
    int width = 0;

    do {
        int start, end;
        int bits;

        for (start = 0; *ptr >= '0' && *ptr <= '9'; ptr++) {
            start = start * 10 + *ptr - '0';
        }
        if (*ptr == '-') {
            for (end = 0, ptr++; *ptr >= '0' && *ptr <= '9'; ptr++) {
                end = end * 10 + *ptr - '0';
            }
        } else {
            end = start;
        }
        bits = end - start;
        if (bits < 0) {
            abort();
        }
        value |= ((insn >> start) & ((2ul << bits) - 1)) << width;
        width += bits + 1;
    } while (*ptr++ == ',');
    *valuep = value;
    if (widthp) {
        *widthp = width;
    }
    return ptr - 1;
}

static void unicore64_decode_shift(long given, fprintf_function func,
        void *stream, int print_shift)
{
    func(stream, "%s", unicore64_regnames[given & 0x1f]);   /*FIXME*/
    if ((given & 0x3fe0) != 0) {
        if ((given & 0x20) == 0) {
            int amount = (given & 0x3e00) >> 9;
            int shift = (given & 0xc0) >> 6;
            if (amount == 0) {
                if (shift == 3) {
                    func(stream, "<>#33");
                    return;
                }
                amount = 32;
            }

            if (print_shift) {
                func(stream, " %s #%d", unicore64_shift[shift], amount);
            } else {
                func(stream, " #%d", amount);
            }
        } else if (print_shift) {
            func(stream, " %s %s", unicore64_shift[(given & 0xc0) >> 6],
                 unicore64_regnames[(given & 0x3e00) >> 9]);
        } else {
            func(stream, " %s", unicore64_regnames[(given & 0x3e00) >> 9]);
        }
    }
}

static void print_unicore64_address(bfd_vma pc, struct disassemble_info *info,
        long given)
{
    void *stream = info->stream;
    fprintf_function func = info->fprintf_func;

    func(stream, "[%s", unicore64_regnames[(given >> 11) & 0x1f]);
    if ((given & 0xefc00000) == 0x60000000) {
        if ((given & 0x08000000) == 0) {/*Pre-index */
            if ((given & 0x10000000) == 0) {/*Down */
                func(stream, "-]");
            } else {/*Up */
                func(stream, "+]");
            }
        } else {/*Post-index */
            if ((given & 0x10000000) != 0) {/*Down */
                func(stream, "]-");
            } else {/*Up */
                func(stream, "]+");
            }
        }
    } else {
        if ((given & 0x08000000) != 0) {/*Pre-index */
            if ((given & 0x10000000) == 0) {/*Down */
                func(stream, "-]");
            } else {/*Up */
                func(stream, "+]");
            }
        } else {/*Post-index */
            if ((given & 0x10000000) == 0) {/*Down */
                func(stream, "]-");
            } else {/*Up */
                func(stream, "]+");
            }
        }
    }
    if ((given & 0x00200000) != 0) {/*immediate */
        int immed;
        immed = (given & 0x7ff);
        func(stream, ", #%d\t", immed);
        if ((given & 0x0000f800) == 0x0000f800) {/*PC*/
            if ((given & 0x10000000) == 0) {
                immed = -immed;
            }
            immed += pc;
            info->print_address_func(immed, info);
        }
    } else {
        func(stream, ", %s", unicore64_regnames[(given & 0x7c0) >> 6]);
    }
}

/* Print one UC64 instruction from PC on INFO->STREAM.  */
static void print_insn_uc64_interal(bfd_vma pc, struct disassemble_info *info,
        long given)
{
    const struct opcode32 *insn;
    void *stream = info->stream;
    fprintf_function func = info->fprintf_func;

    for (insn = unicore64_opcodes; insn->assembler; insn++) {
        /* if (insn->value == FIRST_IWMMXT_INSN
         * && info->mach != bfd_mach_unicore64_XScale
         * && info->mach != bfd_mach_unicore64_iWMMXt)
         * insn = insn + IWMMXT_INSN_COUNT;
         */
        if ((given & insn->mask) == insn->value) {
            const char *c;

            func(stream, "[%08x]   ", (uint32_t) given);
            for (c = insn->assembler; *c; c++) {
                if (*c == '%') {
                    switch (*++c) {
                    case '%':
                        func(stream, "%%");
                        break;

                    case 'a':
                        print_unicore64_address(pc, info, given);
                        break;

                    case 'P':
                        /* Set P address bit and use normal address
                           printing routine.  */
                        print_unicore64_address(pc, info, given | (1 << 24));
                        break;

                    case 's':
                        if ((given & 0x04f80000) == 0x04f80000) {
                            /* PC relative with immediate offset.  */
                            int offset =
                                ((given & 0x3e00) >> 4) | (given & 0x1f);

                            if ((given & 0x08000000) == 0) {
                                offset = -offset;
                            }

                            func(stream, "[pc], #%d\t; ", offset);
                            info->print_address_func(offset + pc, info);
                        } else {
                            func(stream, "[%s",
                                 unicore64_regnames[(given >> 19) & 0x1f]);
                            if ((given & 0x10000000) != 0) {
                                /* Pre-indexed.  */
                                if ((given & 0x04000000) == 0x04000000) {
                                    /* Immediate.  */
                                    int offset =
                                        ((given & 0x3e00) >> 4) | (given &
                                                                   0x1f);
                                    if (offset) {
                                        func(stream, "%s], #%d",
                                        (((given & 0x08000000) ==
                                               0) ? "-" : "+"), offset);
                                    } else {
                                        func(stream, "%s]",
                                             (((given & 0x08000000) ==
                                               0) ? "-" : "+"));
                                    }

                                } else {
                                    /* Register.  */
                                    func(stream, "%s], %s",
                                         (((given & 0x08000000) ==
                                           0) ? "-" : "+"),
                                         unicore64_regnames[given & 0x1f]);
                                }

                            } else {
                                /* Post-indexed.  */
                                if ((given & 0x04000000) == 0x04000000) {
                                    /* Immediate.  */
                                    int offset =
                                        ((given & 0x3e00) >> 4) | (given &
                                                                   0x1f);
                                    if (offset) {
                                        func(stream, "]%s, #%d",
                                             (((given & 0x08000000) ==
                                               0) ? "-" : "+"), offset);
                                    } else {
                                        func(stream, "]");
                                    }
                                } else {
                                    /* Register.  */
                                    func(stream, "]%s, %s",
                                         (((given & 0x08000000) ==
                                           0) ? "-" : "+"),
                                         unicore64_regnames[given & 0x1f]);
                                }
                            }
                        }
                        break;

                    case 'I':
                        {
                            /*#imm16 signed-extended */
                            int disp = (((given & 0xffff) ^ 0x8000) - 0x8000);
                            info->print_address_func(disp * 4 + pc, info);
                        }
                        break;

                    case 'i':
                        {
                            /*#imm24 signed-extended */
                            int disp =
                                (((given & 0xffffff) ^ 0x800000) - 0x800000);
                            info->print_address_func(disp * 4 + pc, info);
                        }
                        break;
                    case 'C':
                        if ((given & 0xf0000000) == 0xf0000000) {
                            if ((given & 0x0f000000) != 0x0f000000) {
                                func(stream, "jepriv\t");
                                /*#imm24 signed-extended */
                                int disp =
                                   (((given & 0xffffff) ^ 0x800000) - 0x800000);
                                info->print_address_func(disp * 4 + pc, info);
                            } else {
                                func(stream, "halt");
                            }
                        } else if ((given & 0xf0000000) == 0xe0000000) {
                            if (((given & 0x0f000000) != 0x0e000000)
                                && ((given & 0x0f000000) != 0x0f000000)) {
                                func(stream, "jepriv%s\t",
                                unicore64_conditional[(given >> 24) & 0xf]);
                            } else if ((given & 0x0f000000) == 0x0e000000) {
                                func(stream, "bkpt\t");
                            }
                            /*#imm24 signed-extended */
                            int disp =
                                (((given & 0xffffff) ^ 0x800000) - 0x800000);
                            info->print_address_func(disp * 4 + pc, info);
                            if ((given & 0x0f000000) == 0x0f000000) {
                                func(stream, "ext");
                            }
                        } else {
                            if ((given & 0x0f000000) != 0x0f000000) {
                                func(stream, "b%s",
                                     unicore64_conditional[(given >> 24) &
                                                           0xf]);
                                if ((given & 0x10000000) == 0x10000000) {
                                    func(stream, ".l\t");
                                } else {
                                    func(stream, "\t");
                                }
                                /*#imm24 signed-extended */
                                int disp =
                                   (((given & 0xffffff) ^ 0x800000) - 0x800000);
                                info->print_address_func(disp * 4 + pc, info);
                            } else {
                                if ((given & 0x00c00000) == 0x00000000) {
                                    func(stream, "jump\t");
                                    func(stream, "%s",
                                         unicore64_regnames[(given &
                                                             0x0000f800) >>
                                                            11]);
                                }

                                if ((given & 0x00c00000) == 0x00800000) {
                                    func(stream, "return ");
                                }
                                if ((given & 0x00c00000) == 0x00c00000) {
                                    func(stream, "eret ");
                                }
                            }
                        }
                        break;

                    case 'u':
                        if ((given & 0x04000000) == 0x04000000) {
                            func(stream, "w");
                        }
                        if ((given & 0x00800000) == 0x00800000) {
                            func(stream, "s");
                        } else {
                            func(stream, "u");
                        }

                        break;

                    case 'X':
                        {
                            int offset = ((given & 0x3ff) >> 2);
                            func(stream, "#%d", offset);
                            break;
                        }

                    case 'q':
                        unicore64_decode_shift(given, func, stream, 0);
                        break;

                    case 'M':
                        {
                            int started = 0;
                            int reg;
                            int list;   /* register list */
                            int base = 0;   /* base register of list */
                            /* Get register list */
                            list = (given & 0x000000ff);
                            switch (given & 0x00000300) {
                            case 0x0:
                                base = 0;
                                break;
                            case 0x100:
                                base = 8;
                                break;
                            case 0x200:
                                base = 16;
                                break;
                            case 0x300:
                                base = 24;
                                break;
                            }
                            func(stream, "(");
                            for (reg = 0; reg < 8; reg++) {
                                if ((list & (1 << reg)) != 0) {
                                    if (started) {
                                        func(stream, ", ");
                                    }
                                    started = 1;
                                    func(stream, "%s",
                                         unicore64_Fregister[base + reg]);
                                }
                            }
                            func(stream, ")");
                        }
                        break;

                    case 'o':
                    /*if ((given &0x1c000000) == 0x10000000) {
                     *  switch ((given & 0x03000000) >> 24) {
                     *  case 0x0: func(stream, "<<"); break;
                     *  case 0x1: func(stream, ">>"); break;
                     *  case 0x2: func(stream, "|>"); break;
                     *  default:break;
                     *  }
                     *}
                     */
                        if ((given & 0x00200000) != 0) {
                            int immed;
                            if ((given & 0x000007c0) == 0x00000000) {
                                immed = (given & 0x3f);
                            } else {
                                immed = (given & 0x7ff);
                            }
                            func(stream, "#%d\t; 0x%x", immed, immed);
                        } else {
                            if (((given >> 29) & 0x7) == 0x1) {
                                func(stream, " %s",
                                     unicore64_regnames[(given >> 11 & 0x1f)]);
                            } else {
                                func(stream, " %s",
                                     unicore64_regnames[(given & 0x7c0) >> 6]);
                            }
                        }
                        break;

                    case 'y':
                        {
                            int immed = ((given & 0x008f0000) >> 16);
                            func(stream, "#%d\t; 0x%x", immed, immed);
                        }
                        break;

                    case 'p':
                        if ((given & 0x0000f000) == 0x0000f000) {
                            func(stream, "p");
                        }
                        break;

                    case 'A':
                        func(stream, "[%s",
                             unicore64_regnames[(given >> 11) & 0x1f]);
                        int offset = (given & 0x7fc);
                        if ((given & (1 << 27)) != 0) {/*Pre-index */
                            if (offset) {
                                func(stream, "%s], #%d",
                                     ((given & 0x10000000) == 0 ? "-" : "+"),
                                     offset);
                            }  else {
                                func(stream, "]");
                            }
                        } else {/*post-index */
                            func(stream, "]");
                            if (offset) {
                                func(stream, "%s, #%d",
                                     ((given & 0x10000000) == 0 ? "-" : "+"),
                                     offset);
                            }
                        }
                        break;

                    case 'B':
                        /* Print UC64 V5 BLX(1) address: pc+25 bits.  */
                        {
                            bfd_vma address;
                            bfd_vma offset = 0;

                            /* Is signed, hi bits should be ones.  */
                            if (given & 0x00800000) {
                                offset = (-1) ^ 0x00ffffff;
                            }

                            /* Offset is (SignExtend(offset field)<<2).  */
                            offset += given & 0x00ffffff;
                            offset <<= 2;
                            address = offset + pc;

                            /* H bit allows addressing to 2-byte boundaries.  */
                            if (given & 0x01000000) {
                                address += 2;
                            }

                            info->print_address_func(address, info);
                        }
                        break;
                        /* "%t" - print 'u' iff bit 25 set and bit 28 clear */
                    case 't':
                        if ((given & 0x12000000) == 0x02000000) {
                            func(stream, ".u");
                        }
                        break;

                        /* "%w" - print 'w' iff bit 25 set and bit
                         * 28 set of bit 25
                         * clear and bit 28 clear
                         */
                    case 'w':
                        if ((given & 0x04000000) == 0x04000000) {
                            func(stream, ".w");
                        }
                        break;

                        /* "%W" - print 'W' iff bit 25 set */
                    case 'W':
                        if ((given & 0x04000000) == 0x04000000) {
                            func(stream, ".W");
                        }
                        break;

                        /* "%h" - print 'h' iff bit 6 set, else print 'b' */
                    case 'h':
                        switch (given & 0x01c00000) {
                        case 0x01000000:
                            func(stream, "b");
                            break;
                        case 0x00800000:
                            func(stream, "h");
                            break;
                        case 0x01800000:
                            func(stream, "w");
                            break;
                        case 0x01400000:
                            func(stream, "sb");
                            break;
                        case 0x00c00000:
                            func(stream, "sh");
                            break;
                        case 0x01c00000:
                            func(stream, "sw");
                            break;
                        case 0x00400000:
                            func(stream, "d");
                            break;
                        default:
                            break;
                        }
                        break;

                    case 'U':
                        switch (given & 0xf) {
                        case 0xf:
                            func(stream, "sy");
                            break;
                        case 0x7:
                            func(stream, "un");
                            break;
                        case 0xe:
                            func(stream, "st");
                            break;
                        case 0x6:
                            func(stream, "unst");
                            break;
                        default:
                            func(stream, "#%d", (int)given & 0xf);
                            break;
                        }
                        break;

                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        {
                            int width;
                            unsigned long value;

                            c = unicore64_decode_bitfield(c, given, &value,
                                                          &width);

                            switch (*c) {
                            case 'r':
                                func(stream, "%s", unicore64_regnames[value]);
                                break;

                            case 'f':
                                func(stream, "%s", unicore64_Fregister[value]);
                                break;

                            case 'S':
                                func(stream, "%s",
                                     unicore64_FSregister[value]);
                                break;

                            case 'F':
                                func(stream, "%s", unicore64_Ffmt[value]);
                                break;

                            case 'n':
                                func(stream, "%s", unicore64_Fcond[value]);
                                break;

                            case 'c':
                                func(stream, "%s",
                                     unicore64_conditional[value]);
                                break;

                            case 'p':
                                func(stream, "%s", unicore64_2D_fmt[value]);
                                break;

                            case 'd':
                                func(stream, "%ld", value);
                                break;

                            case 'b':
                                func(stream, "%ld", value * 8);
                                break;

                            case 'W':
                                func(stream, "%ld", value + 1);
                                break;

                            case 'x':
                                func(stream, "0x%08lx", value);

                                /* Some SWI instructions have special
                                   meanings.  */
                                if ((given & 0x0fffffff) == 0x0FF00000) {
                                    func(stream, "\t; IMB");
                                } else if
                                    ((given & 0x0fffffff) == 0x0FF00001) {
                                    func(stream, "\t; IMBRange");
                                }
                                break;
                            case 'X':
                                func(stream, "%01lx", value & 0xf);
                                break;
                            case '`':
                                c++;
                                if (value == 0) {
                                    func(stream, "%c", *c);
                                }
                                break;
                            case '\'':
                                c++;
                                if (value == ((1ul << width) - 1)) {
                                    func(stream, "%c", *c);
                                }
                                break;
                            case '?':
                                func(stream, "%c",
                                     c[(1 << width) - (int)value]);
                                c += 1 << width;
                                break;
                            default:
                                abort();
                            }
                            break;

                    default:
                            abort();
                        }
                    }
                } else {
                    func(stream, "%c", *c);
                }
            }
            return;
        }
    }
    abort();
}

/* Print data bytes on INFO->STREAM.  */
static void print_insn_data(bfd_vma pc ATTRIBUTE_UNUSED,
        struct disassemble_info *info, long long given)
{
    switch (info->bytes_per_chunk) {
    case 1:
        info->fprintf_func(info->stream, ".byte\t0x%02llx", given);
        break;
    case 2:
        info->fprintf_func(info->stream, ".short\t0x%04llx", given);
        break;
    case 4:
        info->fprintf_func(info->stream, ".word\t0x%08llx", given);
        break;
    case 8:
        info->fprintf_func(info->stream, ".dword\t0x%llx", given);
        break;
    default:
        abort();
    }
}

/* NOTE: There are no checks in these routines that
   the relevant number of data bytes exist.  */
static int print_insn_internal(bfd_vma pc, struct disassemble_info *info,
        bfd_boolean little)
{
    unsigned char b[8];
    long given = 0;
    long long given_data = 0;
    int status = 0;
    int is_data = FALSE;
    int little_code;
    unsigned int size = 4;
    void (*print_data) (bfd_vma, struct disassemble_info *, long long) = NULL;
    void (*printer) (bfd_vma, struct disassemble_info *, long) = NULL;

    if (info->disassembler_options) {
        /* To avoid repeated parsing of these options, we remove them here.  */
        info->disassembler_options = NULL;
    }

    /* Decide if our code is going to be little-endian, despite what the
       function argument might say.  */
    little_code = (little);

    info->display_endian = little ? BFD_ENDIAN_LITTLE : BFD_ENDIAN_BIG;
    if (is_data && size == 8) {
        info->bytes_per_line = 8;
    } else {
        info->bytes_per_line = 4;
    }

    if (is_data) {
        int i;

        /* size was already set above.  */
        info->bytes_per_chunk = size;
        print_data = print_insn_data;

        status = info->read_memory_func(pc, (bfd_byte *) b, size, info);
        given_data = 0;
        if (little) {
            for (i = size - 1; i >= 0; i--) {
                given_data = b[i] | (given_data << 8);
            }
        } else {
            for (i = 0; i < (int)size; i++) {
                given_data = b[i] | (given_data << 8);
            }
        }
    } else {
        /* In UC64 mode endianness is a straightforward issue: the
         * instructionis four bytes long and is either ordered 0123 or
         * 3210.
         */
        printer = print_insn_uc64_interal;
        info->bytes_per_chunk = 4;
        size = 4;

        status = info->read_memory_func(pc, (bfd_byte *) b, 4, info);
        if (little_code) {
            given = (b[0]) | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
        } else {
            given = (b[3]) | (b[2] << 8) | (b[1] << 16) | (b[0] << 24);
        }
    }

    if (status) {
        info->memory_error_func(status, pc, info);
        return -1;
    }
    if (info->flags & INSN_HAS_RELOC) {
        /* If the instruction has a reloc associated with it, then
           the offset field in the instruction will actually be the
           addend for the reloc.  (We are using REL type relocs).
           In such cases, we can ignore the pc when computing
           addresses, since the addend is not currently pc-relative.  */
        pc = 0;
    }

    if (is_data) {
        print_data(pc, info, given_data);
    } else {
        printer(pc, info, given);
    }

    return size;
}

int print_insn_unicore64(bfd_vma pc, struct disassemble_info *info)
{
    return print_insn_internal(pc, info, TRUE);
}
