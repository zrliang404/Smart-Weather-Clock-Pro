#include "key.h"

void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitTypeStructure;
    
    // 1. 使能 GPIOA, GPIOB 和 AFIO (复用功能) 的时钟
    // 注意：要释放 PB4，必须开启 AFIO 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    
    // 2. 释放 PB4 管脚
    // 默认情况下 PB4 是 JTAG 的 NJTRST 引脚。
    // 使用 GPIO_Remap_SWJ_JTAGDisable 可以关闭 JTAG 调试，但保留 SWD 下载/调试。
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    // 3. 配置 GPIOB 的引脚：PB12、PB14、PB4、PB6、PB8
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU;  // 上拉输入模式
    GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_14 | GPIO_Pin_4 | GPIO_Pin_6 | GPIO_Pin_8;
    GPIO_Init(GPIOB, &GPIO_InitTypeStructure);
    
    // 4. 配置 GPIOA 的引脚：PA9、PA11
    GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_11;
    GPIO_Init(GPIOA, &GPIO_InitTypeStructure);
}
