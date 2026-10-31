/**
  ******************************************************************************
  * @file    app_timer.c
  * @brief   三按键控制 + 60 秒倒计时，显示在 PA5/PA6 那块四位数码管
  * @note    显示格式：前两位 = 分钟，后两位 = 秒，中间冒号常亮
  *                  01:00 -> 00:59 -> ... -> 00:00
  *
  *          按键行为：
  *            PB11【长按 1 秒】电源开关：灭 -> 亮 01:00；亮 -> 灭（并复位到 60 秒）
  *            PB11【短按】    亮着时有效：
  *                              待机 -> 开始倒计时
  *                              计时中 -> 暂停（保留剩余时间）
  *                              暂停中 -> 继续倒计时
  *                              归零后 -> 重新从 01:00 开始
  *            PB13【短按】    时间 +30 秒   ┐ 只在数码管点亮、且倒计时没在跑的时候
  *            PA9 【短按】    时间 -30 秒   ┘ 有效（灭屏时按、跑动中按都无效）
  *
  *          全程非阻塞，靠 HAL_GetTick() 计时，不影响其它任务
  ******************************************************************************
  */

#include "app_timer.h"
#include "tm1637.h"

/* 借用 tm1637.c 里的段码表（0~9, A~F 共 16 项） */
extern const unsigned char SEGMENT_CODE[];

/* ============================ TM1637 显示控制指令 ============================ */
#define TM_CMD_DISP_OFF     0x80U       /* 关显示（数码管全灭，显存保留） */
#define TM_CMD_DISP_ON      0x8CU       /* 开显示 + 亮度 4（0x88 | 0~7） */

/* ============================ 状态机 ============================ */

typedef enum
{
    ST_IDLE = 0,            /* 已开机但还没开始计数，显示 01:00 */
    ST_RUNNING,             /* 倒计时中 */
    ST_PAUSED,              /* 已暂停，保留剩余时间 */
    ST_DONE                 /* 已归零 */
} TimerState_t;

typedef enum
{
    KEY_NONE = 0,
    KEY_SHORT,              /* 按下后 1 秒内松手 */
    KEY_LONG                /* 按住满 1 秒（不等松手，立即触发） */
} KeyEvent_t;

typedef enum
{
    KR_NONE = 0,
    KR_DOWN,                /* 消抖确认后的按下沿 */
    KR_UP                   /* 消抖确认后的松开沿 */
} KeyRaw_t;

/* 一个按键的全部状态 */
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       raw;          /* 上次读到的原始电平：1 = 松开 */
    uint8_t       stable;       /* 消抖后电平 */
    uint32_t      change_tick;  /* 电平跳变时刻 */
    uint32_t      press_tick;   /* 确认按下的时刻 */
    uint8_t       long_fired;   /* 本次按下是否已经触发过长按 */
} Key_t;

static uint8_t      s_disp_on    = 0U;               /* 0 = 数码管灭 */
static TimerState_t s_state      = ST_IDLE;
static uint32_t     s_remain_ms  = TIMER_TOTAL_MS;
static uint32_t     s_last_tick  = 0U;
static uint32_t     s_last_sec   = 0xFFFFFFFFU;      /* 上次显示的秒数，用于判断是否需重刷 */

/* 显存缓存：内容没变就不碰总线 */
static uint8_t      s_cache[4]   = {0xFFU, 0xFFU, 0xFFU, 0xFFU};

/* 三个按键 */
static Key_t s_key_main;        /* PB11：长按开关屏，短按 开始/暂停/继续 */
static Key_t s_key_up;          /* PB13：+30 秒 */
static Key_t s_key_dn;          /* PA9 ：-30 秒 */

/* ============================ 底层显示 ============================ */

/** @brief  发一条单字节命令（START -> 数据 -> STOP） */
static void Tm_SendCmd(uint8_t cmd)
{
    TM1637_START();
    TM1637_WRITE_BYTE_DATA(cmd);
    TM1637_CHECK_ack();
    TM1637_STOP();
}

