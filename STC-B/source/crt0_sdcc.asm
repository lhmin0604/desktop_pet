; crt0_sdcc.asm - Custom startup for STC15F2K60S2 (SDCC)
;
; FIX: Define __sdcc_gsinit_startup (the ACTUAL entry point at 0x0000)
; instead of __sdcc_external_startup (which the linker discarded).
;
; SDCC default startup bug: genXINIT/genXRAMCLEAR use broken double-loop
; when XSEG size has both high and low bytes non-zero (our XSEG = 0x010E).
; genXRAMCLEAR only clears 28 bytes (should be 270), and genXINIT
; overwrites XISEG with 0xFF from beyond the XINIT section.
;
; Our fix: Replace __sdcc_gsinit_startup entirely. Do correct 16-bit
; XDATA init, then jump to _main.
;
; Linker symbols (from SDCC runtime):
;   s_XSEG, l_XSEG     - XDATA BSS start and length
;   s_XISEG             - XDATA initialized area start
;   s_XINIT, l_XINIT   - CODE source for XISEG
;
; Memory layout (from .map):
;   XSEG:  0x0001, 0x010E (270 bytes)
;   XISEG: 0x010F, 0x0026 (38 bytes)
;   XINIT: 0x1222, 0x0026 (38 bytes)

                .module crt0
                .globl  _main
                .globl  __sdcc_gsinit_startup

                .area   CSEG    (REL,CON)
                .area   XINIT   (REL,CON)
                .area   GSINIT  (REL,CON)
                .area   GSFINAL (REL,CON)
                .area   XSEG    (REL,CON)
                .area   XISEG   (REL,CON)
                .area   RSEG    (ABS,CON)

;================================================================
; __sdcc_gsinit_startup - THE reset entry point (replaces library)
;================================================================
; Placed in HOME area at 0x0000 by the linker.
; CPU resets -> 0x0000 -> our code runs.
;================================================================
                .area   HOME    (REL,CON)
__sdcc_gsinit_startup:

    ; === 1. Stack pointer ===
    mov     sp, #0x7F             ; 256-byte IDATA stack

    ; === 2. Clear IDATA 0xFF -> 0x01 ===
    mov     r0, #0xFF
    clr     a
_idata_clr:
    mov     @r0, a
    djnz    r0, _idata_clr

    ; === 3. Clear XSEG (XDATA BSS) ===
    ; Proper 16-bit counter: l_XSEG bytes from s_XSEG
    mov     r0, #l_XSEG           ; low byte
    mov     r1, #(l_XSEG >> 8)    ; high byte
    mov     a, r0
    orl     a, r1
    jz      _xseg_done

    mov     dptr, #s_XSEG
    clr     a

_xseg_outer:
    cjne    r0, #0, _xseg_inner   ; low!=0: inner loop
                                    ; low==0: R0=0 means 256 iters
_xseg_inner:
    movx    @dptr, a
    inc     dptr
    djnz    r0, _xseg_inner
    djnz    r1, _xseg_outer

_xseg_done:

    ; === 4. Copy XINIT -> XISEG ===
    ; Proper 16-bit counter: l_XINIT bytes
    mov     r0, #l_XINIT          ; low byte
    mov     r1, #(l_XINIT >> 8)   ; high byte
    mov     a, r0
    orl     a, r1
    jz      _xinit_done

    mov     dptr, #s_XINIT        ; source in CODE
    mov     r2, #(s_XISEG >> 8)   ; dest high byte for MOVX

_xinit_outer:
    cjne    r0, #0, _xinit_inner
    sjmp    _xinit_skip            ; low==0: 256 iters, skip CJNE
_xinit_inner:
    mov     a, #0x00
    movc    a, @a+dptr
    inc     dptr
    mov     P2, r2
    movx    @r0, a
    djnz    r0, _xinit_inner
_xinit_skip:
    djnz    r1, _xinit_outer

_xinit_done:

    ; === 5. Jump to main (no return) ===
    ljmp    _main
