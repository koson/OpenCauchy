/*
 * @file    EZBL_TrapHandler.c
 * @summary
 *      Implements a _DefaultInterrupt() ISR handler to help debug run-time
 *      exception traps. This implementation captures and prints a lot of
 *      potentially useful system state to a console/UART via EZBL_STDOUT.
 *
 *      Example captured data includes:
 *        - w0-w15 at time of the trap (not modified as normally happens for a C
 *          handler)
 *        - SR, RCON, INTCON1, PSVPAG/DSRPAG+DSWPAG, TBLPAG, RCOUNT and trap
 *          return address
 *        - Stack RAM contents
 *        - Flash reset/interrupt vector contents and some code
 */

/*******************************************************************************
  Copyright (C) 2018 Microchip Technology Inc.

  MICROCHIP SOFTWARE NOTICE AND DISCLAIMER:  You may use this software, and any
  derivatives created by any person or entity by or on your behalf, exclusively
  with Microchip's products.  Microchip and its licensors retain all ownership
  and intellectual property rights in the accompanying software and in all
  derivatives here to.

  This software and any accompanying information is for suggestion only.  It
  does not modify Microchip's standard warranty for its products.  You agree
  that you are solely responsible for testing the software and determining its
  suitability.  Microchip has no obligation to modify, test, certify, or
  support the software.

  THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES, WHETHER
  EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED
  WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A PARTICULAR
  PURPOSE APPLY TO THIS SOFTWARE, ITS INTERACTION WITH MICROCHIP'S PRODUCTS,
  COMBINATION WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.

  IN NO EVENT, WILL MICROCHIP BE LIABLE, WHETHER IN CONTRACT, WARRANTY, TORT
  (INCLUDING NEGLIGENCE OR BREACH OF STATUTORY DUTY), STRICT LIABILITY,
  INDEMNITY, CONTRIBUTION, OR OTHERWISE, FOR ANY INDIRECT, SPECIAL, PUNITIVE,
  EXEMPLARY, INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, FOR COST OR EXPENSE OF
  ANY KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWSOEVER CAUSED, EVEN IF
  MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.
  TO THE FULLEST EXTENT ALLOWABLE BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
  CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF
  FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

  MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
  TERMS.
*******************************************************************************/


#include "../ezbl.h"

// Declarations for device SFRs, normally declared in the device
// header file/linker script or generated by the linker. Not all registers exist
// on all devices, so in such cases linking is permitted to continue with run
// time suppression preventing unimplemented registers from being dumped.
extern __attribute__((near))        volatile unsigned int SR;
extern __attribute__((near))        volatile unsigned int CORCON;
extern __attribute__((near))        volatile unsigned int RCON;
extern __attribute__((near))        volatile unsigned int INTCON1;
extern __attribute__((near))        volatile unsigned int SPLIM;
extern __attribute__((near, weak))  volatile unsigned int DSRPAG;
extern __attribute__((near))        volatile unsigned int TBLPAG;
extern __attribute__((near, weak))  volatile unsigned int ACCAL[3];
extern __attribute__((near, weak))  volatile unsigned int ACCBL[3];
extern __attribute__((near, weak))  volatile unsigned int DCOUNT;
extern __attribute__((near, weak))  volatile unsigned long DOSTARTL;
extern __attribute__((near, weak))  volatile unsigned long DOENDL;
extern __attribute__((near))        volatile unsigned int DISICNT;
extern __attribute__((near, weak))  volatile unsigned int CTXTSTAT;
extern __attribute__((near))        volatile unsigned int IFS0;
extern __attribute__((near))        volatile unsigned int IEC0;
extern __attribute__((near))        volatile unsigned int IPC0;
extern unsigned int _SP_init;
extern unsigned int _DATA_BASE;