/**
  * @brief  向指定位置写一位数字，并单独控制该位上的 0x80 位
  * @param  addr  显存地址 0~3（0 是最左位）
  * @param  digit 0~9 显示数字；>=10 表示整位熄灭（空白）
  * @param  colon 1 = 点亮中间冒号（该模块的 0x80 位接的是冒号，不是小数点）
  * @note   这里不再顺手发"开显示"命令，开关屏统一由 Tm_SendCmd 控制，
  *         否则灭屏之后一刷新就又亮起来了。
  */
static void Tm_WriteDigit(uint8_t addr, uint8_t digit, uint8_t colon)
{
    uint8_t seg;

    seg = (digit <= 9U) ? SEGMENT_CODE[digit] : 0x00U;
    if (colon != 0U)
    {
        seg |= 0x80U;
    }

    /* 第一段：设置为"固定地址"写模式 */
    Tm_SendCmd((uint8_t)(0x40U + addr));

    /* 第二段：指定地址并写入段码 */
    TM1637_START();
    TM1637_WRITE_BYTE_DATA((unsigned char)(0xC0U + addr));
    TM1637_CHECK_ack();
    TM1637_WRITE_BYTE_DATA(seg);
    TM1637_CHECK_ack();
    TM1637_STOP();
}

/**
  * @brief  带缓存的写位：灭屏或内容没变都不碰总线
  * @note   倒计时每秒只有"秒个位"在变，其余三位跳过，
  *         单次刷新耗时从 ~1.2ms 降到 ~0.3ms，主循环更不容易被拖住
  */
static void Tm_WriteDigitCached(uint8_t addr, uint8_t digit, uint8_t colon)
{
    uint8_t key;

    if ((s_disp_on == 0U) || (addr > 3U))
    {
        return;
    }

    key = (uint8_t)(((digit > 9U) ? 0x0FU : digit) | ((colon != 0U) ? 0x80U : 0x00U));

    if (s_cache[addr] == key)
    {
        return;                     /* 这一位没变，跳过 */
    }

    s_cache[addr] = key;
    Tm_WriteDigit(addr, digit, colon);
}

/**
  * @brief  把剩余毫秒数显示成 “mm:ss”
  * @note   冒号固定挂在第 2 位（最左位记为第 1 位）上。
  *         如果你的模块实际是第 3 位控制冒号，把下面 colon 参数从第 2 位
  *         挪到第 3 位即可。
  */
static void Display_Time(uint32_t remain_ms)
{
    /* 向上取整：保证每个数字真实显示满 1 秒
       （60.0s 显示 01:00，59.9s 仍显示 01:00，59.0s 才跳到 00:59） */
    uint32_t sec = (remain_ms + 999U) / 1000U;
    uint8_t  mm  = (uint8_t)(sec / 60U);             /* 分钟 0~99 */
    uint8_t  ss  = (uint8_t)(sec % 60U);             /* 秒   0~59 */

    Tm_WriteDigitCached(0U, (uint8_t)(mm / 10U), 0U);      /* 分钟十位 */
    Tm_WriteDigitCached(1U, (uint8_t)(mm % 10U), 1U);      /* 分钟个位 + 冒号 */
    Tm_WriteDigitCached(2U, (uint8_t)(ss / 10U), 0U);      /* 秒十位 */
    Tm_WriteDigitCached(3U, (uint8_t)(ss % 10U), 0U);      /* 秒个位 */

    s_last_sec = sec;
}

/* ============================ 开关屏 ============================ */

/** @brief  开机：复位到 60 秒并点亮显示（先写显存再开屏，避免闪出旧数字） */
static void Power_On(void)
{
    s_state     = ST_IDLE;
    s_remain_ms = TIMER_TOTAL_MS;
    s_last_tick = HAL_GetTick();

    s_cache[0]  = 0xFFU;                /* 全部作废，强制重写四位 */
    s_cache[1]  = 0xFFU;
    s_cache[2]  = 0xFFU;
    s_cache[3]  = 0xFFU;

    s_disp_on   = 1U;
    Display_Time(s_remain_ms);          /* 此时屏还是关的，写的内容看不见 */
    Tm_SendCmd(TM_CMD_DISP_ON);         /* 再开屏，一次到位 */
}

