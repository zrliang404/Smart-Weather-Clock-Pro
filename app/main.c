#include <stdbool.h>    /* 用尖括号<>引用标准库用 */
#include <stdint.h>
#include <stdlib.h> 
#include <stdio.h>
#include <string.h>
#include "stm32f10x.h"  /* 用双引号""引用自定义库 */
#include "main.h"
#include "led.h"
#include "rtc.h"
#include "timer.h"
#include "esp_at.h"
#include "mpu6050.h"
#include "st7735.h"
#include "stfonts.h"
#include "stimage.h"
#include "weather.h"
#include "exti.h"
#include "key.h"
#include "ntc.h"
#include "math.h"

#define WEATHER_URL(city) "https://api.seniverse.com/v3/weather/now.json?key=SAsBo2lIdSq9kVPqt&location=" #city "&language=en&unit=c"

static const char *wifi_ssid = "USER_162401";   /*修改成你的WIFI名字*/
static const char *wifi_password = "28835771";  /*修改成你的WIFI密码*/
/* ---------------------------------------------------------
 * 数据结构抽象
 * --------------------------------------------------------- */
typedef struct {
    const char *uri;
    const char *city_name;
    const st_fonts_t *font;
} City_Config_t;

static const City_Config_t cities[] = {
    {WEATHER_URL(zhaoqing),  "Zhaoqing",  &font_zhaoqing_24x24},
    {WEATHER_URL(shanghai),  "Shanghai",  &font_shanghai_24x24},
    {WEATHER_URL(guangzhou), "Guangzhou", &font_guangzhou_24x24},
    {WEATHER_URL(beijing),   "Beijing",   &font_beijing_24x24},
    {WEATHER_URL(heihe),     "Heihe",     &font_heihe_24x24},
};
#define CITY_COUNT (sizeof(cities) / sizeof(cities[0]))

typedef struct {
    const char *weather_str;
    const st_image_t *img;
} Weather_Icon_Map_t;

static const Weather_Icon_Map_t weather_icons[] = {
    {"Cloudy",     &icon_weather_duoyun},
    {"Wind",       &icon_weather_feng},
    {"Clear",      &icon_weather_qing},
    {"Sunny",      &icon_weather_qing},
    {"Snow",       &icon_weather_xue},
    {"Overcast",   &icon_weather_yin},
    {"Light rain", &icon_weather_xiaoyu},
    {"Foggy",      &icon_weather_wu},
    {"Shower",     &icon_weather_zhenyu},
};
#define WEATHER_ICON_COUNT (sizeof(weather_icons) / sizeof(weather_icons[0]))

/* 全局系统状态变量 */
static uint32_t runms = 0;
static uint32_t disp_height = 0;
extern u8 key_1, key_2; 
static s8 current_city_index = 0; 
static bool sntp_ok = false;

/* 二级缓存池系统 */
static weather_t weather_cache[CITY_COUNT];            /* 在内存里开辟 5 个城市的缓存槽（第一级：数据缓存） */
static bool weather_valid[CITY_COUNT] = {false};       /* 标志位数组：记录哪些城市的缓存是有效的（未失效） */
static uint32_t weather_update_time[CITY_COUNT] = {0}; /* 时间戳数组：记录每个城市上一次更新的时间（第二级：时效缓存） */

/* =========================================================
 * UI 状态机与闹钟全局变量
 * ========================================================= */
typedef enum {
    PAGE_MAIN,      // 主界面
    PAGE_ALARM      // 闹钟设置界面
} UI_Page_t;

static UI_Page_t current_page = PAGE_MAIN; 

static uint8_t alarm_hour = 7;       // 默认闹钟时间 07:30
static uint8_t alarm_min = 30;
static bool alarm_on = false;        // 闹钟开关状态
static bool is_alarming = false;     // 当前是否正在响铃
static uint8_t alarm_cursor = 0;     // 闹钟设置光标 (0:时, 1:分, 2:开关)
static uint32_t alarm_start_time = 0;// 记录闹钟开始响铃的时间戳

/* ---------------------------------------------------------
 * 基础硬件初始化函数
 * --------------------------------------------------------- */

/* ?? 抢占 PB8 引脚配置，用于驱动蜂鸣器 */
static void BEEP_Init(void) {
    GPIO_InitTypeDef GPIO_InitTypeStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出驱动
    GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitTypeStructure);
    
    // ?? 修改：上电默认置为高电平，关闭低电平触发的蜂鸣器
    GPIO_SetBits(GPIOB, GPIO_Pin_8); 
}

static void timer_elapsed_callback(void) {
    runms++;
    if (runms > 24 * 60 * 60 * 1000) runms = 0; 
}

static void print_init_log(const char *msg, uint16_t color) {
    st7735_write_string(0, disp_height, msg, &font_ascii_8x16, color, ST7735_BLACK);
    disp_height += font_ascii_8x16.height;
}

