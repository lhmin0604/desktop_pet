/*
 * stc15_sdcc.h - STC15F2K60S2 SFR 定义 (SDCC 兼容版)
 *
 * 不直接包含 STC15F2K60S2.H (那是 C51 风格, sfr/sbit), 我们用 SDCC 的
 * __sfr / __sbit 重写. 这样不会跟 BSP 其他头文件冲突.
 *
 * 编译选项: sdcc -mmcs51 --model-large
 */

#ifndef STC15_SDCC_H
#define STC15_SDCC_H

/* ============ 端口 ============ */
__sfr __at (0x80) P0;
__sfr __at (0x90) P1;
__sfr __at (0xA0) P2;
__sfr __at (0xB0) P3;
__sfr __at (0xC0) P4;
__sfr __at (0xC8) P5;
__sfr __at (0xE8) P6;
__sfr __at (0xF8) P7;

__sbit __at (0x80) P00;
__sbit __at (0x81) P01;
__sbit __at (0x82) P02;
__sbit __at (0x83) P03;
__sbit __at (0x84) P04;
__sbit __at (0x85) P05;
__sbit __at (0x86) P06;
__sbit __at (0x87) P07;
__sbit __at (0x90) P10;
__sbit __at (0x91) P11;
__sbit __at (0x92) P12;
__sbit __at (0x93) P13;
__sbit __at (0x94) P14;
__sbit __at (0x95) P15;
__sbit __at (0x96) P16;
__sbit __at (0x97) P17;
__sbit __at (0xA0) P20;
__sbit __at (0xA1) P21;
__sbit __at (0xA2) P22;
__sbit __at (0xA3) P23;
__sbit __at (0xA4) P24;
__sbit __at (0xA5) P25;
__sbit __at (0xA6) P26;
__sbit __at (0xA7) P27;
__sbit __at (0xB0) P30;
__sbit __at (0xB1) P31;
__sbit __at (0xB2) P32;
__sbit __at (0xB3) P33;
__sbit __at (0xB4) P34;
__sbit __at (0xB5) P35;
__sbit __at (0xB6) P36;
__sbit __at (0xB7) P37;

/* ============ 核心 SFR ============ */
__sfr __at (0x81) SP;
__sfr __at (0x82) DPL;
__sfr __at (0x83) DPH;
__sfr __at (0x87) PCON;        /* 0x87 电源控制 (STC15 改写) */
__sfr __at (0x88) TCON;
__sfr __at (0x89) TMOD;
__sfr __at (0x8A) TL0;
__sfr __at (0x8B) TL1;
__sfr __at (0x8C) TH0;
__sfr __at (0x8D) TH1;
__sfr __at (0x8E) AUXR;
__sfr __at (0x8F) WAKE_CLKO;   /* STC15 特有 */
__sfr __at (0xE0) ACC;
__sfr __at (0xF0) B;
__sfr __at (0xD0) PSW;

/* ============ 中断 ============ */
__sfr __at (0xA8) IE;
__sfr __at (0xA9) SADDR;
__sfr __at (0xAA) WKTCL;
__sfr __at (0xAB) WKTCH;
__sfr __at (0xAC) S3CON;
__sfr __at (0xAD) S3BUF;
__sfr __at (0xAE) S4CON;
__sfr __at (0xAF) IE2;          /* bit0=ES2 (UART2), bit1=ES3, bit2=ES4 */
__sfr __at (0xB8) IP;
__sfr __at (0xB9) SADEN;
__sfr __at (0xBA) P_SW2;        /* bit0=S2_S: 0=P1, 1=P4 (注: STC15F2K60S2 上 S2_S=1 切到 P4.6/P4.7) */
__sfr __at (0xBB) P_SW1;
__sfr __at (0xBC) ADC_CONTR;
__sfr __at (0xBD) ADC_RES;
__sfr __at (0xBE) ADC_RESL;
__sfr __at (0xBF) S4BUF;
__sfr __at (0xB5) IP2;          /* 0xB5 IP2 (STC15 改写) */
__sfr __at (0xB6) IP2H;
__sfr __at (0xB7) IPH;
__sfr __at (0xC1) WDT_CONTR;
__sfr __at (0xC2) IAP_DATA;
__sfr __at (0xC3) IAP_ADDRH;
__sfr __at (0xC4) IAP_ADDRL;
__sfr __at (0xC5) IAP_CMD;
__sfr __at (0xC6) IAP_TRIG;
__sfr __at (0xC7) IAP_CONTR;
__sfr __at (0xC9) P5M0;
__sfr __at (0xCA) P5M1;
__sfr __at (0xCB) P6M0;
__sfr __at (0xCC) P6M1;
__sfr __at (0xCD) P7M0;
__sfr __at (0xCE) P7M1;

