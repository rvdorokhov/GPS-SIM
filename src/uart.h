#ifndef USART_128
#define USART_128

#include <avr/io.h>
#include <avr/interrupt.h>
// #define F_CPU 16000000UL
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define New_Line(n) USART_Transmitchar(0x0A,n);\
USART_Transmitchar(0x0D,n);
#define Space(n,x) for(int i=0;i<=n;i++)USART_Transmitchar(0x20,x);

void USART_Init(unsigned int, uint8_t);
void USART_InterruptEnable(uint8_t );
void USART_Transmitchar( unsigned char, uint8_t);
unsigned char USART_Receive(uint8_t);
void USART_TransmitString(char *str, uint8_t);
void USART_TransmitNumber(long int, uint8_t);
uint8_t uart_available(uint8_t n);

#endif