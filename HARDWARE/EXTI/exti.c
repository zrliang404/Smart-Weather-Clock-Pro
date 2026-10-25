#include "exti.h"
#include "key.h"
#include "led.h"
#include "main.h"
#include "stm32f10x.h"
u8 key_1 = 0,key_2 = 0;
void EXTIX_Init(void)
{
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	KEY_Init();
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource12); // PB12 -> EXTI12  //PB12 KEY0
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB,GPIO_PinSource14);  //PB14 KEY1
	
    EXTI_InitStructure.EXTI_Line = EXTI_Line12;      //PB12中断配置
    EXTI_InitStructure.EXTI_LineCmd =ENABLE;
    EXTI_InitStructure.EXTI_Mode =EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_Init(&EXTI_InitStructure);
    
    EXTI_InitStructure.EXTI_Line = EXTI_Line14;       //PB14中断配置
    EXTI_InitStructure.EXTI_LineCmd =ENABLE;
    EXTI_InitStructure.EXTI_Mode =EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_Init(&EXTI_InitStructure);
    
    NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;   //PB12 PB14中断配置
    NVIC_InitStructure.NVIC_IRQChannelCmd =ENABLE ;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =3;
    NVIC_Init(&NVIC_InitStructure);}



void EXTI15_10_IRQHandler (void)
{   
    // 处理KEY0中断
    if(EXTI_GetITStatus(EXTI_Line12) != RESET ){
        key_1 = 1;
        EXTI_ClearITPendingBit(EXTI_Line12);
    }

    // 处理KEY1中断
    if(EXTI_GetITStatus(EXTI_Line14) != RESET ){
        key_2 = 1;
        EXTI_ClearITPendingBit(EXTI_Line14);
    }
}




