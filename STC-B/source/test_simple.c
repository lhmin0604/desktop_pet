/*
 * test_simple.c - 简化测试固件
 * 只测试：按键 + 数码管显示 + UART1调试输出
 * 不包含：485通信、ADC、传感器等
 * 
 * 功能：
 * - 按键K1/K2/K3按下时，数码管显示1/2/3
 * - 串口输出按键信息
 */

#include "stc15_sdcc.h"

// ============ 数码管段码表（共阴） ============
const unsigned char seg_table[] = {
    0x3F,  // 0
    0x06,  // 1
    0x5B,  // 2
    0x4F,  // 3
    0x66,  // 4
    0x6D,  // 5
    0x7D,  // 6
    0x07,  // 7
    0x7F,  // 8
    0x6F,  // 9
    0x00   // 灭
};

// ============ 全局变量 ============
volatile unsigned char sys_10ms = 0;
unsigned char display_num = 0;  // 当前显示的数字（0-9，10=灭）

// ============ 延时函数 ============
void delay_ms(unsigned int ms) {
    unsigned int i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 110; j++);
}

// ============ Timer0 初始化（1ms中断） ============
void timer0_init(void) {
    TMOD &= 0xF0;        // 定时器0模式0（16位自动重载）
    AUXR |= 0x80;        // T0x12=1，1T模式
    TH0 = (65536 - 11059) / 256;  // 11.0592MHz，1ms
    TL0 = (65536 - 11059) % 256;
    TR0 = 1;             // 启动定时器0
    ET0 = 1;             // 允许定时器0中断
    EA = 1;              // 开总中断
}

// ============ Timer0 中断服务函数 ============
void timer0_isr(void) __interrupt(1) {
    static unsigned char cnt = 0;
    if (++cnt >= 10) {   // 10ms
        cnt = 0;
        sys_10ms = 1;
    }
}

// ============ UART1 初始化（115200bps） ============
void uart1_init(void) {
    SCON = 0x50;         // 模式1，8位UART，允许接收
    AUXR |= 0x04;        // BRTx12=1，独立波特率1T模式
    BRT = 256 - 11059200 / 4 / 115200;  // 115200bps
    ES = 1;              // 允许串口中断
}

// ============ UART1 发送字符 ============
void uart1_send(unsigned char dat) {
    SBUF = dat;
    while (!TI);
    TI = 0;
}

// ============ UART1 发送字符串 ============
void uart1_send_str(unsigned char *str) {
    while (*str) {
        uart1_send(*str++);
    }
}

// ============ 按键初始化 ============
void key_init(void) {
    // K1=P3.2, K2=P3.3, K3=P1.7
    P3M0 &= ~0x0C;       // P3.2, P3.3 准双向口
    P3M1 |= 0x0C;
    P3 |= 0x0C;          // 内部上拉
    
    P1M0 &= ~0x80;       // P1.7 准双向口
    P1M1 |= 0x80;
    P1 |= 0x80;          // 内部上拉
}

// ============ 读取按键状态 ============
// 返回：0=无按键，1=K1，2=K2，3=K3
unsigned char key_read(void) {
    if (P32 == 0) return 1;  // K1
    if (P33 == 0) return 2;  // K2
    if (P17 == 0) return 3;  // K3
    return 0;
}

// ============ 数码管显示初始化 ============
void display_init(void) {
    P0M0 = 0xFF;         // P0口推挽输出（段码）
    P2M0 |= 0x07;        // P2.0-P2.2 推挽输出（位选）
    
    P0 = 0x00;           // 初始全灭
    P2 |= 0x07;          // 关闭所有位选（高电平关闭）
}

// ============ 数码管显示数字 ============
// pos: 0-7（位选），num: 0-10（10=灭）
void display_digit(unsigned char pos, unsigned char num) {
    if (pos > 7 || num > 10) return;
    
    P2 |= 0x07;          // 先关闭所有位选
    P0 = seg_table[num]; // 输出段码
    P2 &= ~(1 << pos);   // 打开对应位选（低电平有效）
}

// ============ 主函数 ============
void main(void) {
    unsigned char key_cur = 0, key_last = 0;
    unsigned char key_cnt = 0;
    
    // 初始化
    timer0_init();
    uart1_init();
    key_init();
    display_init();
    
    // 开机提示
    uart1_send_str("STC-B Test Start\r\n");
    uart1_send_str("Key: K1=P3.2 K2=P3.3 K3=P1.7\r\n");
    
    // 初始显示0
    display_digit(0, 0);
    
    while (1) {
        // 10ms定时标志
        if (sys_10ms) {
            sys_10ms = 0;
            
            // 读取按键
            key_cur = key_read();
            
            // 按键消抖和检测
            if (key_cur == key_last) {
                if (key_cur != 0) {
                    key_cnt++;
                    if (key_cnt == 5) {  // 50ms消抖后确认按下
                        // 按键按下事件
                        uart1_send_str("Key ");
                        uart1_send('0' + key_cur);
                        uart1_send_str(" pressed\r\n");
                        
                        // 数码管显示对应数字
                        display_num = key_cur;
                        display_digit(0, display_num);
                    }
                }
            } else {
                // 按键释放
                if (key_last != 0 && key_cnt >= 5) {
                    uart1_send_str("Key ");
                    uart1_send('0' + key_last);
                    uart1_send_str(" released\r\n");
                }
                key_cnt = 0;
                key_last = key_cur;
            }
        }
        
        // 数码管动态扫描（需要频繁调用）
        // 这里简化为只显示一位
        display_digit(0, display_num);
        delay_ms(5);
    }
}
