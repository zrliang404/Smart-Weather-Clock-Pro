#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "weather.h"

/**
 * @brief 辅助函数：从 JSON 字符串中提取指定键的字符串值
 * 
 * @param data    原始 JSON 字符串
 * @param key     要查找的键名，包含冒号和前导引号，如 "\"name\":\""
 * @param out_buf 存储提取结果的缓冲区
 * @param max_len 缓冲区的最大长度 (一般传入 sizeof(buf))
 * @return true   提取成功
 * @return false  提取失败（未找到键或格式错误）
 */
static bool extract_json_string(const char *data, const char *key, char *out_buf, size_t max_len)
{
    char *p = strstr(data, key);
    if (p == NULL) return false;
    
    p += strlen(key);
    char *q = strchr(p, '\"');
    if (q == NULL) return false;
    
    size_t len = q - p;
    if (len >= max_len) {
        len = max_len - 1;
    }
    
    strncpy(out_buf, p, len);
    out_buf[len] = '\0';
    return true;
}

bool weather_parse(const char *data, weather_t *weather)
{
    /* 初始化结构体，确保所有字段为空 */
    memset(weather, 0, sizeof(weather_t));
    
    /* 1. 提取各个字段 (利用辅助函数极大简化代码) */
    if (!extract_json_string(data, "\"name\":\"", weather->city, sizeof(weather->city))) return false;
    if (!extract_json_string(data, "\"text\":\"", weather->weather, sizeof(weather->weather))) return false;
    if (!extract_json_string(data, "\"temperature\":\"", weather->temperature, sizeof(weather->temperature))) return false;
    
    /* 2. 解析最后更新时间与星期几 */
    if (extract_json_string(data, "\"last_update\":\"", weather->last_update, sizeof(weather->last_update)))
    {
        struct tm timeinfo = {0};
        int year, month, day, hour, min, sec;
        
        /* 解析ISO 8601格式的时间字符串，例如：2024-08-26T16:20:13+08:00 */
        if (sscanf(weather->last_update, "%d-%d-%dT%d:%d:%d", 
                   &year, &month, &day, &hour, &min, &sec) == 6) 
        {
            timeinfo.tm_year = year - 1900;
            timeinfo.tm_mon  = month - 1;
            timeinfo.tm_mday = day;
            timeinfo.tm_hour = hour;
            timeinfo.tm_min  = min;
            timeinfo.tm_sec  = sec;
            timeinfo.tm_isdst = -1; 

            /* 将tm结构体转换为时间戳，并自动填充tm_wday */
            time_t timestamp = mktime(&timeinfo);
            if (timestamp != (time_t)-1) 
            {
                const char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
                int wday_index = timeinfo.tm_wday;
                
                if (wday_index >= 0 && wday_index < 7) {
                    strncpy(weather->weekday, weekdays[wday_index], sizeof(weather->weekday) - 1);
                    weather->weekday[sizeof(weather->weekday) - 1] = '\0';
                }
            }
        }
    }

    return true;
}