static void wifi_init(void) {
    print_init_log("Init ESP32...", ST7735_WHITE);
    if (!esp_at_init()) { print_init_log("Failed!!!", ST7735_RED); while (1); }

    print_init_log("Init WIFI...", ST7735_WHITE);
    if (!esp_at_wifi_init()) { print_init_log("Failed!!!", ST7735_RED); while (1); }

    print_init_log("Connect WIFI...", ST7735_WHITE);
    if (!esp_at_wifi_connect(wifi_ssid, wifi_password)) { print_init_log("Failed!!!", ST7735_RED); while (1); }

    print_init_log("Sync Time...", ST7735_WHITE);
    if (!esp_at_sntp_init()) { print_init_log("Failed!!!", ST7735_RED); while (1); }
}

/* =========================================================
 * 渲染函数组 
 * ========================================================= */

static void task_update_time_display(void) {
    char str[32];
    rtc_date_t date;
    rtc_get_date(&date);
    
    snprintf(str, sizeof(str), "%02d-%02d-%02d", date.year % 100, date.month, date.day);
    st7735_write_string(59, 49, str, &font_ascii_8x16, ST7735_WHITE, ST7735_BLACK);
    
    snprintf(str, sizeof(str), "%02d%s%02d", date.hour, date.second % 2 ? " " : ":", date.minute);
    st7735_write_string(5, 70, str, &font_time_24x48, ST7735_YELLOW, ST7735_BLACK);
    
    if (alarm_on) {
        st7735_write_string(0, 112, "ALM", &font_ascii_8x16, ST7735_RED, ST7735_BLACK);
    } else {
        st7735_write_string(0, 112, "   ", &font_ascii_8x16, ST7735_BLACK, ST7735_BLACK);
    }
}

static void task_render_weather_ui(void) {
    char str[32];
    const City_Config_t *city = &cities[current_city_index];

    st7735_write_fonts(0, 49, (st_fonts_t*)city->font, 0, 2, ST7735_RED, ST7735_BLACK);

    if (!weather_valid[current_city_index]) {
        st7735_write_string(0, 16, "Updating...", &font_ascii_8x16, ST7735_YELLOW, ST7735_BLACK);
        st7735_write_string(43, 0, "--C", &font_temper_16x32, ST7735_WHITE, ST7735_BLACK);
        return; 
    }
    
    weather_t *weather = &weather_cache[current_city_index];
    const st_image_t *img = NULL;
    for (int i = 0; i < WEATHER_ICON_COUNT; i++) {
        if (strcmp(weather->weather, weather_icons[i].weather_str) == 0) {
            img = weather_icons[i].img; break;
        }
    }

    if (img != NULL) st7735_draw_image(0, 16, img->width, img->height, img->data);
    else {
        snprintf(str, sizeof(str), "%s", weather->weather); 
        st7735_write_string(0, 16, str, &font_ascii_8x16, ST7735_YELLOW, ST7735_BLACK);
    }

    float weather_temp_float = (float)atoi(weather->temperature); 
    uint16_t weather_color = temperature_to_color_gradient(weather_temp_float);
    snprintf(str, sizeof(str), "%sC", weather->temperature); 
    st7735_write_string(43 + (64 - strlen(weather->temperature)*16), 0, str, &font_temper_16x32, weather_color, ST7735_BLACK);
    
    snprintf(str, sizeof(str), "%s", weather->weekday);
    st7735_write_string(0, 0, str, &font_ascii_8x16, ST7735_WHITE, ST7735_BLACK);
}

static void task_update_ntc_temperature(void) {
    char sbuf[32];
    uint16_t adc_value = ntc_read(); 
    float voltage = (float)adc_value * 3.3f / 4095.0f;
    float resistance = 10000.0f * voltage / (3.3f - voltage);
    float temperature = 1.0f / (1.0f/298.15f + logf(resistance/10000.0f)/3950.0f) - 273.15f;
    
    uint16_t temp_color = temperature_to_color_gradient(temperature);
    snprintf(sbuf, sizeof(sbuf), "%.1fC", temperature);
    st7735_write_string(78, 32, sbuf, &font_ascii_8x16, temp_color, ST7735_BLACK);
}

