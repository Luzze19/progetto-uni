#ifndef LOG_H
#define LOG_H

#include <threads.h>

extern mtx_t mutex_log;

void log_event(long timestamp, const char* id, const char* event, const char* message, ...);


#endif