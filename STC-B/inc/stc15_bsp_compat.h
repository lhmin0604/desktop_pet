/*
 * stc15_bsp_compat.h - STC-B BSP function prototypes (SDCC compatible)
 *
 * Minimal subset of the BSP headers that our code references.
 * Avoids pulling in the C51-style BSP headers.
 */

#ifndef STC15_BSP_COMPAT_H
#define STC15_BSP_COMPAT_H

#include "struct_ADC.h"

/* ---- uart2.h ---- */
enum Uart2PortName {
    Uart2UsedforEXT = 0,
    Uart2Usedfor485,
    Uart2Usedfor485ModBus
};
enum Uart2ActName {
    enumUart2TxFree = 0,
    enumUart2TxBusy,
    enumUart2TxOK,
    enumUart2TxFailure
};
extern void Uart2Init(unsigned long band, unsigned char Uart2mode);
extern void SetUart2Rxd(void *RxdPt, unsigned int Nmax, void *matchhead, unsigned int matchheadsize);
extern char Uart2Print(void *pt, unsigned int num);
extern char GetUart2TxStatus(void);
extern unsigned char GetUart2RxNum(void);

/* ---- sys.h ---- */
enum event {
    enumEventSys1mS, enumEventSys10mS, enumEventSys100mS, enumEventSys1S,
    enumEventKey, enumEventHall, enumEventVib, enumEventNav,
    enumEventXADC, enumEventUart1Rxd, enumEventUart2Rxd, enumEventIrRxd
};
extern void MySTC_Init(void);
extern void MySTC_OS(void);
extern void SetEventCallBack(unsigned char event, void (*user_callback)(void));

/* ---- displayer.h / Beep.h / Key.h / Vib.h / hall.H / ADC.h ---- */
extern void DisplayerInit(void);
extern void SetDisplayerArea(unsigned char area_start, unsigned char area_end);
extern void Seg7Print(unsigned char s0, unsigned char s1, unsigned char s2,
                      unsigned char s3, unsigned char s4, unsigned char s5,
                      unsigned char s6, unsigned char s7);
extern void LedPrint(unsigned char led);

extern void BeepInit(void);
extern char SetBeep(unsigned int freq, unsigned int time);
extern unsigned char GetBeepStatus(void);

extern void KeyInit(void);
extern unsigned char GetKeyAct(unsigned char key);

extern unsigned char GetVibAct(void);
extern unsigned char GetHallAct(void);

extern struct_ADC GetADC(void);
extern void AdcInit(unsigned char ch);
extern unsigned char GetAdcNavAct(unsigned char key);

#endif