static void task_render_alarm_ui(void) {    //闹钟显示界面
    char buf[32];
    st7735_fill_screen(ST7735_BLACK); 

    st7735_write_string(24, 10, "ALARM SET", &font_ascii_8x16, ST7735_CYAN, ST7735_BLACK);

    uint16_t color_h = (alarm_cursor == 0) ? ST7735_RED : ST7735_WHITE;
    uint16_t color_m = (alarm_cursor == 1) ? ST7735_RED : ST7735_WHITE;
    
    snprintf(buf, sizeof(buf), "%02d", alarm_hour);
    st7735_write_string(4, 45, buf, &font_time_24x48, color_h, ST7735_BLACK);
    
    st7735_write_string(52, 45, ":", &font_time_24x48, ST7735_WHITE, ST7735_BLACK);
    
    snprintf(buf, sizeof(buf), "%02d", alarm_min);
    st7735_write_string(76, 45, buf, &font_time_24x48, color_m, ST7735_BLACK);

    uint16_t color_s = (alarm_cursor == 2) ? ST7735_RED : ST7735_WHITE;
    snprintf(buf, sizeof(buf), "State: %s ", alarm_on ? "ON " : "OFF");
    st7735_write_string(20, 105, buf, &font_ascii_8x16, color_s, ST7735_BLACK);
}

static void task_render_current_page(void) {
    if (current_page == PAGE_MAIN) {
        st7735_fill_screen(ST7735_BLACK); 
        task_update_time_display();       
        task_update_ntc_temperature();    
        task_render_weather_ui();         
    } else if (current_page == PAGE_ALARM) {
        task_render_alarm_ui();
    }
}

/* =========================================================
 * 业务逻辑任务封装
 * ========================================================= */
static void task_prefetch_weather_background(void) {
    int target_index = -1;
    int next_idx = (current_city_index + 1) % CITY_COUNT;
    int prev_idx = (current_city_index - 1 + CITY_COUNT) % CITY_COUNT;

    if (!weather_valid[current_city_index] || (runms - weather_update_time[current_city_index] > 10 * 60 * 1000)) {
        target_index = current_city_index;
    } else if (!weather_valid[next_idx] || (runms - weather_update_time[next_idx] > 10 * 60 * 1000)) {
        target_index = next_idx;
    } else if (!weather_valid[prev_idx] || (runms - weather_update_time[prev_idx] > 10 * 60 * 1000)) {
        target_index = prev_idx;
    }

    if (target_index != -1) {
        const char *rsp;
        if (esp_at_get_http(cities[target_index].uri, &rsp, NULL, 4000)) {
            weather_parse(rsp, &weather_cache[target_index]);
            weather_valid[target_index] = true;
            weather_update_time[target_index] = runms;
            
            if (target_index == current_city_index && current_page == PAGE_MAIN && !is_alarming) {
                task_render_current_page();
            }
        }
    }
}

/* ---------------------------------------------------------
 * 主程序入口
 * --------------------------------------------------------- */
