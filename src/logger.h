#ifndef LOGGER_H
#define LOGGER_H

int  log_init(const char *path);
void log_event(const char *level, const char *fmt, ...);
void log_close(void);

#endif