/* 中断使能位 (IE) - bit 地址 = SFR 起始 + bit 位 */
__sbit __at (0xAF) EA;           /* IE bit 7 */
__sbit __at (0xA8) ES0;          /* IE bit 0 (保留) */
__sbit __at (0xA9) ET0;          /* IE bit 1 */
__sbit __at (0xAA) EX1;          /* IE bit 2 */
__sbit __at (0xAB) ET1;          /* IE bit 3 */
__sbit __at (0xAC) ES1;          /* IE bit 4 (UART1) */
__sbit __at (0xAD) ET2;          /* IE bit 5 */
__sbit __at (0xAF) ES2;          /* IE2 bit 0, 但因为 IE2 在 0xAF 跟 EA 同位, 这里重新命名 */

/* ============ UART2 ============ */
__sfr __at (0x9A) S2CON;        /* bit7=S2SM0 bit5=S2REN bit1=S2TI bit0=S2RI */
__sfr __at (0x9B) S2BUF;
__sfr __at (0x9C) BRT;          /* 独立波特率发生器 (STC15 特有) */

/* ============ 定时器 ============ */
__sfr __at (0xD1) T2L;
__sfr __at (0xD2) T2H;
__sfr __at (0xD3) T3L;
__sfr __at (0xD4) T3H;
__sfr __at (0xD5) T4L;
__sfr __at (0xD6) T4H;
__sfr __at (0xD7) T4T3M;
__sfr __at (0xD9) WDT_CNT;
__sfr __at (0xDE) ADCCFG;       /* 实际是 0xDE, STC15 扩展 */
__sfr __at (0xDF) DPS;

/* ============ 端口模式 (STC15 特有) ============ */
__sfr __at (0x91) P1M1;
__sfr __at (0x92) P1M0;
__sfr __at (0x93) P0M1;
__sfr __at (0x94) P0M0;
__sfr __at (0x95) P2M1;
__sfr __at (0x96) P2M0;
__sfr __at (0x97) P3M1;
__sfr __at (0xB1) P3M0;
__sfr __at (0xB3) P4M1;
__sfr __at (0xB4) P4M0;

/* ============ 关键 macro ============ */
/* STC15 ISR interrupt numbers:
 *   0  INT0
 *   1  T0
 *   2  INT1
 *   3  T1
 *   4  UART1
 *   5  ADC
 *   6  LVD
 *   7  PCA
 *   8  UART2
 *   9  SPI
 *  10  INT2
 *  11  INT3
 *  12  T2
 *  13  --reserved--
 *  14  INT4
 */

/* ============ 关键 sbit (跟 paw_box 协议对齐) ============ */
__sbit __at (0x8E) TR1;          /* TCON bit6 */
__sbit __at (0x8F) TF1;          /* TCON bit7 */
__sbit __at (0x8C) TR0;
__sbit __at (0x8D) TF0;
__sbit __at (0xB7) RS485_DE;     /* P3.7 - 485 方向控制 (跟 TXD2 共用, 收发切换时手动控制) */

#endif /* STC15_SDCC_H */
