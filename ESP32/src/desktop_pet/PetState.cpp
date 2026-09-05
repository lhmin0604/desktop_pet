/************************************************************
 * 桌上宠物 - 宠物状态机实现 (PetState.cpp)
 * 宠物"大脑"：情绪系统 + 属性衰减 + 环境感知
 ************************************************************/

#include "PetState.h"

/* ==================== 常量定义 ==================== */

/* 属性衰减参数 */
#define HUNGER_DECAY_INTERVAL   60000   /* 每60秒饱食度-1 */
#define ENERGY_DECAY_INTERVAL   120000  /* 每120秒精力-1 */
#define HAPPINESS_DECAY_INTERVAL 90000  /* 每90秒快乐-1 */

/* 精力被动恢复 (+1/300秒 = +0.2/分,远慢于自然衰减 -0.5/分,实现"比消耗慢") */
#define ENERGY_PASSIVE_INTERVAL_SEC  300

/* 交互增益 */
#define FEED_AMOUNT     20      /* 喂食增加的饱食度 */
#define PLAY_AMOUNT     15      /* 玩耍增加的快乐值 */
#define PET_AMOUNT      10      /* 摸头增加的好感度 */
#define SLAP_AMOUNT     5       /* 拍桌子增加的快乐值 */

/* 心情阈值 */
#define HUNGRY_THRESHOLD    30  /* 饱食度低于30 → 饥饿 */
#define SLEEPY_THRESHOLD    25  /* 精力低于25 → 困倦 */
#define ANGRY_THRESHOLD     15  /* 快乐低于15 → 生气 */
#define HAPPY_THRESHOLD     70  /* 快乐高于70 → 开心 */
#define EXCITED_THRESHOLD   90  /* 快乐高于90 → 超开心 */

/* 光照阈值 (ADC 0-1023) */
#define LIGHT_DARK      200     /* 低于此值 = 天黑 */
#define LIGHT_BRIGHT    600     /* 高于此值 = 明亮 */

/* 温度阈值 (ADC 0-1023, NTC: ADC越高=温度越低) */
#define TEMP_COLD       700     /* 高于此值 = 冷 */
#define TEMP_HOT        300     /* 低于此值 = 热 */

/* ==================== 构造函数 ==================== */

PetState::PetState()
    : _proto(nullptr),
      _mood(MOOD_NORMAL),
      _prev_mood(MOOD_NORMAL),
      _action(ACT_NONE),
      _action_end(0),
      _temperature(512),
      _light(512),
      _last_decay(0),
      _last_expression(0),
      _mood_start(0) {

    _stats.hunger = 80;
    _stats.energy = 50;     /* 初始精力充足 → 默认心情 NORMAL(阈值 < 25 才进 SLEEPY) */
    _stats.happiness = 60;
    _stats.affection = 50;
    _stats.age_minutes = 0;
    _stats.level = 1;
    _stats.interact_count = 0;
}

void PetState::begin(PetProtocol* proto) {
    _proto = proto;
    _last_decay = millis();
    _last_expression = millis();
    _mood_start = millis();
}

/* ==================== 每秒更新 ==================== */

void PetState::update() {
    unsigned long now = millis();

    /* 0. 动作超时检测 (3s 后自动回心情) */
    if (_action != ACT_NONE && now >= _action_end) {
        _action = ACT_NONE;
        Serial.println("[动作] 结束,恢复显示心情");
    }

    /* 0.5 精力被动恢复 (+0.2/分,比自然衰减 -0.5/分 慢,空闲时仍会缓慢掉精力) */
    static uint16_t energy_recover_sec = 0;
    energy_recover_sec++;
    if (energy_recover_sec >= ENERGY_PASSIVE_INTERVAL_SEC) {
        energy_recover_sec = 0;
        if (_stats.energy < 100) _stats.energy++;
    }

    /* 1. 属性自然衰减 (每60秒衰减一次) */
    if (now - _last_decay > 60000) {
        _last_decay = now;
        decayStats();
    }

    /* 2. 评估心情是否该变化 */
    evaluateMood();

    /* 3. 每3秒刷新一次表情（防止闪烁） */
    if (now - _last_expression > 3000) {
        _last_expression = now;
        applyMood();
    }

    /* 4. 环境感知 */
    checkEnvironment();

    /* 5. 年龄增长（每分钟+1） */
    static uint8_t sec_counter = 0;
    sec_counter++;
    if (sec_counter >= 60) {
        sec_counter = 0;
        _stats.age_minutes++;

        /* 每10分钟升一级 (最高10级) */
        if (_stats.age_minutes % 10 == 0 && _stats.level < 10) {
            _stats.level++;
            Serial.printf("[成长] 🎉 宠物升级到 Lv.%d！\n", _stats.level);
            _proto->sendAll(EXPR_LAUGH, 0xFF, SOUND_HAPPY);
        }
    }
}

