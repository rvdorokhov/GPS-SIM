#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim800l.h"
#include "utils.h"

#define GPS_UART      0   // USART1 -> GPS
#define GPS_BUF_SIZE   96
static char gps_line[GPS_BUF_SIZE];

// Обработка одной строки NMEA из GPS-модуля
void processResponseGPS(const char *sentence);

// Преобразование координат NMEA (ddmm.mmmm) в десятичный формат
double nmeaToDecimal(const char *coord, char hemisphere);

char *dtostrf(double val, signed char width, unsigned char prec, char *s);

bool gps_read_nmea_line(char *out, size_t outsz);

void sendGPS(const char *sentence);

#endif