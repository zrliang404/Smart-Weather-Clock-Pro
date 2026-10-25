#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "main.h"
#include "esp_usart.h"
#include "esp_at.h"


/* 串口接收缓存给4K大小 */
#define RX_BUFFER_SIZE  4096

#define RX_RESULT_OK    0   /* AT命令返回OK */
#define RX_RESULT_ERROR 1   /* AT命令返回ERROR */
#define RX_RESULT_FAIL  2   /* 数据接收错误，缓存区满 */


static uint8_t rxdata[RX_BUFFER_SIZE];  /* 接收缓存 */
static uint32_t rxlen;  /* 接收数据长度 */
static bool rxready;  /* rxready=true允许接收数据，防止异常数据进入rxdata */
static uint8_t rxresult; /* 接收结果 */


/**
 * @brief 串口接收回调函数
 *
 * @param data 串口接收到的数据
 */
static void on_usart_received(uint8_t data)
{
    /* 接收数据标志位为false，不接收数据，只有MCU发了AT命令后，才会开启rxready准备接收数据 */
    if (!rxready)
    {
        return;
    }

    /* 接收数据长度必须小于缓存区大小 */
    if (rxlen < RX_BUFFER_SIZE)
    {
        /* 接收数据 */
        rxdata[rxlen++] = data;
    }
    else
    {
        /* 缓存区满，接收失败 */
        rxresult = RX_RESULT_FAIL;
        rxready = false;
        return;
    }

    /* 判断是否接收到换行符，表示数据接收完毕 */
    if (data == '\n')
    {
        /* 判断接收换行符是否为\r\n */
        if (rxlen >= 2 && rxdata[rxlen - 2] == '\r')
        {
            /* 判断收到的是否是OK */
            if (rxlen >= 4 &&
                rxdata[rxlen - 4] == 'O' && rxdata[rxlen - 3] == 'K')
            {
                /* AT命令执行成功 */
                rxresult = RX_RESULT_OK;
                rxready = false;
            }
            /* 判断收到的是否是ERROR */
            else if (rxlen >= 7 &&
                     rxdata[rxlen - 5] == 'E' && rxdata[rxlen - 4] == 'R' &&
                     rxdata[rxlen - 3] == 'R' && rxdata[rxlen - 2] == 'O' &&
                     rxdata[rxlen - 1] == 'R')
            {
                /* AT命令返回ERROR，表示AT命令执行失败 */
                rxresult = RX_RESULT_ERROR;
                rxready = false;
            }
        }
    }
}

/**
 * @brief 初始化esp32的at通信模块，包括串口初始化、AT命令执行结果回调函数注册
 *
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool esp_at_init(void)
{
    /* rxready=false，不接收数据 */
    rxready = false;

    /* 初始化串口2，波特率115200，PA2 PA3 */
    esp_usart_init();
    /* 注册串口接收回调函数 */
    esp_usart_receive_register(on_usart_received);

    /* 发送复位esp32命令，重置esp32 */
    return esp_at_reset();
}

/**
 * @brief 发送AT命令到esp32
 *
 * @param cmd AT命令字符串
 * @param rsp AT命令执行结果
 * @param length AT命令执行结果长度
 * @param timeout AT命令执行超时时间
 * @return true AT命令执行成功
 * @return false AT命令执行失败
 */
bool esp_at_send_command(const char *cmd, const char **rsp, uint32_t *length, uint32_t timeout)
{
    rxlen = 0;  /* rxlen=0，接收数据长度清零 */
    rxready = true; /* rxready=true，准备接收数据 */
    rxresult = RX_RESULT_FAIL; /* rxresult=RX_RESULT_FAIL，先预置接收结果失败 */

    /* 通过串口发送AT命令 */
    esp_usart_write_string(cmd);
    esp_usart_write_string("\r\n");

    /* 等待AT命令执行结果 */
    while (rxready && timeout--)
    {
        delay_ms(1);
    }
    rxready = false; /* rxready=false，无论接收成功or失败or超时，都不再接收数据了 */

    /* 如果rsp不为空，将接收到的数据返回 */
    if (rsp)
    {
        *rsp = (const char *)rxdata;
    }
    if (length)
    {
        *length = rxlen;
    }

    return rxresult == RX_RESULT_OK;
}

/**
 * @brief 发送数据到esp32
 *
 * @param data 待发送的数据
 * @param length 待发送数据的长度
 * @return true 发送成功
 * @return false 发送失败（不会出现失败情况）
 */
bool esp_at_send_data(const uint8_t *data, uint32_t length)
{
    esp_usart_write_data((uint8_t *)data, length);

    return true;
}

/**
 * @brief 复位esp32
 *
 * @return true 复位成功
 * @return false 复位失败
 */
