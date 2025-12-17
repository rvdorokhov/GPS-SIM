#include "uart.h"
#include "sim800l.h"
// 
// ---------------------------------------------------
// Отправка строк в SIM
// ---------------------------------------------------
void sim_send_line(const char *s)
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
char *sim_wait_response(void)
{
    memset(sim_resp, 0, sizeof(sim_resp));
    uint16_t pos = 0;

    uint16_t idle_ms = 0;
    const uint16_t MAX_IDLE_MS   = 200;   // можно 200-500мс
    const uint16_t MAX_TOTAL_MS  = 10000;

    uint16_t total_ms = 0;

    while (total_ms < MAX_TOTAL_MS)
    {
        if (uart_available(SIM_UART))              // <-- ключевое
        {
            char c = USART_Receive(SIM_UART);      // блокирующий ОК, т.к. байт уже есть
            idle_ms = 0;

            if (pos < (RESP_BUF_SIZE - 1))
            {
                sim_resp[pos++] = c;
                sim_resp[pos] = '\0';
            }
        }
        else
        {
            _delay_ms(1);
            idle_ms++;
            total_ms++;

            if (idle_ms >= MAX_IDLE_MS)
                break;
        }
    }

    return sim_resp;
}


// ---------------------------------------------------
// sendATCommand (cmd без CR в конце)
// если waiting = true – ждём ответ в sim_resp
// ---------------------------------------------------
char *sendATCommand(const char *cmd, bool waiting)
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
void sendSMS(const char *phone, const char *message)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", phone);

    sendATCommand(cmd, true);          // перешли в режим ввода текста

    // текст + Ctrl+Z
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s\r\n%c", message, 26);

    sendATCommand(tmp, true);
}

// ---------------------------------------------------
// Парсинг SMS 
// ---------------------------------------------------
void handleSMS(const char *msg)
{
    //sendSMS("+79915760104", "handle sms: ");
    //sendSMS("+79915760104", msg);
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

    char reply[64];

    sendSMS("+79915760104", reply);
    // Можно было бы на phone, если отвечать отправителю.
}