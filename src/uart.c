#include "uart.h"

/*
 * USART_Init
 * Инициализация UART0 или UART1
 * baud — значение UBRR (а не скорость)
 * n = 1 → использовать USART1
 * n = 0 → использовать USART0
 */
void USART_Init( unsigned int baud, uint8_t n )
{
    sei(); // Разрешаем глобальные прерывания (на всякий случай)

    if(n) // Если выбран USART1
    {
        // Установка скорости передачи (UBRR1 = baud)
        UBRR1H = (unsigned char)(baud >> 8);
        UBRR1L = (unsigned char)baud;

        // Включение приемника и передатчика USART1
        UCSR1B |= (1 << RXEN1) | (1 << TXEN1);

        // Формат кадра: 8 бит данных, без паритета
        // (3 << UCSZ10) устанавливает UCSZ11=1 и UCSZ10=1 → размер символа 8 бит
        UCSR1C |= (3 << UCSZ10);
    }
    else // Конфигурация USART0
    {
        // Установка скорости передачи (UBRR0 = baud)
        UBRR0H = (unsigned char)(baud >> 8);
        UBRR0L = (unsigned char)baud;

        // Включение приемника и передатчика USART0
        UCSR0B |= (1 << RXEN0) | (1 << TXEN0);

        // Формат кадра: 8 бит данных, без паритета
        UCSR0C |= (3 << UCSZ00);
    }
}



/*
 * USART_InterruptEnable
 * Включает прерывание по приёму RXC
 */
void USART_InterruptEnable(uint8_t n)
{
    if(n) // USART1
    {
        UCSR1B |= (1 << RXCIE1); // Разрешить прерывание при приёме данных
    }
    else // USART0
    {
        UCSR0B |= (1 << RXCIE0);
    }
}



/*
 * USART_Transmitchar
 * Передаёт один байт по UART (блокирующая передача)
 */
void USART_Transmitchar(unsigned char data, uint8_t n)
{
    if(n) // USART1
    {
        // Ждать, пока буфер передачи освободится (UDRE1 = 1)
        while (!(UCSR1A & (1 << UDRE1)));
        UDR1 = data; // Записать байт в регистр передачи
    }
    else // USART0
    {
        while (!(UCSR0A & (1 << UDRE0)));
        UDR0 = data;
    }
}



/*
 * USART_Receive
 * Блокирующий приём одного байта
 */
unsigned char USART_Receive(uint8_t n)
{
    if(n) // USART1
    {
        // Ждать появления нового принятого байта (RXC1)
        while (!(UCSR1A & (1 << RXC1)));
        return UDR1; // Вернуть принятый байт
    }
    else // USART0
    {
        while (!(UCSR0A & (1 << RXC0)));
        return UDR0;
    }
}



/*
 * USART_TransmitString
 * Передаёт строку (заканчивается символом '\0')
 */
void USART_TransmitString(char *str, uint8_t n)
{
    while(*str > 0) // Пока не достигнут нулевой терминатор
    {
        USART_Transmitchar(*str, n); // Отправить символ
        str++; // Перейти к следующему
    }
}



/*
 * USART_TransmitNumber
 * Передаёт целое число в виде ASCII-символов
 * Использует рекурсию для вывода цифр слева направо
 */
void USART_TransmitNumber(long int num, uint8_t n)
{
    if(num < 0) // Если число отрицательное
    {
        USART_Transmitchar('-', n); // Печатаем '-'
        num = -num; // Делаем число положительным
    }

    if(num >= 10) // Если больше 9 — рекурсивно выводим старшие цифры
    {
        USART_TransmitNumber(num / 10, n); // Вывод старших разрядов
        num = num % 10;                     // Младшая цифра
    }

    // Печатаем последнюю цифру как ASCII символ
    USART_Transmitchar(num + '0', n);
}



/* --- Устаревшая функция (закомментирована)
 * Передавала число в бинарном виде из 8 бит
 */
/*
void USART0_TransmitBinary(int num)
{
    int i = 0, j = 0;

    while(num)
    {
        USART0_TransmitNumber(num % 2);
        i++;
        num = num / 2;
    }

    if(i != 8)
    {
        for(j = 0; j < (8 - i); j++)
            USART0_TransmitNumber(0);
    }
}
*/