/** @brief  关机：灭屏，并把倒计时复位回 60 秒（下次开机重新从 01:00 起） */
static void Power_Off(void)
{
    s_disp_on   = 0U;
    s_state     = ST_IDLE;
    s_remain_ms = TIMER_TOTAL_MS;
    s_last_sec  = 0xFFFFFFFFU;

    Tm_SendCmd(TM_CMD_DISP_OFF);
}

/* ============================ 时间加减 ============================ */

/**
  * @brief  在当前剩余时间上加/减 ADJ_STEP_MS
  * @param  add 1 = 加，0 = 减
  * @note   上下限：最大 99:59，最小 00:00。减到 0 就进入"已归零"状态；
  *         从 0 加回去则回到"暂停"状态，短按即可从新数值继续。
  */
static void Adjust_Time(uint8_t add)
{
    if (add != 0U)
    {
        if ((TIMER_MAX_MS - s_remain_ms) <= ADJ_STEP_MS)
        {
            s_remain_ms = TIMER_MAX_MS;
        }
        else
        {
            s_remain_ms += ADJ_STEP_MS;
        }
    }
    else
    {
        if (s_remain_ms <= ADJ_STEP_MS)
        {
            s_remain_ms = 0U;
        }
        else
        {
            s_remain_ms -= ADJ_STEP_MS;
        }
    }

    /* 状态跟着数值同步 */
    if (s_remain_ms == 0U)
    {
        s_state = ST_DONE;
    }
    else if (s_state == ST_DONE)
    {
        s_state = ST_PAUSED;            /* 从 0 加回来，等短按继续 */
    }
    else
    {
        /* ST_IDLE / ST_PAUSED 保持不变 */
    }

    Display_Time(s_remain_ms);
}

/* ============================ 按键扫描 ============================ */

static void Key_InitOne(Key_t *k, GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};

    if (port == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    else
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }

    gpio.Pin   = pin;
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_PULLUP;           /* 内部上拉，按键另一端接 GND */
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &gpio);

    k->port        = port;
    k->pin         = pin;
    k->raw         = 1U;
    k->stable      = 1U;
    k->change_tick = HAL_GetTick();
    k->press_tick  = HAL_GetTick();
    k->long_fired  = 0U;
}

/**
  * @brief  通用消抖：只在电平稳定满 KEY_DEBOUNCE_MS 后才认账
  * @return KR_DOWN = 确认按下那一瞬；KR_UP = 确认松开那一瞬；KR_NONE = 无事件
  */
static KeyRaw_t Key_Update(Key_t *k, uint32_t now)
{
    uint8_t level = (HAL_GPIO_ReadPin(k->port, k->pin) == GPIO_PIN_SET) ? 1U : 0U;

    /* 1. 原始电平变化，只记录时刻，先不下结论 */
    if (level != k->raw)
    {
        k->raw         = level;
        k->change_tick = now;
        return KR_NONE;
    }

    /* 2. 稳定不足消抖时间，继续等 */
    if ((now - k->change_tick) < KEY_DEBOUNCE_MS)
    {
        return KR_NONE;
    }

    /* 3. 稳定够了才切换状态 */
    if (level != k->stable)
    {
        k->stable = level;

        if (level == 0U)                /* 确认按下 */
        {
            k->press_tick = k->change_tick;
            k->long_fired = 0U;
            return KR_DOWN;
        }

        return KR_UP;                   /* 确认松开 */
    }

    return KR_NONE;
}

/**
  * @brief  主键（PB11）扫描：区分短按 / 长按
  * @note   长按在按住满 KEY_LONG_MS 的瞬间就触发（不用等松手），
  *         触发后本次按下期间不会再产生任何事件，松手也不会补一个短按。
  */
static KeyEvent_t Key_Main_Scan(void)
{
    uint32_t now = HAL_GetTick();
    KeyRaw_t raw = Key_Update(&s_key_main, now);

    if (raw == KR_UP)
    {
        if ((s_key_main.long_fired == 0U) &&
            ((now - s_key_main.press_tick) < KEY_LONG_MS))
        {
            return KEY_SHORT;
        }
        return KEY_NONE;
    }

    if ((s_key_main.stable == 0U) && (s_key_main.long_fired == 0U) &&
        ((now - s_key_main.press_tick) >= KEY_LONG_MS))
    {
        s_key_main.long_fired = 1U;
        return KEY_LONG;
    }

    return KEY_NONE;
}

