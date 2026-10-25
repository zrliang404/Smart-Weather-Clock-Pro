/*
 * MIT License
 *
 * Copyright (c) 2023 梅花嵌入式
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>
#include "stm32f10x.h"

/**
 * @brief 微妙级延时
 *
 * @param us 延时时间，单位微秒
 */
void delay_us(uint32_t us)
{
    /* 使能SysTick，时钟源为AHB时钟，即CPU时钟，计数值为us * (CPU频率 / 1000000) - 1 */
    SysTick->LOAD = us * (SystemCoreClock / 1000000) - 1;
    SysTick->VAL = 0;   /* 清空计数值 */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; /* 使能SysTick，启动计数 */
    while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0); /* 等待计数完成 */
    SysTick->CTRL = 0;  /* 关闭SysTick */
}

/**
 * @brief 毫秒级延时
 *
 * @param ms 延时时间，单位毫秒
 */
void delay_ms(uint32_t ms)
{
    /* 循环调用微妙级延时函数，延时ms毫秒 */
    while (ms--)
    {
        delay_us(1000);
    }
}