int main(void) {
    /* 初始化硬件 */
    board_lowlevel_init();
    ntc_init();
    led_init();
    KEY_Init();    
    BEEP_Init(); // 初始化蜂鸣器
    rtc_init();    
    EXTIX_Init();
    
    st7735_init();  
    st7735_fill_screen(ST7735_BLACK);

    print_init_log("Initializing...", ST7735_WHITE);
    delay_ms(500);  
    print_init_log("Wait ESP32...", ST7735_WHITE);
    delay_ms(500);
    wifi_init();

    st7735_fill_screen(ST7735_BLACK); 
    uint16_t center_y = 45; 
    st7735_write_string(40,center_y, "Loading", &font_ascii_8x16, ST7735_WHITE, ST7735_BLACK);
    
    uint16_t bar_x = 14, bar_y = center_y + 25, bar_w = 100, bar_h = 8; 
    st7735_fill_rect(bar_x - 1, bar_y - 1, bar_w + 2, 1, ST7735_WHITE);     
    st7735_fill_rect(bar_x - 1, bar_y + bar_h, bar_w + 2, 1, ST7735_WHITE); 
    st7735_fill_rect(bar_x - 1, bar_y, 1, bar_h, ST7735_WHITE);             
    st7735_fill_rect(bar_x + bar_w, bar_y, 1, bar_h, ST7735_WHITE);         

    for (int i = 0; i < CITY_COUNT; i++) {
        const char *rsp;
        if (esp_at_get_http(cities[i].uri, &rsp, NULL, 4000)) {
            weather_parse(rsp, &weather_cache[i]);
            weather_valid[i] = true;
            weather_update_time[i] = 0; 
        }
        uint16_t fill_w = (bar_w * (i + 1)) / CITY_COUNT;
        st7735_fill_rect(bar_x, bar_y, fill_w, bar_h, ST7735_GREEN);
    }
    
    st7735_write_string(44, bar_y + 16, "Ready", &font_ascii_8x16, ST7735_GREEN, ST7735_BLACK);
    delay_ms(1000);

    uint32_t ts;
    if (esp_at_get_time(&ts)) {
        sntp_ok = true;
        rtc_set_timestamp(ts + 8 * 60 * 60); 
    }
    
    current_page = PAGE_MAIN;
    task_render_current_page();

    timer_init(1000);
    timer_elapsed_register(timer_elapsed_callback);
    timer_start();
    
    runms = 0;  
    uint32_t last_runms = runms;
    uint32_t last_key_time = 0;

    while (1) {
        /* =======================================
         * 1. 状态机按键路由逻辑
         * ======================================= */
        if (key_1 || key_2 || (WK_UP == 0) || (PAin(11) == 0)) {
            if (runms - last_key_time > 200) {
                
                // 响铃专属拦截：仅在主界面按下 PA11 时停止闹钟
                if (is_alarming) {
                    if (current_page == PAGE_MAIN && PAin(11) == 0) {
                        is_alarming = false;
                        // ?? 修改：拉高电平关闭蜂鸣器
                        GPIO_SetBits(GPIOB, GPIO_Pin_8); 
                        task_render_current_page();        
                    }
                    last_key_time = runms;
                    key_1 = 0; key_2 = 0;
                    continue; 
                }
                
                // --- 全局菜单键 (KEY3 / PA9)：切换页面 ---
                if (WK_UP == 0) {
                    current_page = (current_page == PAGE_MAIN) ? PAGE_ALARM : PAGE_MAIN;
                    task_render_current_page();
                }
                // --- 主页面操作 ---
                else if (current_page == PAGE_MAIN) {
                    if (KEY0 == 0) current_city_index = (current_city_index + 1) % CITY_COUNT;
                    else if (KEY1 == 0) current_city_index = (current_city_index - 1 + CITY_COUNT) % CITY_COUNT;
                    task_render_current_page();
                }
                // --- 闹钟页面操作 ---
                else if (current_page == PAGE_ALARM) {
                    if (PAin(11) == 0) { 
                        alarm_cursor = (alarm_cursor + 1) % 3; 
                    } else if (KEY0 == 0) { 
                        if (alarm_cursor == 0) alarm_hour = (alarm_hour + 1) % 24;
                        else if (alarm_cursor == 1) alarm_min = (alarm_min + 1) % 60;
                        else if (alarm_cursor == 2) alarm_on = !alarm_on;
                    } else if (KEY1 == 0) { 
                        if (alarm_cursor == 0) alarm_hour = (alarm_hour - 1 + 24) % 24;
                        else if (alarm_cursor == 1) alarm_min = (alarm_min - 1 
                            + 60) % 60;
                        else if (alarm_cursor == 2) alarm_on = !alarm_on;
                    }
                    task_render_alarm_ui(); 
                }
                
                last_key_time = runms; 
            }
            key_1 = 0;
            key_2 = 0;
        }

        if (runms == last_runms) continue;
        last_runms = runms;

        /* =======================================
         * 2. 闹钟响铃任务 (3分钟限制 + 蜂鸣器配合屏幕爆闪)
         * ======================================= */
        if (is_alarming) {
            // 如果持续超过 3 分钟 (180000 毫秒)，自动停止
            if (runms - alarm_start_time >= 180000) {
                is_alarming = false;
                // ?? 修改：拉高电平关闭蜂鸣器
                GPIO_SetBits(GPIOB, GPIO_Pin_8); 
                task_render_current_page();        
            } else {
                // 每 500ms 翻转一次状态
                if (last_runms % 1000 == 0) {
                    // ?? 修改：拉低电平发声
                    GPIO_ResetBits(GPIOB, GPIO_Pin_8); 
                    st7735_fill_screen(ST7735_RED);
                    st7735_write_string(32, 56, "WAKE UP!", &font_ascii_8x16, ST7735_WHITE, ST7735_RED);
                } else if (last_runms % 1000 == 500) {
                    // ?? 修改：拉高电平停止发声
                    GPIO_SetBits(GPIOB, GPIO_Pin_8); 
                    st7735_fill_screen(ST7735_BLACK);
                }
            }
            continue; 
        }

        /* =======================================
         * 3. 时钟更新与闹钟触发 (100ms)
         * ======================================= */
        if (last_runms % 100 == 0) {
            rtc_date_t date;
            rtc_get_date(&date);
            
            if (alarm_on && date.hour == alarm_hour && date.minute == alarm_min && date.second == 0 && !is_alarming) {
                is_alarming = true;
                alarm_start_time = runms; 
            }

            if (current_page == PAGE_MAIN) {
                task_update_time_display();
            }
        }

        if (last_runms % 1000 == 0 && current_page == PAGE_MAIN) {
            task_update_ntc_temperature();
        }

        if (!sntp_ok || last_runms % (60 * 60 * 1000) == 0) {
            uint32_t ts2;
            if (esp_at_get_time(&ts2)) {
                sntp_ok = true;
                rtc_set_timestamp(ts2 + 8 * 60 * 60); 
            }
        }

        if (last_runms % 3000 == 0) {
            task_prefetch_weather_background();
        }
    }
}
