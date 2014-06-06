// License: please see LICENSE2 file for more details.
#include <stdio.h>
#include <stdarg.h>
#include <time.h>


void println(const char* file, const char* function, const int line, const char *str)
{
	time_t timer;
    time(&timer);  /* get current time; same as: timer = time(NULL)  */
	fprintf(stdout, "[%s] (%s:%d) %s\n", function, file, line, str);
}

void println_var(const char* file, const char* function, const int line, const char* format, ...)
{
	char status[512];
	va_list arglist;

	va_start(arglist, format);
	vsprintf(status, format, arglist);
	va_end(arglist);

	println(file, function, line, status);
}

void println_start(const char* file, const char* function, const int line)
{
	println(file, function, line, "start");
}

void println_end(const char* file, const char* function, const int line)
{
	println(file, function, line, "end");
}