/* ==================== 属性衰减 ==================== */

void PetState::decayStats() {
    /* 饱食度缓慢下降 */
    if (_stats.hunger > 0) {
        _stats.hunger--;
    }

    /* 精力缓慢下降 */
    if (_stats.energy > 0) {
        _stats.energy--;
    }

    /* 快乐值缓慢下降 */
    if (_stats.happiness > 0) {
        _stats.happiness--;
    }

    /* 如果很饿，快乐下降更快 */
    if (_stats.hunger < HUNGRY_THRESHOLD && _stats.happiness > 0) {
        _stats.happiness--;
    }

    /* 如果很困，快乐也下降 */
    if (_stats.energy < SLEEPY_THRESHOLD && _stats.happiness > 0) {
        _stats.happiness--;
    }
}

/* ==================== 心情评估 ==================== */

void PetState::evaluateMood() {
    PetMood new_mood = _mood;

    /* 根据属性值决定心情 */
    if (_stats.hunger < ANGRY_THRESHOLD && _stats.happiness < ANGRY_THRESHOLD) {
        new_mood = MOOD_ANGRY;
    } else if (_stats.hunger < HUNGRY_THRESHOLD) {
        new_mood = MOOD_HUNGRY;
    } else if (_stats.energy < SLEEPY_THRESHOLD) {
        new_mood = MOOD_SLEEPY;
    } else if (_stats.happiness > EXCITED_THRESHOLD) {
        new_mood = MOOD_EXCITED;
    } else if (_stats.happiness > HAPPY_THRESHOLD) {
        new_mood = MOOD_HAPPY;
    } else {
        new_mood = MOOD_NORMAL;
    }

    /* 心情变化时更新显示 */
    if (new_mood != _mood) {
        _prev_mood = _mood;
        _mood = new_mood;
        _mood_start = millis();

        Serial.printf("[心情] %d → %d (饱食=%d 精力=%d 快乐=%d)\n",
                      _prev_mood, _mood,
                      _stats.hunger, _stats.energy, _stats.happiness);
    }
}

/* ==================== 应用心情到硬件 ==================== */

void PetState::applyMood() {
    if (!_proto) return;

    uint8_t expr = moodToExpression(_mood);
    uint8_t led = moodToLed(_mood);

    /* 只有在心情变化时才发送音效（避免重复播放） */
    if (_mood != _prev_mood) {
        uint8_t sound = moodToSound(_mood);
        _proto->sendAll(expr, led, sound);
        _prev_mood = _mood;
    } else {
        /* 只更新表情和LED */
        _proto->sendExpression(expr);
        _proto->sendLed(led);
    }
}

uint8_t PetState::moodToExpression(PetMood mood) {
    switch (mood) {
        case MOOD_HAPPY:    return EXPR_SMILE;
        case MOOD_NORMAL:   return EXPR_SMILE;
        case MOOD_HUNGRY:   return EXPR_SAD;
        case MOOD_SLEEPY:   return EXPR_SLEEPY;
        case MOOD_ANGRY:    return EXPR_ANGRY;
        case MOOD_SICK:     return EXPR_SICK;
        case MOOD_EXCITED:  return EXPR_LAUGH;
        case MOOD_LOVE:     return EXPR_LOVE;
        default:            return EXPR_SMILE;
    }
}

