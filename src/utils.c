#include "utils.h"

#include <string.h>
#include <stddef.h>

void str_trim(char *s)
{
    // обрезаем пробелы/CR/LF в начале и в конце
    char *p = s;
    while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')
        p++;

    if (p != s)
        memmove(s, p, strlen(p) + 1);

    size_t len = strlen(s);
    while (len > 0 &&
           (s[len - 1] == ' ' ||
            s[len - 1] == '\r' ||
            s[len - 1] == '\n' ||
            s[len - 1] == '\t'))
    {
        s[len - 1] = '\0';
        len--;
    }
}

bool str_starts_with(const char *s, const char *prefix)
{
    size_t lp = strlen(prefix);
    return strncmp(s, prefix, lp) == 0;
}

