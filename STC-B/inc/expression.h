/************************************************************
 * 桌上宠物 - 表情引擎模块 (expression.h)
 * 控制8位数码管显示宠物表情
 ************************************************************/

#ifndef _expression_H_
#define _expression_H_

#include "comm.h"     /* 引入表情ID定义 */

/* ==================== 公共函数 ==================== */

/* 初始化表情引擎（调用 DisplayerInit） */
extern void ExprInit(void);

/* 设置预设表情（使用表情ID） */
extern void ExprSetFace(unsigned char expr_id);

/* 设置自定义段码表情（8字节原始段码） */
extern void ExprSetCustom(unsigned char *seg_data);

/* 设置LED状态 */
extern void ExprSetLed(unsigned char led_val);

/* 播放预设音效 */
extern void ExprPlaySound(unsigned char sound_id);

/* 一次性设置表情+LED+音效 */
extern void ExprSetAll(unsigned char expr_id, unsigned char led, unsigned char sound_id);

/* 表情动画帧更新（在定时器回调中调用） */
extern void ExprAnimate(void);

#endif