uint8_t PetState::moodToLed(PetMood mood) {
    switch (mood) {
        case MOOD_HAPPY:    return 0x18;    /* L3+L4 绿色区域 */
        case MOOD_NORMAL:   return 0x08;    /* L3 */
        case MOOD_HUNGRY:   return 0x04;    /* L2 黄色警告 */
        case MOOD_SLEEPY:   return 0x02;    /* L1 暗淡 */
        case MOOD_ANGRY:    return 0x01;    /* L0 红色警报 */
        case MOOD_SICK:     return 0x55;    /* 交替闪烁 */
        case MOOD_EXCITED:  return 0xFF;    /* 全亮！ */
        case MOOD_LOVE:     return 0x81;    /* L0+L7 心形 */
        default:            return 0x00;
    }
}

uint8_t PetState::moodToSound(PetMood mood) {
    switch (mood) {
        case MOOD_HAPPY:    return SOUND_HAPPY;
        case MOOD_HUNGRY:   return SOUND_SAD;
        case MOOD_SLEEPY:   return SOUND_SLEEP;
        case MOOD_ANGRY:    return SOUND_ALARM;
        case MOOD_EXCITED:  return SOUND_HAPPY;
        case MOOD_LOVE:     return SOUND_LOVE;
        default:            return 0;   /* 不发声音 */
    }
}

/* ==================== 传感器更新 ==================== */

void PetState::updateSensor(uint16_t temp, uint16_t light,
                              uint8_t buttons, uint8_t flags) {
    _temperature = temp;
    _light = light;
}

/* ==================== 环境感知 ==================== */

void PetState::checkEnvironment() {
    /* 天黑了 → 宠物想睡觉 */
    if (_light < LIGHT_DARK && _mood != MOOD_SLEEPY) {
        if (_stats.energy < 50) {
            /* 天黑 + 精力低 → 更困 */
            if (_stats.energy > 5) _stats.energy -= 2;
        }
    }

    /* 太冷了 → 宠物不高兴 */
    if (_temperature > TEMP_COLD) {
        if (_stats.happiness > 2) _stats.happiness -= 1;
    }

    /* 太热了 → 宠物不高兴 */
    if (_temperature < TEMP_HOT) {
        if (_stats.happiness > 2) _stats.happiness -= 1;
    }
}

/* ==================== 动作触发 ==================== */

/* 触发一个动作 (覆盖心情显示,持续 ACT_DURATION_MS 后自动回心情) */
void PetState::triggerAction(PetAction act) {
    if (act == ACT_NONE || (uint8_t)act >= ACT_COUNT) return;
    _action = act;
    _action_end = millis() + ACT_DURATION_MS;
    Serial.printf("[动作] → %d (持续 %d ms)\n", act, ACT_DURATION_MS);
}

/* ==================== 交互事件 ==================== */

void PetState::setMood(PetMood mood) {
    _prev_mood = _mood;
    _mood = mood;
    _mood_start = millis();
    applyMood();
}

/* 动作触发 (瞬时动画,持续 3s 自动回心情) */