/** @brief  加减键扫描：按下沿就触发一次，按住不放不会连发 */
static uint8_t Key_Adj_Scan(Key_t *k)
{
    return (Key_Update(k, HAL_GetTick()) == KR_DOWN) ? 1U : 0U;
}

/* ============================ 对外接口 ============================ */

void AppTimer_Init(void)
{
    Key_InitOne(&s_key_main, KEY_PORT,       KEY_PIN);
    Key_InitOne(&s_key_up,   KEY_UP_PORT,    KEY_UP_PIN);
    Key_InitOne(&s_key_dn,   KEY_DOWN_PORT,  KEY_DOWN_PIN);

    s_state     = ST_IDLE;
    s_remain_ms = TIMER_TOTAL_MS;
    s_last_tick = HAL_GetTick();
    s_last_sec  = 0xFFFFFFFFU;
    s_disp_on   = 0U;

    Tm_SendCmd(TM_CMD_DISP_OFF);        /* 上电就是灭的，等长按唤醒 */
}

void AppTimer_Task(void)
{
    KeyEvent_t evt = Key_Main_Scan();
    uint8_t    up  = Key_Adj_Scan(&s_key_up);
    uint8_t    dn  = Key_Adj_Scan(&s_key_dn);

    /* ---- 1. 长按：切换开关屏（灭屏时也能长按开机） ---- */
    if (evt == KEY_LONG)
    {
        if (s_disp_on == 0U)
        {
            Power_On();                 /* 灭 -> 亮 01:00 */
        }
        else
        {
            Power_Off();                /* 亮 -> 灭，倒计时复位 */
        }
        return;
    }

    /* ---- 2. 灭屏时其余按键一律无效 ---- */
    if (s_disp_on == 0U)
    {
        return;
    }

    /* ---- 3. 短按主键：开始 / 暂停 / 继续 / 重来 ---- */
    if (evt == KEY_SHORT)
    {
        switch (s_state)
        {
            case ST_IDLE:                       /* 待机 -> 开始（用当前设定的时长） */
                s_last_tick = HAL_GetTick();
                s_state     = ST_RUNNING;
                Display_Time(s_remain_ms);
                break;

            case ST_DONE:                       /* 归零 -> 重新从 60 秒开始 */
                s_remain_ms = TIMER_TOTAL_MS;
                s_last_tick = HAL_GetTick();
                s_state     = ST_RUNNING;
                Display_Time(s_remain_ms);
                break;

            case ST_RUNNING:                    /* 倒计时中 -> 暂停 */
                s_state = ST_PAUSED;
                break;

            case ST_PAUSED:                     /* 暂停 -> 继续 */
                s_last_tick = HAL_GetTick();
                s_state     = ST_RUNNING;
                break;

            default:
                break;
        }
        return;
    }

    /* ---- 4. 加减 30 秒 ---- */
    if (up != dn)                           /* 两个键同时按就不理，避免边加边减 */
    {
        if ((ADJ_ENABLE_WHILE_RUNNING != 0U) || (s_state != ST_RUNNING))
        {
            Adjust_Time(up);
        }
    }

    /* ---- 5. 倒计时推进 ---- */
    if (s_state == ST_RUNNING)
    {
        uint32_t now     = HAL_GetTick();
        uint32_t elapsed = now - s_last_tick;

        if (elapsed >= TIMER_STEP_MS)
        {
            uint32_t steps = elapsed / TIMER_STEP_MS;
            s_last_tick += steps * TIMER_STEP_MS;       /* 累加式推进，不会越走越慢 */

            if (s_remain_ms > (steps * TIMER_STEP_MS))
            {
                s_remain_ms -= steps * TIMER_STEP_MS;
            }
            else
            {
                s_remain_ms = 0U;
                s_state     = ST_DONE;
            }

            /* 秒数变化才刷新，避免 100ms 就刷一次总线 */
            if (((s_remain_ms + 999U) / 1000U) != s_last_sec)
            {
                Display_Time(s_remain_ms);
            }
        }
    }
}
