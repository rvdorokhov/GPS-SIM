#include "uart.h"
#include "gps.h"

static bool started = false;
extern bool gps_status;
extern const char USER_PHONE[];

// ---------------------------------------------------
// GPS: конвертация NMEA координат в десятичные
// coord: "ddmm.mmmm" или "dddmm.mmmm"
// hemisphere: "N","S","E","W"
// ---------------------------------------------------
double nmeaToDecimal(const char *coord, char hemisphere)
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
void processResponseGPS(const char *sentence1)
{
   // 55.765790, 37.685530
   // общаги 55.772234, 37.695887
    // в градусы*1e7:
    int32_t lat_e7 = 557657900L;
    int32_t lon_e7 = 376855300L;

    // Разброс ±100 м:
    // 1 м по широте ~ 1e7/111320 ≈ 90
    // 1 м по долготе ~ 1e7/(111320*cos(lat)) ≈ 160 для ~56°
    int32_t dlat = ((int32_t)(rand() % 201) - 100) * 90L;
    int32_t dlon = ((int32_t)(rand() % 201) - 100) * 160L;

    lat_e7 += dlat;
    lon_e7 += dlon;

    // ---------- convert e7 -> ddmm.mmmm / dddmm.mmmm ----------
    char ns = 'N', ew = 'E';
    if (lat_e7 < 0) { ns = 'S'; lat_e7 = -lat_e7; }
    if (lon_e7 < 0) { ew = 'W'; lon_e7 = -lon_e7; }

    // LAT
    int32_t lat_deg = lat_e7 / 10000000L;
    int32_t lat_rem = lat_e7 - lat_deg * 10000000L;

    int64_t lat_min_e4 = (int64_t)lat_rem * 60LL * 10000LL / 10000000LL;  // minutes * 1e4
    int32_t lat_min_i  = (int32_t)(lat_min_e4 / 10000LL);
    int32_t lat_min_f  = (int32_t)(lat_min_e4 % 10000LL);

    char lat_str[16];
    snprintf(lat_str, sizeof(lat_str), "%02ld%02ld.%04ld",
             (long)lat_deg, (long)lat_min_i, (long)lat_min_f);

    // LON
    int32_t lon_deg = lon_e7 / 10000000L;
    int32_t lon_rem = lon_e7 - lon_deg * 10000000L;

    int64_t lon_min_e4 = (int64_t)lon_rem * 60LL * 10000LL / 10000000LL;
    int32_t lon_min_i  = (int32_t)(lon_min_e4 / 10000LL);
    int32_t lon_min_f  = (int32_t)(lon_min_e4 % 10000LL);

    char lon_str[16];
    snprintf(lon_str, sizeof(lon_str), "%03ld%02ld.%04ld",
             (long)lon_deg, (long)lon_min_i, (long)lon_min_f);

    // ---------- build RMC with placeholders ----------
    // Тебе важны только поля 2..6: status=A, lat, N/S, lon, E/W
    char tmp[128];
    snprintf(tmp, sizeof(tmp),
             "$GPRMC,120000.00,A,%s,%c,%s,%c,0.0,0.0,191225,,,A*",
             lat_str, ns, lon_str, ew);

    // ---------- checksum ----------
    uint8_t cs = 0;
    for (const char *p = tmp + 1; *p && *p != '*'; p++) cs ^= (uint8_t)(*p);

    char sentence[128];
    snprintf(sentence, sizeof(sentence), "%s%02X", tmp, cs);

    // ---------- feed your parser ----------
    sendGPS(sentence);
}

void sendGPS(const char *sentence)
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
        return;
    }

    double lat = nmeaToDecimal(latStr, latHemStr[0]);
    double lon = nmeaToDecimal(lonStr, lonHemStr[0]);

    char lat_buf[16], lon_buf[16];
    dtostrf(lat, 2, 6, lat_buf);
    dtostrf(lon, 3, 6, lon_buf);

    char msg[96];
    snprintf(msg, sizeof(msg),
             "%s,%s \n https://yandex.ru/maps/?text=%s,%s", lat_buf, lon_buf, lat_buf, lon_buf);

    sendSMS(USER_PHONE, msg);
}

// Читает NMEA-поток с UART и возвращает true, когда собрана одна строка (например $GPRMC...)
// uart_n: 0 или 1 (USART0/USART1)
// out: куда сложить строку
// outsz: размер буфера out
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
extern uint32_t millis(void);

bool gps_read_nmea_line(char *out, size_t outsz)
{
    /*
    static uint32_t t0 = 0;

    if (!started) {
        t0 = millis();
        started = true;
    }

    // первые 15 секунд — "нет фикса/нет данных"
    if ((uint32_t)(millis() - t0) < 15000UL) {
        return false;
    } */

    // после 15 секунд — "пошли данные"
    const char *s = "$GPRMC,120000.00,A,5545.0960,N,03741.2980,E,0.0,0.0,191225,,,A*5D";

    if (!out || outsz < 2) return false;

    strncpy(out, s, outsz - 1);
    out[outsz - 1] = '\0';
    return true;
}
