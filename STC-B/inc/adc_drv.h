/*
 * adc_drv.h - ADC 驱动 (SDCC)
 *
 * 通道: ADC2=P1.2(五向导航), ADC6=P1.6(温度), ADC7=P1.7(光照)
 * 10位 ADC, 1T 模式
 */
#ifndef ADC_DRV_H
#define ADC_DRV_H

#include "struct_ADC.h"

#define ADC_CH_NAV    2    /* P1.2 - 五向导航键 */
#define ADC_CH_TEMP   6    /* P1.6 - 温度传感器 */
#define ADC_CH_LIGHT  7    /* P1.7 - 光照传感器 */

/* 五向导航方向 */
#define NAV_NONE     0
#define NAV_UP       1
#define NAV_DOWN     2
#define NAV_LEFT     3
#define NAV_RIGHT    4
#define NAV_CENTER   5

void adc_init(void);
unsigned int adc_read(unsigned char ch);
unsigned char adc_get_nav_dir(unsigned int val);
void adc_poll(void);        /* 每10ms调用, 轮流采样 */

#endif

/* 便捷函数: 直接读取温度/光照 ADC 值 */
unsigned int adc_get_temp(void);
unsigned int adc_get_light(void)  /* TODO: ADC7暂禁用, 返回0 */;
