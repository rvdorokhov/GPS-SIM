#ifndef STR_UTILS_H
#define STR_UTILS_H

#include <stdbool.h>

// Убирает пробелы/таб/CR/LF в начале и в конце строки (in-place)
void str_trim(char *s);

// true, если строка s начинается с prefix
bool str_starts_with(const char *s, const char *prefix);

#endif // STR_UTILS_H
