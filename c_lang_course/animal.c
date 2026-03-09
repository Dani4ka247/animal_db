#include "animal.h"
#include <stdio.h>
#include <string.h>

static int is_leap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

int parse_date_ddmmyyyy(const char *s, struct tm *out) {
    int d=0, m=0, y=0;
    if (sscanf(s, "%d.%d.%d", &d, &m, &y) != 3) return -1;
    if (y < 1900 || y > 3000) return -1;
    if (m < 1 || m > 12) return -1;
    if (d < 1) return -1;

    int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && is_leap(y)) dim[1] = 29;
    if (d > dim[m - 1]) return -1;

    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_mday = d;
    t.tm_mon  = m - 1;
    t.tm_year = y - 1900;
    t.tm_isdst = -1;

    if (mktime(&t) == (time_t)-1) return -1;
    *out = t;
    return 0;
}

int format_date_ddmmyyyy(const struct tm *t, char *buf, size_t bufsize) {
    return (strftime(buf, bufsize, "%d.%m.%Y", t) > 0) ? 0 : -1;
}
