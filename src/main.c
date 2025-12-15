#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "uart.h"

// ---------------------------------------------------
// Настройки
// ---------------------------------------------------
#define SIM_UART      0   // USART0 -> SIM800
#define GPS_UART      1   // USART1 -> GPS
#define COMP_UART      1   // USART1 -> GPS

#define BAUD          9600UL

// UBRR = F_CPU/(16*BAUD) - 1
#define UBRR_VALUE    ((F_CPU / (16UL * BAUD)) - 1)

// Буферы
#define RESP_BUF_SIZE 256
#define GPS_BUF_SIZE   96

static char sim_resp[RESP_BUF_SIZE];
static char gps_line[GPS_BUF_SIZE];

static bool needGPS = false;

// ---------------------------------------------------
// Вспомогательные функции для строк
// ---------------------------------------------------
static void str_trim(char *s)
{
    // обрезаем пробелы/CR/LF в начале и в конце
    char *p = s;
    while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')
        p++;

    if (p != s)
        memmove(s, p, strlen(p) + 1);

    size_t len = strlen(s);
    while (len > 0 &&
           (s[len - 1] == ' ' || s[len - 1] == '\r' || s[len - 1] == '\n' || s[len - 1] == '\t'))
    {
        s[len - 1] = '\0';
        len--;
    }
}

static bool str_starts_with(const char *s, const char *prefix)
{
    size_t lp = strlen(prefix);
    return strncmp(s, prefix, lp) == 0;
}

// ---------------------------------------------------
// Работа с UART0 (SIM) / UART1 (GPS)
// ---------------------------------------------------
static inline uint8_t uart_available(uint8_t n)
{
    if (n == 0)
        return (UCSR0A & (1 << RXC0)) != 0;
    else
        return (UCSR1A & (1 << RXC1)) != 0;
}

/*
static inline char uart_getchar_nb(uint8_t n, bool *has)
{
    if (!uart_available(n))
    {
        *has = false;
        return 0;
    }

    *has = true;
    if (n == 0)
        return UDR0;
    else
        return UDR1;
}*/

// ---------------------------------------------------
// Отправка строк в SIM
// ---------------------------------------------------
static void sim_send_line(const char *s)
{
    USART_TransmitString((char *)s, SIM_UART);
    USART_Transmitchar('\r', SIM_UART);   // SIM800 достаточно CR
    
    char buf[80];
    snprintf(buf, sizeof(buf), "send line to sim: %s\r\n", s);
   // USART_TransmitString(buf, COMP_UART);
}

// ---------------------------------------------------
// Ожидание ответа от SIM
// читаем до "тишины" ~100 мс или максимум буфера
// ---------------------------------------------------
static char *sim_wait_response(void)
{
    memset(sim_resp, 0, sizeof(sim_resp));
    uint16_t pos = 0;

    uint16_t idle_ms = 0;
    const uint16_t MAX_IDLE_MS = 100;   // тишина 100мс – считаем, что все пришло
    const uint16_t MAX_TOTAL_MS = 10000; // максимум 10с

    uint16_t total_ms = 0;

    while (total_ms < MAX_TOTAL_MS)
    {
        bool has;
        char c = USART_Receive(SIM_UART);

        if (has)
        {
            if (pos < (RESP_BUF_SIZE - 1))
            {
                sim_resp[pos++] = c;
                sim_resp[pos] = '\0';
            }
            idle_ms = 0;    // пришел байт – обнуляем счётчик тишины
        }
        else
        {
            _delay_ms(1);
            idle_ms++;
            total_ms++;

            if (idle_ms >= MAX_IDLE_MS) {
               // USART_TransmitString("MAX_IDLE_MS IS OVER", COMP_UART);
                break;
            }    
        }
    }

    return sim_resp;
}

// ---------------------------------------------------
// sendATCommand (cmd без CR в конце)
// если waiting = true – ждём ответ в sim_resp
// ---------------------------------------------------
static char *sendATCommand(const char *cmd, bool waiting)
{
    sim_resp[0] = '\0';

    sim_send_line(cmd);

   // USART_TransmitString("send command: ", COMP_UART);
  //  USART_TransmitString(cmd, COMP_UART);

    if (waiting)
    {
        char *resp = sim_wait_response();

        // Выкидываем эхо-команду
        /*
        if (str_starts_with(resp, cmd))
        {
            char *p = strstr(resp, "\r");
            if (p)
            {
                // пропускаем \r\n
                p += 2;
                memmove(resp, p, strlen(p) + 1);
            }
        } */

      //  USART_TransmitString("got response:", COMP_UART);
      //  USART_TransmitString(resp, COMP_UART);

        return resp;
    }

    return sim_resp;
}

// ---------------------------------------------------
// Отправка SMS
// ---------------------------------------------------
static void sendSMS(const char *phone, const char *message)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", phone);

    /////////////////////
   // USART_TransmitString(cmd, COMP_UART); 
    //////////////////////

    sendATCommand(cmd, true);          // перешли в режим ввода текста

    // текст + Ctrl+Z
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s\r\n%c", message, 26);

    /////////////////////
 //   USART_TransmitString(tmp, COMP_UART); 
    //////////////////////

    sendATCommand(tmp, true);
}

