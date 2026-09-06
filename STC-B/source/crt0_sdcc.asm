; crt0_sdcc.asm - SDCC 启动代码 (STC15F2K60S2)
;
; SDCC 用 sdas8051 汇编, 语法跟 A51 不同.
; 关键点:
;   - 默认 crt0 已经被 sdcc 链接, 但我们想让 main() 在上电后能跑,
;     且 UART2 ISR 放到正确的向量地址 0x8B.
;   - 这个文件是 OPTIONAL 的 — 如果你只是用 SDCC 默认的 crt0, 中断
;     用 __interrupt(N) 属性就够了, SDCC 会自动生成 LJMP.
;   - 这里写一个 minimal startup, 只设置 SP, 清 IDATA, 调 main.
;
; 编译:
;   sdas8051 -plosgff crt0_sdcc.asm
;   得到 crt0_sdcc.rel

                .module crt0
                .globl  _main

                .area   CSEG    (REL,CON)
                .area   INIT    (REL,CON)
                .area   XINIT   (REL,CON)
                .area   GSINIT  (REL,CON)
                .area   GSFINAL (REL,CON)
                .area   CABS    (ABS,CON)

                .area   SSEG    (REL,CON)
                .area   PSEG    (REL,CON)
                .area   XSEG    (REL,CON)
                .area   ISEG    (REL,CON)
                .area   BSEG    (REL,CON)
                .area   RSEG    (ABS,CON)

; 复位向量: 上电跳到 init
                .area   RSEG    (ABS,CON)
                .org    0x0000
                ljmp    _sdcc_init

; 中断向量: 不用手写, SDCC 通过 __interrupt(N) 自动生成
; (会在 link 时插入到对应地址)

                .area   GSINIT  (REL,CON)
_sdcc_init:
                mov     sp, #0x7F        ; stack top, 256 bytes IDATA
                ; clear IDATA 0x00..0x7F
                mov     r0, #0x7F
                clr     a
clr_idata:
                mov     @r0, a
                djnz    r0, clr_idata

                ; call main() directly (no _sdcc_external_startup)
                lcall   _main

                ; should never reach here
here:
                sjmp    here
