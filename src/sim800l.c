#include "uart.h"
#include "sim800l.h"


#define COMP_UART      0   // USART1 -> GPS
// 
// ---------------------------------------------------
// Отправка строк в SIM
// ---------------------------------------------------
void sim_send_line(const char *s)
{
    USART_TransmitString((char *)s, SIM_UART);
    USART_Transmitchar('\r', SIM_UART);  
    
    //USART_TransmitString("SEND SIM LINE ", COMP_UART);
   // USART_TransmitString((char *)s, COMP_UART);
    //USART_TransmitString("\n", COMP_UART);
}

// ---------------------------------------------------
// Ожидание ответа от SIM
// читаем до "тишины" ~100 мс или максимум буфера
// ---------------------------------------------------
char *sim_wait_response(void)
{
    sim_resp[0] = '\0';
    //USART_TransmitString("SIM WAIT RESPONSE ", COMP_UART);
    //USART_TransmitString("\n", COMP_UART);

    memset(sim_resp, 0, sizeof(sim_resp));
    uint16_t pos = 0;

    uint16_t idle_ms = 0;
    const uint16_t MAX_IDLE_MS   = 1000;   // можно 200-500мс
    const uint16_t MAX_TOTAL_MS  = 10000;

    uint16_t total_ms = 0;

    while (total_ms < MAX_TOTAL_MS)
    {
        if (uart_available(SIM_UART))           
        {
            char c = USART_Receive(SIM_UART);      // блокирующий ОК, т.к. байт уже есть

           // USART_TransmitString("GOT FROM SIM ", COMP_UART);
            //USART_TransmitString(sim_resp, COMP_UART);
            //USART_TransmitString("\n", COMP_UART);

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

            if (idle_ms >= MAX_IDLE_MS) {
                //USART_TransmitString("TOTAL BREAK ", COMP_UART);
                //USART_TransmitString("\n", COMP_UART);
                break;
            }    
        }
    }

    USART_TransmitString("SIM RESP ", COMP_UART);
    USART_TransmitString(sim_resp, COMP_UART);
    USART_TransmitString("\n", COMP_UART);

    return sim_resp;
}


// ---------------------------------------------------
// sendATCommand (cmd без CR в конце)
// если waiting = true – ждём ответ в sim_resp
// ---------------------------------------------------
char *sendATCommand(const char *cmd, bool waiting)
{
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
        }
            */

      //  USART_TransmitString("got response:", COMP_UART);
      //  USART_TransmitString(resp, COMP_UART);
        //USART_TransmitString("GOT SIM RESP SEND AT ", COMP_UART);
        //USART_TransmitString(resp, COMP_UART);
        //USART_TransmitString("\n", COMP_UART);

        return resp;
    }

    return sim_resp;
}

// ---------------------------------------------------
// Отправка SMS
// ---------------------------------------------------
void sendSMS(const char *phone, const char *message)
{
    //USART_TransmitString("SEND SMS ", COMP_UART);
    //USART_TransmitString(message, COMP_UART);
    //USART_TransmitString("\n", COMP_UART);

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", phone);

    sendATCommand(cmd, true);          // перешли в режим ввода текста

    // текст + Ctrl+Z
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s\r\n%c", message, (char)26);

    sendATCommand(tmp, true);

  //  USART_TransmitString("SMS SENDED ", COMP_UART);
   // USART_TransmitString(cmd, COMP_UART);
    //USART_TransmitString("\n", COMP_UART);
    //USART_TransmitString(tmp, COMP_UART);
    //USART_TransmitString("\n", COMP_UART);
}

// ---------------------------------------------------
// Парсинг SMS 
// ---------------------------------------------------
void handleSMS(const char *msg)
{
    
   // USART_TransmitString("HANDLE SMS 1", COMP_UART);
    //USART_TransmitString("\n", COMP_UART);

    //sendSMS("+79915760104", "handle sms: ");
    //sendSMS("+79915760104", msg);
    // Ищем "+CMGR: "
    const char *cmgr = strstr(msg, "+CMGR: ");
    if (!cmgr)
        return;

//USART_TransmitString("HANDLE SMS 2", COMP_UART);
  //  USART_TransmitString("\n", COMP_UART);

    // Заголовок до первой \r
    const char *cr = strstr(cmgr, "\r");
    if (!cr)
        return;

  //      USART_TransmitString("HANDLE SMS 3", COMP_UART);
   // USART_TransmitString("\n", COMP_UART);

    size_t header_len = cr - cmgr;
    char header[128];
    if (header_len >= sizeof(header))
        header_len = sizeof(header) - 1;
    memcpy(header, cmgr, header_len);
    header[header_len] = '\0';

//USART_TransmitString("HANDLE SMS 4", COMP_UART);
  //  USART_TransmitString("\n", COMP_UART);

    // Тело SMS начинается через два символа после \r: то есть после "\r\n"
    const char *body_start = cr + 2;
    const char *ok_pos = strstr(body_start, "OK");
    if (!ok_pos)
        return;

//USART_TransmitString("HANDLE SMS 5", COMP_UART);
  //  USART_TransmitString("\n", COMP_UART);

    // Копируем тело SMS в буфер body[]
    size_t body_len = ok_pos - body_start;
    char body[160];
    if (body_len >= sizeof(body))
        body_len = sizeof(body) - 1;
    memcpy(body, body_start, body_len);
    body[body_len] = '\0';
    str_trim(body);

//USART_TransmitString("HANDLE SMS 6", COMP_UART);
  //  USART_TransmitString("\n", COMP_UART);

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

    USART_TransmitString("PPP", COMP_UART);

    sendSMS("+79915760104", body);
    
    USART_TransmitString("HANDLE SMS SUCCESSFULL ", COMP_UART);
    USART_TransmitString(body, COMP_UART);
    USART_TransmitString("\n", COMP_UART);
}