// Declare extra symbol to hook up to the IVT trap handler slots. Having these
// causes the same EZBL_TrapHandler() interrupt handler to be called for any of
// the named trap vectors, assuming a non-weak trap handler of the same name
// doesn't already exist in the project. In such cases, the explicitly defined
// trap handler is used for the IVT slot and only non-specified ones map to
// EZBL_TrapHandler().
extern void __attribute__((weak, alias("EZBL_TrapHandler"))) _DefaultInterrupt(void);  
extern void __attribute__((weak, alias("EZBL_TrapHandler"))) _OscillatorFail(void);
extern void __attribute__((weak, alias("EZBL_TrapHandler"))) _AddressError(void);
extern void __attribute__((weak, alias("EZBL_TrapHandler"))) _HardTrapError(void);
extern void __attribute__((weak, alias("EZBL_TrapHandler"))) _StackError(void);
extern void __attribute__((weak, alias("EZBL_TrapHandler"))) _MathError(void);
extern void __attribute__((weak, alias("EZBL_TrapHandler"))) _DMACError(void);
extern void __attribute__((weak, alias("EZBL_TrapHandler"))) _SoftTrapError(void);
extern void __attribute__((weak, alias("EZBL_TrapHandler"))) _NVMError(void);
extern void __attribute__((weak, alias("EZBL_TrapHandler"))) _GeneralError(void);
extern void __attribute__((weak, alias("EZBL_TrapHandler"))) _ReservedTrap7(void);


// Formatting string contents to highlight special items in red text (with
// ANSI capable terminal emulator software on the PC)
#define ANSI_CLR    "\x1B[0m"   // \x1B[0m = ANSI Reset All Attributes
#define ANSI_RED    "\x1B[31m"  // \x1B[31m = ANSI red text


#define BINARY      0x0002  // Constant for printReg()'s formatFlags parameter indicating the decoded hexidecimal value should be displayed alongside a binary decoding.
#define DECIMAL     0x0004  // Constant for printReg()'s formatFlags parameter indicating the decoded hexidecimal value should be displayed alongside a signed decimal decoding.
#define UNSIGNED    0x0008  // Constant for printReg()'s formatFlags parameter indicating the decoded hexidecimal value should be displayed alongside an unsigned decimal decoding. If DECIMAL and UNSIGNED are both defined, and the value equates to a positive number (which displays the same with decimal and unsigned decimal decodings), then the output is only printed once.

static void printReg(const char *regName, volatile void *reg, int formatFlags);


// Static RAM for capturing the critical system state that the compiler would
// normally overwrite immediately at trap ISR execution and not let us capture.
static struct
{
    unsigned int w[16];
    unsigned int rcount;
    union
    {
        unsigned int psvpag;
        unsigned int dsrpag;
    };
    unsigned int dswpag;
    unsigned int sr;
} contextRegs __attribute__((update, persistent));


/**
 * Powerful trap exception ISR handler to help debug run-time exception
 * traps. This implementation captures and prints a greater than normal amount
 * of potentially useful system state to a console/UART via stdout.
 *
 * Before a trap occurs, a UART or other interface must be initialized to handle
 * stdout. This can be accomplished with the UART_Reset() ezbl_lib.a macro.
 *
 * Example captured data includes:
 *   - w0-w15 at time of the trap (not modified as normally happens for a C
 *     handler)
 *   - SR, RCON, INTCON1, PSVPAG/DSRPAG+DSWPAG, TBLPAG, RCOUNT and trap
 *     return address
 *   - The opcodes immediately near the trap's return address.
 *   - IFS/IEC/IPC SFR state bits, with interrupts having both the IEC Interrupt
 *     Enable bit set and IFS Interrupt Flag bit set highlighted as red text.
 *   - RAM and Stack contents
 *   - Flash reset/interrupt vector contents and some code
 *
 * To use this handler, at global file scope within a .c file in the project,
 * insert:
 *      EZBL_KeepSYM(_DefaultInterrupt);
 *      EZBL_KeepSYM(_AddressError);
 *      EZBL_KeepSYM(_MathError);
 *      etc.
 * Each statement hooks the corresponding IVT entry for the trap to the common
 * EZBL_TrapHandler() ISR function, assuming a named trap handler ISR function
 * doesn't already exist that matches the kept symbol.
 *
 * @return void via RETFIE and with the trap flag bits in INTCON1 cleared.
 *
 */
