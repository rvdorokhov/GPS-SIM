#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "uart.h"
#include "sim800l.h"
#include "gps.h"
#include "utils.h"

// ---------------------------------------------------
// Настройки
// ---------------------------------------------------
#define COMP_UART      1   // USART1 -> GPS

#define BAUD          9600UL

// UBRR = F_CPU/(16*BAUD) - 1
#define UBRR_VALUE    ((F_CPU / (16UL * BAUD)) - 1)

int main(void)
{
   // stdin = &uart_stdin;
    //stdout = &uart_stdout;

    //init_uart();
    //sei();

    // UART0 = SIM800
    USART_Init(UBRR_VALUE, SIM_UART);
    // UART1 = GPS
    USART_Init(UBRR_VALUE, GPS_UART);

    // USART_TransmitString("UART_TERM_CONNECTED", COMP_UART); 

    // PA0 – выход, сразу в 1 (открываем MOSFET)
    DDRA |= (1 << PA0);
    PORTA |= (1 << PA0);

    // Немного подождём, пока модем проснётся
    //_delay_ms(2000);

    // Инициализация модема
    sendATCommand("AT", true);
    sendATCommand("AT+CMGF=1", true);            // текстовый режим SMS
    sendATCommand("AT+CNMI=2,1,0,0,0", true);    // +CMTI при новой SMS
    sendATCommand("ATE0", true);                 // Выключаем эхо
    sendATCommand("AT&W", true);                 // сохранить профиль

    sendSMS("+79915760104", "success1");
    _delay_ms(5000);
    sendSMS("+79915760104", "success2");
    _delay_ms(5000);
    sendSMS("+79915760104", "success3");
    _delay_ms(5000);

    // Буфер для строк от SIM (URC: +CMTI, RING и т.п.)
    char sim_line[RESP_BUF_SIZE];
    uint16_t sim_pos = 0;

    // Буфер для GPS
    uint16_t gps_pos = 0;

    uint16_t tick = 0;

    for (;;)
    {
        /*
        // --------------------------------------------------
        // PC → SIM800
        // --------------------------------------------------
        if (uart_available(COMP_UART))
        {
            char c = USART_Receive(COMP_UART);
            USART_Transmitchar(c, COMP_UART);
        }

        // --------------------------------------------------
        // SIM800 → PC
        // --------------------------------------------------
        if (uart_available(SIM_UART))
        {
            char c = USART_Receive(SIM_UART);
            USART_Transmitchar(c, COMP_UART);
        }

        _delay_ms(1);
        tick++;

        if (tick >= 10000)  // 1 секунда
        {
            tick = 0;

            USART_TransmitString("-> AT\r\n", COMP_UART);  // debug в терминал
            //USART_TransmitString("AT\r", SIM_UART);        // команда в SIM800
            USART_TransmitString("AT+CBC\r", SIM_UART);        // команда в SIM800
            
        }
        */

        // ---------- SIM800: читаем построчно ----------
        if (uart_available(SIM_UART))
        {
            char c = USART_Receive(SIM_UART);
            if (c == '\r')
            {
                // игнорируем CR
            }
            else if (c == '\n')
            {
                sim_line[sim_pos] = '\0';
                str_trim(sim_line);

                if (sim_pos > 0)
                {
                    // Обработка строки
                    if (str_starts_with(sim_line, "RING"))
                    {
                        sendATCommand("ATH", true);
                    }
                    else if (str_starts_with(sim_line, "+CMTI:"))
                    {
                        // +CMTI: "ME",20
                        const char *comma = strrchr(sim_line, ',');
                        if (comma)
                        {
                            char idx_str[8];
                            strncpy(idx_str, comma + 1, sizeof(idx_str) - 1);
                            idx_str[sizeof(idx_str) - 1] = '\0';
                            str_trim(idx_str);

                            char cmd[32];
                            snprintf(cmd, sizeof(cmd), "AT+CMGR=%s", idx_str);
                            char *resp = sendATCommand(cmd, true);
                            handleSMS(resp);
                        }
                    }
                }

                sim_pos = 0;
            }
            else
            {
                if (sim_pos < (RESP_BUF_SIZE - 1))
                {
                    sim_line[sim_pos++] = c;
                }
            }
        }

        // ---------- GPS: читаем построчно ----------
        /*
        c = uart_getchar_nb(GPS_UART, &has);
        if (has)
        {
            if (c == '\r')
            {
                // пропускаем
            }
            else if (c == '\n')
            {
                gps_line[gps_pos] = '\0';
                if (gps_pos > 0 && needGPS)
                {
                    processResponseGPS(gps_line);
                    needGPS = false;   // один запрос -> один ответ
                }
                gps_pos = 0;
            }
            else
            {
                if (gps_pos < (GPS_BUF_SIZE - 1))
                    gps_line[gps_pos++] = c;
            }
        }
        */
    }


    return 0;
}