bool esp_at_reset(void)
{
    /* 复位esp32命令 */
    if (!esp_at_send_command("AT+RESTORE", NULL, NULL, 1000))
    {
        return false;
    }
    delay_ms(2000); /* 等待esp32重启 */

    /* 关闭回显 */
    if (!esp_at_send_command("ATE0", NULL, NULL, 1000))
    {
        return false;
    }

    /* 关闭配置储存 */
    if (!esp_at_send_command("AT+SYSSTORE=0", NULL, NULL, 1000))
    {
        return false;
    }

    return true;
}

/**
 * @brief 初始化esp32的wifi模块，设置为station模式
 *
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool esp_at_wifi_init(void)
{
    /* 设置为station模式 */
    if (!esp_at_send_command("AT+CWMODE=1", NULL, NULL, 1000))
    {
        return false;
    }

    return true;
}

/**
 * @brief 连接wifi
 *
 * @param ssid wifi名称
 * @param pwd wifi密码
 * @return true 连接成功
 * @return false 连接失败
 */
bool esp_at_wifi_connect(const char *ssid, const char *pwd)
{
    char cmd[64];

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    if (!esp_at_send_command(cmd, NULL, NULL, 10000))
    {
        return false;
    }

    return true;
}

/**
 * @brief 获取http数据
 *
 * @param url http地址
 * @param rsp http响应数据
 * @param length http响应数据长度
 * @param timeout 超时时间
 * @return true 获取成功
 * @return false 获取失败
 */
bool esp_at_get_http(const char *url, const char **rsp, uint32_t *length, uint32_t timeout)
{
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "AT+HTTPCGET=\"%s\"", url);
    if (!esp_at_send_command(cmd, rsp, length, 10000))
    {
        return false;
    }

    return true;
}

/**
 * @brief 初始化sntp时间
 *
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool esp_at_sntp_init(void)
{
    /* 设置sntp服务器 */
    if (!esp_at_send_command("AT+CIPSNTPCFG=1,8,\"cn.ntp.org.cn\",\"ntp.sjtu.edu.cn\"", NULL, NULL, 1000))
    {
        return false;
    }

    /* 从sntp服务器获取时间，此时会校准esp32的rtc时间 */
    if (!esp_at_send_command("AT+CIPSNTPTIME?", NULL, NULL, 1000))
    {
        return false;
    }

    return true;
}

/**
 * @brief 从esp32获取时间戳
 *
 * @param timestamp 时间戳
 * @return true 获取成功
 * @return false 获取失败
 */
bool esp_at_get_time(uint32_t *timestamp)
{
    const char *rsp;
    uint32_t length;

    if (!esp_at_send_command("AT+SYSTIMESTAMP?", &rsp, &length, 1000))
    {
        return false;
    }

    char *sts = strstr(rsp, "+SYSTIMESTAMP:") + strlen("+SYSTIMESTAMP:");

    *timestamp = atoi(sts);

    return true;
}

/**
 * @brief 获取wifi的ip地址，ip地址格式为xxx.xxx.xxx.xxx
 *
 * @param ip ip地址
 * @return true 获取成功
 * @return false 获取失败
 */
bool esp_at_wifi_get_ip(char ip[16])
{
    const char *rsp;
    if (!esp_at_send_command("AT+CIPSTA?", &rsp, NULL, 1000))
    {
        return false;
    }

    /* 解析ip地址，响应示例：+CIPSTA:ip:192.168.1.1 */
    const char *pip = strstr(rsp, "+CIPSTA:ip:") + strlen("+CIPSTA:ip:");

    /* 如果响应中有ip地址，则拷贝到ip中 */
    if (pip)
    {
        for (int i = 0; i < 16; i++)
        {
            if (pip[i] == '\r')
            {
                ip[i] = '\0';
                break;
            }
            ip[i] = pip[i];
        }
        return true;
    }

    return true;
}

/**
 * @brief 获取wifi的mac地址，mac地址格式为xx:xx:xx:xx:xx:xx
 *
 * @param mac mac地址
 * @return true 获取成功
 * @return false 获取失败
 */
bool esp_at_wifi_get_mac(char mac[18])
{
    const char *rsp;
    if (!esp_at_send_command("AT+CIPSTAMAC?", &rsp, NULL, 1000))
    {
        return false;
    }

    /* 解析mac地址，响应示例：+CIPSTAMAC:mac:00:11:22:33:44:55 */
    const char *pmac = strstr(rsp, "+CIPSTAMAC:mac:") + strlen("+CIPSTAMAC:mac:");
    /* 如果响应中有mac地址，则拷贝到mac中 */
    if (pmac)
    {
        strncpy(mac, pmac, 18);
        return true;
    }

    return true;
}
