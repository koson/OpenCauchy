;/*******************************************************************************
;  Easy Bootloader for PIC24 and dsPIC33 source file (ezbl_lib)
;
;  Summary:
;    Supplies default symbol values for various ezbl_lib.a APIs that depend on
;    symbols that could appear in the Linker Script (via ezbl_tools.jar) or be
;    missing and not be manually defined (ex: calling EZBL_WriteROM() without
;    any EZBL infrastructure to do EEPROM emulation without a bootloader)
;
;*******************************************************************************/
;
;/*******************************************************************************
;  Copyright (C) 2018 Microchip Technology Inc.
;
;  MICROCHIP SOFTWARE NOTICE AND DISCLAIMER:  You may use this software, and any
;  derivatives created by any person or entity by or on your behalf, exclusively
;  with Microchip's products.  Microchip and its licensors retain all ownership
;  and intellectual property rights in the accompanying software and in all
;  derivatives here to.
;
;  This software and any accompanying information is for suggestion only.  It
;  does not modify Microchip's standard warranty for its products.  You agree
;  that you are solely responsible for testing the software and determining its
;  suitability.  Microchip has no obligation to modify, test, certify, or
;  support the software.
;
;  THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES, WHETHER
;  EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED
;  WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A PARTICULAR
;  PURPOSE APPLY TO THIS SOFTWARE, ITS INTERACTION WITH MICROCHIP'S PRODUCTS,
;  COMBINATION WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.
;
;  IN NO EVENT, WILL MICROCHIP BE LIABLE, WHETHER IN CONTRACT, WARRANTY, TORT
;  (INCLUDING NEGLIGENCE OR BREACH OF STATUTORY DUTY), STRICT LIABILITY,
;  INDEMNITY, CONTRIBUTION, OR OTHERWISE, FOR ANY INDIRECT, SPECIAL, PUNITIVE,
;  EXEMPLARY, INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, FOR COST OR EXPENSE OF
;  ANY KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWSOEVER CAUSED, EVEN IF
;  MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.
;  TO THE FULLEST EXTENT ALLOWABLE BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
;  CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF
;  FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
;
;  MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
;  TERMS.
;*******************************************************************************/

    ; Declare weak absolute symbol. Being weak and in an archive, this value
    ; will not be used unless code elsewhere needs such a value and no other
    ; source files or linker script included in the linking process defines this
    ; value.
    .weak _EZBL_ADDRESSES_PER_SECTOR
;    .ifdef  ezbl_lib16ch    ; __GENERIC-16DSP-CH target, but can't test this symbol since it has dashes in it, commented out since slave is 0x400 size
;_EZBL_ADDRESSES_PER_SECTOR    = 0x800      ; Program space addresses per flash erase page - normally 0x800 on dsPIC33C devices (3072 bytes or 1024 instruction words)
;    .else
_EZBL_ADDRESSES_PER_SECTOR    = 0x400      ; Program space addresses per flash erase page - normally 0x400 or 0x800 unless dsPIC30F or PIC24F with 'K' flash. 0x400 addresses = 1536 bytes = 512 instruction words
;    .endif


