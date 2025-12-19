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
#define COMP_UART      0   // USART1 -> GPS

#define BAUD          9600UL
#define TIMEOUT 5000UL

// UBRR = F_CPU/(16*BAUD) - 1
#define UBRR_VALUE    ((F_CPU / (16UL * BAUD)) - 1)

bool new_sms = false;
char idx_str[8];

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

volatile uint32_t g_ms = 0;

ISR(TIMER0_COMP_vect)
{
    g_ms++;
}

static void time_init_ms(void)
{
    // CTC
    TCCR0 = (1 << WGM01);
    // prescaler 64
    TCCR0 |= (1 << CS01) | (1 << CS00);

    // 16MHz / 64 = 250kHz, чтобы получить 1kHz нужно OCR0=249
    OCR0 = 249;

    // enable compare match interrupt
    TIMSK |= (1 << OCIE0);

    sei();
}

static inline uint32_t millis(void)
{
    uint32_t t;
    cli();
    t = g_ms;
    sei();
    return t;
}


int main(void)
{
    //init_uart();
    //sei();

    // UART0 = SIM800
    USART_Init(UBRR_VALUE, SIM_UART);
    // UART1 = GPS
    USART_Init(UBRR_VALUE, GPS_UART);

    // PA0 – выход, сразу в 1 (открываем MOSFET)
    DDRA |= (1 << PA0);
    PORTA |= (1 << PA0);

    // Немного подождём, пока модем проснётся
    //_delay_ms(2000);

    // Инициализация модема
    sendATCommand("AT", true);
    sendATCommand("AT+CMGF=1", true);            // текстовый режим SMS
    sendATCommand("AT+CNMI=0,0,0,0,0", true);    // +CMTI при новой SMS
    sendATCommand("ATE0", true);                 // Выключаем эхо
    sendATCommand("AT&W", true);                 // сохранить профиль

    sendATCommand("AT+CHFA=2", true);
    sendATCommand("AT+CLVL=100", true);

    sendATCommand("AT+CPMS=\"SM\",\"SM\",\"SM\"", true);
    sendATCommand("AT+CMGD=1,1", true);   // удалить все прочитанный сообщения
    sendATCommand("AT+CPMS?", true);
    sendATCommand("AT+CMGL=\"ALL\"", true);

    sendSMS("+79915760104", "START");
    _delay_ms(3000);

    // Буфер для строк от SIM (URC: +CMTI, RING и т.п.)
    static char sim_line[RESP_BUF_SIZE];
    uint16_t sim_pos = 0;

    // Буфер для GPS
    uint16_t gps_pos = 0;

    uint16_t tick = 0;

    USART_TransmitString("UART_TERM_CONNECTED", COMP_UART); 
    USART_TransmitString("\n", COMP_UART);

    time_init_ms();

    uint32_t last_poll = 0;
    char body[160];

    for (;;)
    {
        if ((uint32_t)(millis() - last_poll) >= TIMEOUT)
        {    
            sendATCommand("AT+STTONE=1,16,1000", true);

            last_poll = millis();

            while (sim_sms_get_unread_body_from("+79915760104", body, sizeof(body)))
            {
                USART_TransmitString("GOT SMS: ", COMP_UART);
                USART_TransmitString(body, COMP_UART);
                USART_TransmitString("\n", COMP_UART);

                sendSMS("+79915760104", body);
                _delay_ms(3000);
            }
        }


        // тут могут быть другие задачи (GPS и т.п.)
    }

    return 0;
}
