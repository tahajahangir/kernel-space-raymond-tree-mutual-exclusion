#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <machine/stdarg.h>
//#include <stdarg.h>
#include "log.h"

int currentLogLevel = 1;

char *log_level_str[5] = { "NOTSET", "DEBUG", "INFO", "WARN", "ERROR" };

void mylog(int level, char* text, ...) {
	if (level >= currentLogLevel) {
		va_list args;
		va_start( args, text);
		printf("%s", log_level_str[level]);
		printf(":\t");
		vprintf(text, args);
		va_end( args);
		printf("\n");
	}
}

