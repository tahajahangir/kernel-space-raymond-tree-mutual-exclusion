#ifndef	_MY_LOG_H
#define	_MY_LOG_H	1

/**
 * IMPORTANT NOTE: None of log_* functions changes the errno!
 */

#define LOG_DEBUG 1
#define LOG_INFO 2
#define LOG_WARNING 3
#define LOG_WARN 3
#define LOG_ERROR 4

void mylog(int level, char* text, ...);

#define log_debug(format, ...) mylog(LOG_DEBUG, format,  ##__VA_ARGS__)
#define log_info(format, ...) mylog(LOG_INFO, format,  ##__VA_ARGS__)
#define log_warn(format, ...) mylog(LOG_WARN, format,  ##__VA_ARGS__)
#define log_error(format, ...) mylog(LOG_ERROR, format,  ##__VA_ARGS__)

#endif /* my/log.h */
