#include "convert_date.h"
#include <exception>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <time.h>

void display(tm &time, int frac_sec)
{
	time_t local_time = timegm(&time);
	localtime_r(&local_time, &time);

	std::cout << (time.tm_year + 1900) << "-" << (time.tm_mon + 1) << "-"
	        << time.tm_mday << " " << time.tm_hour << ":" << time.tm_min << ":"
	        << time.tm_sec;
	if (frac_sec) {
		std::cout << '.' << frac_sec;
	}
	std::cout << std::endl;
}

void display(DateTime &datetime)
{
	display(datetime.datetime, datetime.frac_sec);
}

Term parse_8601_datetime(const std::string &input, DateTime &result)
{
	try {
		tm time = {.tm_year = 0,
		           .tm_mon = 0,
		           .tm_mday = 0,
		           .tm_hour = 0,
		           .tm_min = 0,
		           .tm_sec = 0
		   };
		int frac_sec = 0;
		const char *p = input.c_str();
		const char *q = p;
		auto append_to = [](int &field, const char *p) {
			field = field * 10 + *p - '0';
		};
		Term state = year;
		Term error = none;
		while (*p) {
			switch (state) {
				case none:
					break;
				case year:
					if (!isdigit(*p)) {
						error = state;
						break;
					}
					append_to(time.tm_year, p);
					if (p - q == 3) {
						q = p;
						state = mon;
					}
					break;
				case mon:
					if (!isdigit(*p)) {
						error = state;
						break;
					}
					append_to(time.tm_mon, p);
					if (p - q == 2) {
						q = p;
						state = day;
					}
					break;
				case day:
					if (!isdigit(*p)) {
						error = state;
						break;
					}
					append_to(time.tm_mday, p);
					if (p - q == 2) {
						q = p;
						state = timesep;
					}
					break;
				case timesep:
					q = p;
					state = hour;
					break;
				case hour:
					if (!isdigit(*p)) {
						error = state;
						break;
					}
					append_to(time.tm_hour, p);
					if (p - q == 2) {
						q = p;
						state = min;
					}
					break;
				case min:
					if (!isdigit(*p)) {
						error = state;
						break;
					}
					append_to(time.tm_min, p);
					if (p - q == 2) {
						q = p;
						state = secs;
					}
					break;
				case secs:
					if (!isdigit(*p)) {
						if (*p == '.') {
							state = fracsecs;
							break;
						}
						else if (*p == 'Z') {
							state = zonehr;
							break;
						}
						else if (isdigit(*p) && p - q > 2) {
							error = state;
							break;
						}
						else {
							state = none;
							break;
						}
					}
					append_to(time.tm_sec, p);
					break;
				case fracsecs:
					if (!isdigit(*p)) {
						if (*p == 'Z') {
							state = zonehr;
							break;
						}
						else {
							state = none;
							break;
						}
					}
					append_to(frac_sec, p);
					break;
				case zonesep:
					break;
				case zonehr:
					state = none;
					break; // TODO
					break;
				case zhmsep:
					break;
				case zonemin:
					break;
			}
			if (state == none || error != none) {
				break;
			}
			++p;
		};
		if (error != none) {
			return error;
		}
		else if (!*p || state == none) {
			time.tm_year -= 1900;
			time.tm_mon -= 1;
			result.datetime = time;
			result.frac_sec = frac_sec;
		}
	}
	catch (const std::exception &e) {
		std::cout << "Error: " << e.what() << std::endl;
	}
	return none;
}

#ifdef TESTING
int main(int argc, char *argv[])
{

	for (int i = 1; i < argc; ++i) {
		DateTime dt;
		if (parse_8601_datetime(argv[i], dt) == none) {
			display(dt);
		}
		else {
			std::cerr << "Unexpected datetime format\n";
		}
	}

	return EXIT_SUCCESS;
}
#endif
