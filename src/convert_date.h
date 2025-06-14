#include <time.h>
#include <string>

struct DateTime {
    tm datetime;
    int frac_sec;
};

void display(tm &time, int frac_sec);
void display(DateTime &datetime);

enum Term {
    none,
    year,
    mon,
    day,
    timesep,
    hour,
    min,
    secs,
    fracsecs,
    zonesep,
    zonehr,
    zhmsep,
    zonemin
};

Term parse_8601_datetime(const std::string &input, DateTime &result);