void __attribute__((keep, auto_psv, interrupt(preprologue("\n"
    "\n     .weak   _DSRPAG"
    "\n     .weak   _DSWPAG"
    "\n     .weak   _PSVPAG"
    "\n     mov w0, _contextRegs+0"     // w0
    "\n     mov w1, _contextRegs+2"     // w1
    "\n     mov _RCOUNT, w1"            // RCOUNT
    "\n     mov w1, _contextRegs+32"
    "\n     mov #30, w1"                // Get &WREG15 (stack pointer) without having XC16 compiler issue warning about unsafe SFR access (this code is safe)
    "\n     mov #(_contextRegs+30), w0"
    "\n     repeat #13"
    "\n     mov [w1--], [w0--]"         // w15..w2
    "\n     mov _PSVPAG, w0"            // PSVPAG (note: this is a mov w0, w0 instruction if DSRPAG/DSWPAG is implemented instead on this device)
    "\n     mov _DSRPAG, w0"            // DSRPAG (note: this is a mov w0, w0 instruction if PSVPAG is implemented instead on this device)
    "\n     mov w0, _contextRegs+34"
    "\n     mov _DSWPAG, w0"            // DSWPAG (if applicable)
    "\n     mov w0, _contextRegs+36"
    "\n     mov _SR, w0"                // SR (note: SR has already been adjusted by hardware exception logic by changing the IPL/SFA/RA/DA bits. The pre-trap SR is on the stack)
    "\n     mov w0, _contextRegs+38"
    "\n     mov _contextRegs+32, w1"    // Restore original RCOUNT so we don't corrupt it if we return from this handler
    "\n     mov w1, _RCOUNT"
    "\n     mov _contextRegs+2, w1"
    "\n     mov _contextRegs+0, w0"
)))) EZBL_TrapHandler(void)
{
    unsigned long retInstr[3];
    unsigned long frameRetAddr;
    unsigned long retAddr;
    unsigned int retIPL;
    unsigned int retSR;
    unsigned int retSFA;
    unsigned int i, j, k;
    volatile unsigned int *intFlagBits, *intEnableBits, *intPriBits;
    unsigned int readFlag, readEnable;
    char regName[4];

    contextRegs.w[15] -= 0x4u;    // Subtract 0x4 from the stack pointer to remove ISR return address/SR data from our debug report
    EZBL_RAMCopy(&retAddr, (unsigned long*)contextRegs.w[15], 4);               // Get ISR return address/SR data and decode the fields in it
    retSR = (unsigned int)(((unsigned char*)&retAddr)[3]);
    retIPL = (unsigned int)(((((unsigned char*)&retAddr)[2]) & 0x80)>>4) | (retSR>>5);
    retSFA = ((unsigned int)retAddr) & 0x1u;
    retAddr = retAddr & 0x007FFFFEu;                                            // This is the trap return address, which is the instruction just after the one that triggered the exception (when exception is code related)

    EZBL_printf("\n\n%s():", __func__);
    regName[0] = 'w';
    regName[2] = 0;
    regName[3] = 0;
    for(i = 0; i < 16; i++)
    {
        regName[1] = '0' + i;
        if(i >= 10u)
        {
            regName[2] = '0' + (i-10u);
            regName[1] = '1';
        }
        printReg(regName, &contextRegs.w[i], DECIMAL | UNSIGNED);
    }
    printReg("SPLIM",   &SPLIM, 0);
    printReg("RCON",    &RCON, BINARY);
    printReg("INTCON1", &INTCON1, BINARY);
    printReg("SR",      &contextRegs.sr, BINARY);
    printReg("CORCON",  &CORCON, BINARY);
    printReg("RCOUNT",  &contextRegs.rcount, UNSIGNED);
    printReg("DISICNT", &DISICNT, UNSIGNED);
    printReg("TBLPAG",  &TBLPAG, 0);
    printReg(&DSRPAG ? "DSRPAG" : "PSVPAG", &contextRegs.dsrpag, 0);
    if(&DSRPAG)
        printReg("DSWPAG", &contextRegs.dswpag, 0);

    if(ACCAL)
    {
        EZBL_printf("\n  ACCA     0x%04X%04X%04X"
                    "\n  ACCB     0x%04X%04X%04X"
                    "\n  DCOUNT   0x%04X (%u)"
                    "\n  DSTART   0x%06lX"
                    "\n  DEND     0x%06lX",
                    ACCAL[2], ACCAL[1], ACCAL[0], ACCBL[2], ACCBL[1], ACCBL[0], DCOUNT, DCOUNT, DOSTARTL, DOENDL);
    }
    if(&CTXTSTAT)
        printReg("CTXTSTAT", &CTXTSTAT, 0);

    EZBL_printf("\n"
                "\nTrap return address: 0x%06lX (SFA = %X, IPL = %X, SR<7:0> = 0x%02X)",
                retAddr, retSFA, retIPL, retSR);

    EZBL_ReadUnpacked(retInstr, retAddr-0x4, sizeof(retInstr));
    EZBL_printf("\n  Preceeding opcode:   %06lX  0x%06lX"
                "\n  Trap trigger opcode: %06lX  " ANSI_RED "0x%06lX" ANSI_CLR
                "\n  Trap return opcode:  %06lX  0x%06lX",
                retAddr-0x4, retInstr[0], retAddr-0x2, retInstr[1], retAddr, retInstr[2]);

    if(contextRegs.w[14] && retSFA)
    {
        EZBL_RAMCopy(&frameRetAddr, (unsigned long*)(contextRegs.w[14] - 6u), 4);
        frameRetAddr &= 0xFFFFFFFEu;
        EZBL_printf("\n  Stack frame was active. They return to: 0x%06lX", frameRetAddr & 0xFFFFFFFEu);
    }

    intFlagBits = &IFS0;
    intEnableBits = &IEC0;
    intPriBits = &IPC0;
    EZBL_printf("\n\nInterrupt SFRs (red indicates flag and enable set):");
                  //"\n  111111           111111           111111           111111"
                  //"\n  5432109876543210 5432109876543210 5432109876543210 5432109876543210");
    for(i = 0; i < ((unsigned int)(&IEC0 - &IFS0)); i += 4u)
    {
        // Header row
        EZBL_printf("\n"
                    "\n       ");
        for(k = 3; ((int)k) >= 0; k--)
        {
            j = EZBL_printf("IFS%u/IEC%u/IPC%u", i+k, i+k, (i+k)*4u);
            if(k > 0)
            {
                while(j++ < 19u)
                    EZBL_printf(" ");
            }
        }

        // IFSx row
        EZBL_printf("\n  IFS  ");
        intFlagBits += 4u;
        intEnableBits += 4u;
        for(k = 0; k < 4u; k++)
        {
            readFlag = *--intFlagBits;
            readEnable = *--intEnableBits;
            j = 0x8000;
            do
            {
                if(readFlag & j)
                {
                    if(readEnable & j)
                        EZBL_printf(ANSI_RED "1" ANSI_CLR);
                    else
                        EZBL_printf("1");
                }
                else
                {
                    EZBL_printf("0");
                }
                j >>= 1;
            } while(j != 0x0000u);
            if(k < 3u)
                EZBL_printf("   ");
        }
        intFlagBits += 4u;

        // IECx row
        EZBL_printf("\n  IEC  ");
        intEnableBits += 4u;
        for(k = 0; k < 4u; k++)
        {
            readEnable = *--intEnableBits;
            j = 0x8000;
            do
            {
                EZBL_printf("%X", (readEnable & j) != 0);
                j >>= 1;
            } while(j != 0x0000u);
            if(k < 3u)
                EZBL_printf("   ");
        }
        intEnableBits += 4u;

        // IPCx*4 row
        EZBL_printf("\n  IPC  ");
        intPriBits += 16u;
        for(k = 0; k < 4u; k++)
        {
            EZBL_printf("%04X", *--intPriBits);
            EZBL_printf("%04X", *--intPriBits);
            EZBL_printf("%04X", *--intPriBits);
            EZBL_printf("%04X", *--intPriBits);
            if(k < 3u)
                EZBL_printf("   ");
        }
        intPriBits += 16u;

        EZBL_printf("\n");
    }

    EZBL_printf("\nRegular RAM:");
    EZBL_DumpRAM((void*)&_DATA_BASE, (unsigned int)&_SP_init - (unsigned int)&_DATA_BASE);

    EZBL_printf("\nStack RAM:");
    EZBL_DumpRAM(&_SP_init, contextRegs.w[15] - ((unsigned int)&_SP_init));

    EZBL_printf("\nFirst page of flash:");
    EZBL_DumpFlash(0x000000, EZBL_SYM32(EZBL_ADDRESSES_PER_SECTOR));

    // Clear trap(s), if any, and return to see what the code was doing when
    // this interrupt was triggered
    INTCON1 &= 0x8700;

    EZBL_printf("\nAttempting to return from trap handler...");
    EZBL_FIFOFlush(EZBL_STDOUT, NOW_sec);

    if(EZBL_WeakSYM(_DEBUG))
    {
        EZBL_printf("\nOn second thought, you are in debug mode, so let's halt instead.");
        EZBL_FIFOFlush(EZBL_STDOUT, NOW_sec);
        __builtin_nop();
        __builtin_software_breakpoint();
        __builtin_nop();
        __builtin_nop();
    }
}


