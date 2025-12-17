#include "uart.h"
#include "gps.h"

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
void processResponseGPS(const char *sentence)
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
             "[Yandex] Koordinaty: %s,%s", lat_buf, lon_buf);

    sendSMS("+79915760104", msg);
}