// ---------------------------------------------------
// Парсинг SMS 
// ---------------------------------------------------
static void handleSMS(const char *msg)
{
    sendSMS("+79915760104", "handle sms: ");
    sendSMS("+79915760104", msg);
    // Ищем "+CMGR: "
    const char *cmgr = strstr(msg, "+CMGR: ");
    if (!cmgr)
        return;

    // Заголовок до первой \r
    const char *cr = strstr(cmgr, "\r");
    if (!cr)
        return;

    size_t header_len = cr - cmgr;
    char header[128];
    if (header_len >= sizeof(header))
        header_len = sizeof(header) - 1;
    memcpy(header, cmgr, header_len);
    header[header_len] = '\0';

    // Тело SMS начинается через два символа после \r: то есть после "\r\n"
    const char *body_start = cr + 2;
    const char *ok_pos = strstr(body_start, "OK");
    if (!ok_pos)
        return;

    // Копируем тело SMS в буфер body[]
    size_t body_len = ok_pos - body_start;
    char body[160];
    if (body_len >= sizeof(body))
        body_len = sizeof(body) - 1;
    memcpy(body, body_start, body_len);
    body[body_len] = '\0';
    str_trim(body);

    // Номер телефона в header: между первым и вторым ","
    // +CMGR: "REC UNREAD","+79915760104","","..."
    char phone[32] = {0};
    const char *p1 = strstr(header, "\",\"");
    if (p1)
    {
        p1 += 3;
        const char *p2 = strstr(p1, "\",\"");
        if (p2)
        {
            size_t phone_len = p2 - p1;
            if (phone_len >= sizeof(phone))
                phone_len = sizeof(phone) - 1;
            memcpy(phone, p1, phone_len);
            phone[phone_len] = '\0';
        }
    }

    // Переключаем needGPS
    needGPS = !needGPS;

    char reply[64];
    snprintf(reply, sizeof(reply),
             "sms handle success, gpsneed is %d",
             needGPS ? 1 : 0);

    sendSMS("+79915760104", reply);
    // Можно было бы на phone, если отвечать отправителю.
}

// ---------------------------------------------------
// GPS: конвертация NMEA координат в десятичные
// coord: "ddmm.mmmm" или "dddmm.mmmm"
// hemisphere: "N","S","E","W"
// ---------------------------------------------------
static double nmeaToDecimal(const char *coord, char hemisphere)
{
    double raw = atof(coord);
    int deg = (int)(raw / 100.0);
    double minutes = raw - deg * 100.0;
    double decimal = deg + minutes / 60.0;

    if (hemisphere == 'S' || hemisphere == 'W')
        decimal = -decimal;

    return decimal;
}

// ---------------------------------------------------
// Обработка одной NMEA-строки
// ---------------------------------------------------
static void processResponseGPS(const char *sentence)
{
    if (!str_starts_with(sentence, "$GNRMC") &&
        !str_starts_with(sentence, "$GPRMC"))
        return;

    // Копию сделаем, чтобы резать
    char buf[GPS_BUF_SIZE];
    strncpy(buf, sentence, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // Отрежем по '*'
    char *star = strchr(buf, '*');
    if (star)
        *star = '\0';

    // Разбиваем по запятым
    char *fields[20];
    int fieldCount = 0;

    char *p = buf;
    while (fieldCount < 20)
    {
        fields[fieldCount++] = p;
        char *comma = strchr(p, ',');
        if (!comma)
            break;
        *comma = '\0';
        p = comma + 1;
    }

    if (fieldCount < 7)
        return;

    const char *status = fields[2];
    const char *latStr = fields[3];
    const char *latHemStr = fields[4];
    const char *lonStr = fields[5];
    const char *lonHemStr = fields[6];

    bool hasFix = (status[0] == 'A');
    if (!hasFix)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "[GPS] NO FIX %s", sentence);
        sendSMS("+79915760104", msg);
        return;
    }

    double lat = nmeaToDecimal(latStr, latHemStr[0]);
    double lon = nmeaToDecimal(lonStr, lonHemStr[0]);

    char lat_buf[16], lon_buf[16];
    dtostrf(lat, 2, 6, lat_buf);
    dtostrf(lon, 3, 6, lon_buf);

    char msg[96];
    snprintf(msg, sizeof(msg),
             "[Yandex] Koordinaty: %s,%s", lat_buf, lon_buf);

    sendSMS("+79915760104", msg);
}

// ---------------------------------------------------
// main
// ---------------------------------------------------
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

    USART_TransmitString("UART_TERM_CONNECTED", COMP_UART); 





    // В main, после инициализации UART
    DDRB |= (1 << PB0);  // PB0 как выход (например, светодиод)

    PORTB |= (1 << PB0);    // Output HIGH on PE0

    DDRE |= (1 << PE0);     // PE0 direction = OUTPUT

    // Ставим PE0 в логическую 1 (HIGH)
    PORTE |= (1 << PE0);    // Output HIGH on PE0





    // PA0 – выход, сразу в 1 (открываем MOSFET)
    DDRA |= (1 << PA0);
    PORTA |= (1 << PA0);

    // Немного подождём, пока модем проснётся
    _delay_ms(2000);

    // Инициализация модема
   // sendATCommand("AT", true);
    //sendATCommand("AT+CMGF=1", true);            // текстовый режим SMS
    //sendATCommand("AT+CNMI=2,1,0,0,0", true);    // +CMTI при новой SMS
    //sendATCommand("ATE0", true);                 // Выключаем эхо
    //sendATCommand("AT&W", true);                 // сохранить профиль

    //sendSMS("+79915760104", "success1");
    //sendSMS("+79915760104", "success2");
    //sendSMS("+79915760104", "success3");

    // Буфер для строк от SIM (URC: +CMTI, RING и т.п.)
    char sim_line[RESP_BUF_SIZE];
    uint16_t sim_pos = 0;

    // Буфер для GPS
    uint16_t gps_pos = 0;

    uint16_t tick = 0;

    for (;;)
    {
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



/*

        // ---------- SIM800: читаем построчно ----------
        if (uart_available(SIM_UART))
        {
            char c = USART_Receive(SIM_UART);
            USART_Transmitchar(c, COMP_UART);
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
*/
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
