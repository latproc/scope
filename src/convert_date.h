#include <time.h>
#include <string>
#include <ostream>

struct DateTime {
	tm datetime;
	int frac_sec;
};

void display(tm &time, int frac_sec);
void display(DateTime &datetime);

enum Term {
	none,
	year,
    monsep,
	mon,
    daysep,
	day,
	timesep,
	hour,
    minsep,
	min,
    secsep,
	secs,
	fracsecs,
	zonesep,
	zonehr,
	zhmsep,
	zonemin
};

std::ostream &operator<<(std::ostream &out, const DateTime &datetime);
std::ostream & operator<<(std::ostream &out, const Term &term);

Term parse_8601_datetime(const std::string &input, DateTime &result);
