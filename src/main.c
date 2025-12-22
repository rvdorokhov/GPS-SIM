#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "uart.h"
#include "sim800l.h"
#include "gps.h"
#include "utils.h"


#define COMP_UART 0   // отладка

#define BAUD 9600UL
#define TIMEOUT 5000UL // период опроса SIM800L о новых сообщениях

// UBRR = F_CPU/(16*BAUD) - 1 - настройка делителя UART для 9600 бодрейта
#define UBRR_VALUE    ((F_CPU / (16UL * BAUD)) - 1)



const char USER_PHONE[] = "+79915760104"; // вместо define от греха подальше - чтобы все же был отдельный char[] в памяти



bool gps_status = false;
char idx_str[8];

volatile uint32_t g_ms = 0; // глобальный счетчик миллисекунд
uint32_t last_poll;

ISR(TIMER0_COMP_vect) // Прерывание
{
    g_ms++; // увеличивает счетчик каждую миллисекунду 
}

static void time_init_ms(void)
{
    // Таймер0 в режиме CTC (Clear Timer on Compare)
    TCCR0 = (1 << WGM01);

    // Предделитель 64 16MHz/64 = 250 kHz
    TCCR0 |= (1 << CS01) | (1 << CS00);

    // 250kHz / (249+1) = 1000 Hz  - 1 мс тик
    OCR0 = 249;

    // Разрешаем прерывание по совпадению OCR0
    TIMSK |= (1 << OCIE0);

    // Глобальное разрешение прерываний 
    sei();
}

// Возвращает текущее время в миллисекундах от старта
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
    // UART0 = SIM800
    USART_Init(UBRR_VALUE, SIM_UART);
    // UART1 = GPS
    USART_Init(UBRR_VALUE, GPS_UART);

    // PA0 – выход
    DDRA |= (1 << PA0);

    // Инициализация модема
    sendATCommand("AT", true);
    sendATCommand("AT+CMGF=1", true);            // текстовый режим SMS
    sendATCommand("AT+CNMI=0,0,0,0,0", true);    // отключаем уведомления
    sendATCommand("ATE0", true);                 // Выключаем эхо
    sendATCommand("AT&W", true);                 // сохранить профиль

    sendATCommand("AT+CHFA=2", true);            // выбор нужного аудиоканала
    sendATCommand("AT+CLVL=100", true);          // громкость динамика на 100

    sendATCommand("AT+CPMS=\"SM\",\"SM\",\"SM\"", true);  // настраиваем смс на память sim-карты
    sendATCommand("AT+CMGD=1,4", true);   // удаляем все сообщения
    // sendATCommand("AT+CMGD=1,1", true);   // удаляем все прочитанные сообщения
    // sendATCommand("AT+CPMS?", true);
    // sendATCommand("AT+CMGL=\"ALL\"", true);

    sendSMS(USER_PHONE, "START\nCOMMANDS: GPS COORDINATES SIGNAL");

    // USART_TransmitString("UART_TERM_CONNECTED", COMP_UART); 
    // USART_TransmitString("\n", COMP_UART);

    time_init_ms();

    last_poll = 0;
    char body[160];

    for (;;)
    {
        if ((uint32_t)(millis() - last_poll) >= TIMEOUT)
        {    
            last_poll = millis();

            while (sim_sms_get_unread_body_from(USER_PHONE, body, sizeof(body)))
            {
                // USART_TransmitString("GOT SMS: ", COMP_UART);
                // USART_TransmitString(body, COMP_UART);
                //USART_TransmitString("\n", COMP_UART);

                if (strstr(body, "SIGNAL") != NULL) {
                    sendATCommand("AT+STTONE=1,16,5000", true);
                }

                if (strstr(body, "COORDINATES") != NULL) {
                    if (gps_status) {
                        char nmea[128];
                        if (gps_read_nmea_line(nmea, sizeof(nmea)))
                        {
                            processResponseGPS(nmea); 
                        } else {
                            sendSMS(USER_PHONE, "GPS doesnt work...");
                        }
                    } else {
                        sendSMS(USER_PHONE, "GPS is off. Turn on GPS");
                    }
                }

                if (strstr(body, "GPS") != NULL) {
                    gps_status = !gps_status;

                    if (gps_status) {
                        PORTA |=  (1 << PA0);
                        sendSMS(USER_PHONE, "GPS is on");
                    } else {
                        PORTA &= ~(1 << PA0);
                        sendSMS(USER_PHONE, "GPS is off");
                    }
                }
            }
        }
    }

    return 0;
}