/* K1 = 喂食 (ACT_EAT), K2 = 玩耍 (ACT_PLAY), K3 = 摸头 (ACT_STROKE) */
void PetState::onButtonPress(uint8_t key_id) {
    _stats.interact_count++;

    switch (key_id) {
        case 1: /* K1: 喂食 → ACT_EAT (+饱食 +快乐 +精力) */
            Serial.println("[互动] 🍖 喂食！");
            _stats.hunger = (_stats.hunger + FEED_AMOUNT > 100) ? 100 : (_stats.hunger + FEED_AMOUNT);
            _stats.happiness = (_stats.happiness + 5 > 100) ? 100 : (_stats.happiness + 5);
            _stats.energy    = (_stats.energy    + 10 > 100) ? 100 : (_stats.energy    + 10);   /* 食物=能量 */
            _proto->sendAll(EXPR_EAT, 0x0C, SOUND_EAT);
            triggerAction(ACT_EAT);
            break;

        case 2: /* K2: 玩耍 → ACT_PLAY (+快乐 -精力) */
            Serial.println("[互动] 🎮 玩耍！");
            _stats.happiness = (_stats.happiness + PLAY_AMOUNT > 100) ? 100 : (_stats.happiness + PLAY_AMOUNT);
            _stats.energy    = (_stats.energy > 5) ? (_stats.energy - 5) : 0;   /* 玩耍消耗精力,有下限保护 */
            _proto->sendAll(EXPR_PLAY, 0x3C, SOUND_HAPPY);
            triggerAction(ACT_PLAY);
            break;

        case 3: /* K3: 摸头 → ACT_STROKE (+好感 +快乐 +精力) */
            Serial.println("[互动] 🤚 摸头！");
            _stats.affection = (_stats.affection + PET_AMOUNT > 100) ? 100 : (_stats.affection + PET_AMOUNT);
            _stats.happiness = (_stats.happiness + 8 > 100) ? 100 : (_stats.happiness + 8);
            _stats.energy    = (_stats.energy + 5 > 100) ? 100 : (_stats.energy + 5);   /* 抚摸放松 */
            _proto->sendAll(EXPR_LOVE, 0x81, SOUND_LOVE);
            triggerAction(ACT_STROKE);
            break;
    }

    /* 互动后重新评估心情 */
    evaluateMood();
}

void PetState::onNavPress(uint8_t direction) {
    _stats.interact_count++;

    switch (direction) {
        case NAV_UP:
            Serial.println("[导航] ⬆ 上");
            _stats.happiness += 3;
            _proto->sendExpression(EXPR_SURPRISE);
            _proto->sendSound(SOUND_SHORT);
            break;
        case NAV_DOWN:
            Serial.println("[导航] ⬇ 下");
            _proto->sendExpression(EXPR_SLEEPY);
            break;
        case NAV_LEFT:
            Serial.println("[导航] ⬅ 左");
            _proto->sendLed(0xF0);   /* 左半亮 */
            break;
        case NAV_RIGHT:
            Serial.println("[导航] ➡ 右");
            _proto->sendLed(0x0F);   /* 右半亮 */
            break;
        case NAV_CENTER:
            Serial.println("[导航] ⏺ 中");
            _stats.happiness += 5;
            _proto->sendAll(EXPR_SMILE, 0xFF, SOUND_HAPPY);
            break;
    }

    if (_stats.happiness > 100) _stats.happiness = 100;
    evaluateMood();
}

void PetState::onVibration() {
    _stats.interact_count++;
    Serial.println("[互动] 📳 拍桌子！");

    /* 拍桌子 = 引起注意,宠物被吓到或开心 */
    if (_stats.happiness > 50) {
        /* 心情好 → 开心回应 */
        _proto->sendAll(EXPR_LAUGH, 0xFF, SOUND_HAPPY);
        _stats.happiness += SLAP_AMOUNT;
    } else {
        /* 心情差 → 被吓到 */
        _proto->sendAll(EXPR_SURPRISE, 0x55, SOUND_ALARM);
    }

    if (_stats.happiness > 100) _stats.happiness = 100;
    evaluateMood();

    /* 屏幕上显示拍桌反应 (复用 stand_up.svg,3 秒) */
    triggerAction(ACT_VIBRATION);
}

void PetState::onHall(bool close) {
    _stats.interact_count++;

    if (close) {
        Serial.println("[互动] 🧲 磁铁靠近 = 摸摸！");
        _stats.affection += PET_AMOUNT;
        _stats.happiness += 8;
        if (_stats.affection > 100) _stats.affection = 100;
        if (_stats.happiness > 100) _stats.happiness = 100;
        _proto->sendAll(EXPR_LOVE, 0x81, SOUND_LOVE);
        triggerAction(ACT_STROKE);
    } else {
        Serial.println("[互动] 🧲 磁铁离开");
        evaluateMood();
    }
}
