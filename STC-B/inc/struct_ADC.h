/*
 * struct_ADC.h - 简化版 BSP 类型定义 (SDCC 兼容)
 *
 * 原 BSP 头文件用了大量 C51 专属宏, 我们这里只摘出需要的类型.
 */

#ifndef STRUCT_ADC_H_SDCC
#define STRUCT_ADC_H_SDCC

typedef struct {
    unsigned int Rt;     /* 热敏电阻 */
    unsigned int Rop;    /* 光敏电阻 */
} struct_ADC;

#endif
