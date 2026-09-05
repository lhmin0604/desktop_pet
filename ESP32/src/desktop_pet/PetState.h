/************************************************************
 * 桌上宠物 - 宠物状态机 (PetState.h)
 * 管理宠物的情绪、行为、成长系统
 ************************************************************/

#ifndef PET_STATE_H
#define PET_STATE_H

#include <Arduino.h>
#include "PetProtocol.h"

/* 宠物心情 */
enum PetMood {
    MOOD_HAPPY = 0,     /* 开心 */
    MOOD_NORMAL,        /* 普通 */
    MOOD_HUNGRY,        /* 饥饿 */
    MOOD_SLEEPY,        /* 困倦 */
    MOOD_ANGRY,         /* 生气 */
    MOOD_SICK,          /* 生病 */
    MOOD_EXCITED,       /* 超开心 */
    MOOD_LOVE,          /* 恋爱/被摸 */
    MOOD_COUNT
};

/* 宠物动作 (瞬时动画,覆盖心情显示,超时后回到心情)
 * ACT_NONE=0 不会画动作图(查表用 action-1 当索引,所以索引 0..3 对应 ACT_EAT..ACT_STROKE)
 * 与 cat_bitmaps.h 的 CAT_BMP_ACT_* 一一对应
 * ACT_VIBRATION 共用 stand_up.svg 文件 (语义改成"拍桌反应") */
enum PetAction {
    ACT_NONE      = 0,
    ACT_EAT       = 1,    /* 吃饭 (K1) */
    ACT_PLAY      = 2,    /* 玩耍 (K2) */
    ACT_VIBRATION = 3,    /* 拍桌反应 (v 键) - 用 stand_up.svg 做图 */
    ACT_STROKE    = 4,    /* 抚摸 (K3 / 磁铁) */
    ACT_COUNT
};
#define ACT_DURATION_MS  3000   /* 动作图持续显示 3 秒后回到心情图 */

/* 导航方向 */
enum NavDir {
    NAV_RIGHT = 1,
    NAV_DOWN  = 2,
    NAV_CENTER = 3,
    NAV_LEFT  = 4,
    NAV_UP    = 5
};

/* 宠物属性 */
struct PetStats {
    uint8_t  hunger;        /* 饱食度 0-100 (0=很饿) */
    uint8_t  energy;        /* 精力值 0-100 (0=很困) */
    uint8_t  happiness;     /* 快乐值 0-100 */
    uint8_t  affection;     /* 好感度 0-100 (对你的亲近度) */
    uint16_t age_minutes;   /* 年龄（分钟） */
    uint8_t  level;         /* 等级 1-10 */
    uint32_t interact_count;/* 互动总次数 */
};

class PetState {
public:
    PetState();

    void begin(PetProtocol* proto);
    void update();  /* 每秒调用一次 */

    /* 设置心情 */
    void setMood(PetMood mood);

    /* 传感器更新 */
    void updateSensor(uint16_t temp, uint16_t light,
                      uint8_t buttons, uint8_t flags);

    /* 交互事件 */
    void onButtonPress(uint8_t key_id);     /* K1=喂食 K2=玩耍 K3=摸头 */
    void onNavPress(uint8_t direction);     /* 导航按键 */
    void onVibration();                     /* 拍桌子 */
    void onHall(bool close);                /* 磁铁靠近/离开 */

    /* 动作触发 (覆盖心情显示,ACT_DURATION_MS 后自动回心情) */
    void triggerAction(PetAction act);

    /* 获取状态 */
    PetMood getMood() const { return _mood; }
    PetAction getAction() const { return _action; }   /* ACT_NONE 表示无动作,显示心情 */
    PetStats getStats() const { return _stats; }

private:
    PetProtocol* _proto;
    PetMood _mood;
    PetMood _prev_mood;
    PetAction _action;                  /* 当前正在执行的动作 (ACT_NONE=无) */
    unsigned long _action_end;          /* 动作结束时间 (millis) */
    PetStats _stats;

    /* 传感器缓存 */
    uint16_t _temperature;
    uint16_t _light;

    /* 计时器 */
    unsigned long _last_decay;      /* 属性衰减计时 */
    unsigned long _last_expression; /* 表情刷新计时 */
    unsigned long _mood_start;      /* 当前心情开始时间 */

    /* 内部方法 */
    void decayStats();              /* 自然衰减 */
    void evaluateMood();            /* 重新评估心情 */
    void applyMood();               /* 根据心情更新显示 */
    uint8_t moodToExpression(PetMood mood);
    uint8_t moodToLed(PetMood mood);
    uint8_t moodToSound(PetMood mood);
    void checkEnvironment();        /* 环境感知 */
};

#endif
