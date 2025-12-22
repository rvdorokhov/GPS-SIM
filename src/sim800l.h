#ifndef SIM800_H
#define SIM800_H

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "utils.h"


#define SIM_UART      1   // USART1 -> SIM800
#define RESP_BUF_SIZE 256
static char sim_resp[RESP_BUF_SIZE];

// Отправка AT-команды в SIM800 (без \r)
// Если waiting = true, то ждёт ответа в буфере sim_resp
char *sendATCommand(const char *cmd, bool waiting);

// Отправка строки в модем (с добавлением \r в конце)
void sim_send_line(const char *s);

char *sim_wait_response(void);

// Отправка SMS на указанный номер с заданным текстом
void sendSMS(const char *phone, const char *message);

bool sim_sms_has_unread(void);

// Достаёт ТЕЛО первого непрочитанного SMS от нужного номера.
// Возвращает true если нашло и положило текст в out_body.
// (Сообщение после успешного чтения удаляется, чтобы не ловить его снова.)
bool sim_sms_get_unread_body_from(const char *wanted_phone,
                                 char *out_body,
                                 size_t out_body_sz);

#endif