/**
 * Helper function displaying an int aligned, unsigned int SFR or integer RAM
 * value in human readable format with hexidecimal ASCII printing + optional
 * binary/decimal/unsigned decimal decoding.
 *
 * @param *regName Pointer to null terminated string for the SFR or variable's
 *                 name to be displayed before its value.
 *
 *                 Should be <= 8 characters, not including the null terminator
 *                 for neat alignment.
 *
 *                 If this parameter is null, the hexidecimal register/RAM
 *                 address is displayed as the name.
 *
 * @param *reg Pointer to the SFR or RAM location that will be read to obtain
 *             the value to display.
 *
 * @param formatFlags Combination of optional BINARY, DECIMAL and/or UNSIGNED
 *                    constants to display the value with alternate formats.
 *
 *                    For values that are 0x0000, this parameter is ignored and
 *                    only the default hex value is shown.
 *
 *                    To combine more than one format option, bitwise OR them
 *                    together, ex: BINARY | UNSIGNED
 *
 * @return void
 *
 *         A one line string is printed to stdout/EZBL_STDOUT, such as:
 *              "  TBLPAG   0x0001 (1)"
 *         or
 *              "  CORCON   0x1234   00010010 00110100"
 */
static void printReg(const char *regName, volatile void *reg, int formatFlags)
{
    unsigned int i;
    unsigned int readData;

    readData = *((unsigned int*)reg);

    if(regName == (void*)0)
        EZBL_printf("\n  %04X     0x%04X", (unsigned int)reg, readData);
    else
        EZBL_printf("\n  %- 9s0x%04X", regName, readData);

    if(formatFlags && (readData != 0u))
    {
        if(formatFlags & BINARY)
        {
            EZBL_printf("   ");
            i = 0x8000u;
            do
            {
                EZBL_printf("%X", (readData & i) != 0u);
                if(i == 0x0100u)
                    EZBL_printf(" ");
                i >>= 1;
            } while(i);
        }
        if(formatFlags & DECIMAL)
        {
            formatFlags ^= DECIMAL;
            if((((int)readData < 0) || (~formatFlags & UNSIGNED)))
            {
                EZBL_printf("   %6d", readData);
            }
        }
        if(formatFlags & UNSIGNED)
        {
            EZBL_printf("   %5u", readData);
            formatFlags ^= UNSIGNED;
        }
    }

    const char **regBitNames = 0;
    const char *INTCON1BitNames[] = {"", "OSCFAIL", "STKERR", "ADDRERR", "MATHERR", "DMACERR", "DIV0ERR", "SFTACERR", "COVTE", "OVBTE", "OVATE", "COVBERR", "COVAERR", "OVBERR", "OVAERR", "NSTDIS"};
    const char *RCONBitNames[] = {"POR", "BOR", "IDLE", "SLEEP", "WDTO", "SWDTEN", "SWR", "EXTR", "PMSLP/VREGS", "CM", "DPSLP", "VREGSF", "RETEN", "SBOREN", "IOPUWR", "TRAPR"};
    const char *SRBitNames[] = {"C", "Z", "OV", "N", "RA", "IPL[0]", "IPL[1]", "IPL[2]", "DC", "DA", "SAB", "OAB", "SB", "SA", "OB", "OA"};
    if(reg == (void*)&INTCON1)
        regBitNames = INTCON1BitNames;
    else if(reg == (void*)&RCON)
        regBitNames = RCONBitNames;
    else if(reg == (void*)&SR)
        regBitNames = SRBitNames;
    
    if(regBitNames)
    {
        if(readData)
        {
            EZBL_printf(" {");
            for(i = 0; i < 16u; i++)
            {
                if(readData & (0x1<<(15u-i)))
                {
                    readData ^= (0x1<<(15u-i));
                    EZBL_printf("%s%s", regBitNames[15u-i], readData ? ", " : "");
                }
            }
            EZBL_printf("}");
        }
        else
            EZBL_printf(" {none}");
    }
}
