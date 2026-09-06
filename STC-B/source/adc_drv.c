/*
 * adc_drv.c - ADC 驱动 (SDCC)
 *
 * STC15F2K60S2 10-bit ADC
 * ADC2=P1.2 (五向导航, 电阻分压)
 * ADC6=P1.6 (温度 NTC)
 * ADC7=P1.7 (光照 LDR)
 *
 * 五向导航阈值 (10-bit, 3.3V ref, R31-R36 分压):
 *   CENTER: 0-30,  RIGHT: 31-130,  UP: 131-280
 *   DOWN: 281-480, LEFT: 481-680,  释放: >680
 *   注: 阈值需上板校准
 */
#include "stc15_sdcc.h"
#include "adc_drv.h"

static unsigned char adc_ch_seq[2] = { ADC_CH_NAV, ADC_CH_TEMP };
static unsigned char adc_seq_idx = 0;
static unsigned int  adc_val_nav = 0;
static unsigned int  adc_val_temp = 0;
static unsigned int  adc_val_light = 0;

/* 导航阈值 (需上板校准) */
#define NAV_TH_CENTER   30
#define NAV_TH_RIGHT    130
#define NAV_TH_UP       280
#define NAV_TH_DOWN     480
#define NAV_TH_LEFT     680

void adc_init(void)
{
    P1ASF = 0x44;          /* ADC2, ADC6 模拟输入 (ADC7禁用:与K3共用P1.7) */
    ADCCFG = 0xA0;          /* ADC clock = FOSC/2/16 */
    ADC_CONTR = 0x80;       /* ADC power on */
}

unsigned int adc_read(unsigned char ch)
{
    unsigned int result;

    ADC_CONTR = 0x80 | 0x20 | ch;   /* ADC_POWER | START | channel */
    while (!(ADC_CONTR & 0x10));     /* wait DONE */
    ADC_CONTR &= ~0x10;              /* clear DONE */

    result = ((unsigned int)ADC_RES << 2) | (ADC_RESL >> 6);
    return result;
}

unsigned char adc_get_nav_dir(unsigned int val)
{
    if (val <= NAV_TH_CENTER)  return NAV_CENTER;
    if (val <= NAV_TH_RIGHT)   return NAV_RIGHT;
    if (val <= NAV_TH_UP)      return NAV_UP;
    if (val <= NAV_TH_DOWN)    return NAV_DOWN;
    if (val <= NAV_TH_LEFT)    return NAV_LEFT;
    return NAV_NONE;
}

void adc_poll(void)
{
    unsigned char ch = adc_ch_seq[adc_seq_idx];
    unsigned int val = adc_read(ch);

    switch (ch)
    {
        case ADC_CH_NAV:   adc_val_nav   = val; break;
        case ADC_CH_TEMP:  adc_val_temp  = val; break;
    }
    adc_seq_idx++;
    if (adc_seq_idx >= 2) adc_seq_idx = 0;
}


unsigned int adc_get_temp(void)
{
    return adc_val_temp;
}

unsigned int adc_get_light(void)  /* TODO: ADC7暂禁用, 返回0 */
{
    return 0;  /* ADC7 disabled: conflicts with K3 on P1.7 */
}