// --- внутренний парсер ответа AT+CMGR=... ---
static bool parse_cmgr_phone_and_body(const char *resp,
                                      char *out_phone, size_t out_phone_sz,
                                      char *out_body,  size_t out_body_sz)
{
    const char *cmgr = strstr(resp, "+CMGR:");
    if (!cmgr) return false;

    const char *crlf = strstr(cmgr, "\r\n");
    if (!crlf) return false;

    // Парсим номер (3-я пара кавычек в заголовке)
    // +CMGR: "REC UNREAD","+7999....",,"..."
    const char *q1 = strchr(cmgr, '"');
    if (!q1) return false;
    const char *q2 = strchr(q1 + 1, '"');
    if (!q2) return false;
    const char *q3 = strchr(q2 + 1, '"');
    if (!q3) return false;
    const char *q4 = strchr(q3 + 1, '"');
    if (!q4) return false;

    size_t phone_len = (size_t)(q4 - (q3 + 1));
    if (phone_len >= out_phone_sz) phone_len = out_phone_sz - 1;
    memcpy(out_phone, q3 + 1, phone_len);
    out_phone[phone_len] = '\0';
    str_trim(out_phone);

    // Тело SMS идёт со следующей строки после заголовка
    const char *body_start = crlf + 2;

    // Ищем конец тела перед OK (стараемся искать "CRLF OK", чтобы не поймать "OK" в тексте)
    const char *ok = strstr(body_start, "\r\nOK");
    if (!ok) ok = strstr(body_start, "\nOK");
    if (!ok) ok = strstr(body_start, "OK");
    if (!ok) return false;

    size_t body_len = (size_t)(ok - body_start);
    if (body_len >= out_body_sz) body_len = out_body_sz - 1;
    memcpy(out_body, body_start, body_len);
    out_body[body_len] = '\0';
    str_trim(out_body);

    return true;
}

// --- 2) Если есть непрочитанные, и они от wanted_phone — достать тело ---
bool sim_sms_get_unread_body_from(const char *wanted_phone,
                                 char *out_body,
                                 size_t out_body_sz)
{
    if (!wanted_phone || !out_body || out_body_sz == 0) return false;
    out_body[0] = '\0';

    char *resp = sendATCommand("AT+CMGL=\"REC UNREAD\"", true);
    if (!resp) return false;

    const char *p = resp;

    while ((p = strstr(p, "+CMGL:")) != NULL)
    {
        // индекс сообщения сразу после "+CMGL:"
        p += 6;
        while (*p == ' ') p++;

        uint16_t idx = (uint16_t)strtoul(p, NULL, 10);
        if (idx == 0) { continue; }

        // Чтобы не читать лишнее: попробуем вытащить телефон прямо из строки CMGL
        // Формат: +CMGL: <idx>,"REC UNREAD","<phone>",...
        // Возьмём текущую строку до CRLF
        const char *line_end = strstr(p, "\r\n");
        if (!line_end) line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);

        char header[200];
        size_t hl = (size_t)(line_end - (p - 6));
        if (hl >= sizeof(header)) hl = sizeof(header) - 1;
        memcpy(header, p - 6, hl);
        header[hl] = '\0';

        // достаём телефон из header (3-я пара кавычек)
        char phone[32] = {0};
        const char *q1 = strchr(header, '"');
        const char *q2 = q1 ? strchr(q1 + 1, '"') : NULL;
        const char *q3 = q2 ? strchr(q2 + 1, '"') : NULL;
        const char *q4 = q3 ? strchr(q3 + 1, '"') : NULL;

        if (q3 && q4)
        {
            size_t pl = (size_t)(q4 - (q3 + 1));
            if (pl >= sizeof(phone)) pl = sizeof(phone) - 1;
            memcpy(phone, q3 + 1, pl);
            phone[pl] = '\0';
            str_trim(phone);

            if (strcmp(phone, wanted_phone) == 0)
            {
                // читаем конкретно это SMS
                char cmd[24];
                snprintf(cmd, sizeof(cmd), "AT+CMGR=%u", idx);
                char *cmgr = sendATCommand(cmd, true);

                char parsed_phone[32];
                if (parse_cmgr_phone_and_body(cmgr,
                                              parsed_phone, sizeof(parsed_phone),
                                              out_body, out_body_sz))
                {
                    // на всякий случай перепроверим номер
                    if (strcmp(parsed_phone, wanted_phone) == 0)
                    {
                        // удалим, чтобы не ловить повторно
                        snprintf(cmd, sizeof(cmd), "AT+CMGD=%u,0", idx);
                        sendATCommand(cmd, true);
                        return true;
                    }
                }
            }
        }

        p = line_end; // двигаемся дальше
    }

    return false;
}
