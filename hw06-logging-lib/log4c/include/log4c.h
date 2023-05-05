#ifndef _LOG4C_H_
#define _LOG4C_H_

#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#if defined(__GNUC__) && defined(G_CXX_STD_VERSION)
#define LOG4C_FUNCSTR ((const char *)(__PRETTY_FUNCTION__))
#else
#define LOG4C_FUNCSTR ((const char *)(__func__))
#endif

#define LOG4C_STRINGIFY(macro_or_string) LOG4C_STRINGIFY_ARG(macro_or_string)
#define LOG4C_STRINGIFY_ARG(contents) #contents

#define LOG4C_ERROR_ALREADY_INITIALIZED -1
#define LOG4C_ERROR_NOT_INITIALIZED -2
#define LOG4C_MEMORY_ERROR -3
#define LOG4C_CONFIG_ERROR -4
#define LOG4C_INIT_ERROR -5
#define LOG4C_IO_ERROR -6

#define LOG4C_DEFAULT_MESSAGE_FORMAT "%t %l14 %p %f30 %c25 %m"

typedef enum { DEBUG, INFO, WARNING, ERROR } log4c_level;

#define LOG4C_APPENDER_TYPE_STDOUT 1
#define LOG4C_APPENDER_TYPE_FILE 2

typedef struct {
  struct timeval ts;
  log4c_level level;
  const char *file;
  const char *line;
  const char *function;
  const char *message_string;
  bool print_stacktrace;
} log4c_message_type;

typedef struct log4c_appeder_cfg_struct {
  unsigned short type;
  log4c_level threshold;
  char *message_format;
  int (*open)(struct log4c_appeder_cfg_struct *);
  int (*write)(const struct log4c_appeder_cfg_struct *,
               const log4c_message_type *);
  int (*release)(struct log4c_appeder_cfg_struct *);
} log4c_appeder_cfg_struct;

typedef struct {
  log4c_appeder_cfg_struct base;
  bool error_to_stderr;
} log4c_stdout_appender_cfg_struct;

typedef struct {
  log4c_appeder_cfg_struct base;
  char *file_name;
  FILE *file_handle;
} log4c_file_appender_cfg_struct;

#define LOG4C_DEBUG(...)                                                       \
  log4c_log(DEBUG, __FILE__, LOG4C_STRINGIFY(__LINE__), LOG4C_FUNCSTR,         \
            __VA_ARGS__)
#define LOG4C_INFO(...)                                                        \
  log4c_log(INFO, __FILE__, LOG4C_STRINGIFY(__LINE__), LOG4C_FUNCSTR,          \
            __VA_ARGS__)
#define LOG4C_WARNING(...)                                                     \
  log4c_log(WARNING, __FILE__, LOG4C_STRINGIFY(__LINE__), LOG4C_FUNCSTR,       \
            __VA_ARGS__)
#define LOG4C_ERROR(...)                                                       \
  log4c_log(ERROR, __FILE__, LOG4C_STRINGIFY(__LINE__), LOG4C_FUNCSTR,         \
            __VA_ARGS__)

void log4c_log(log4c_level level, const char *file, const char *line,
               const char *func, const char *message_format, ...);

int log4c_setup_cmdline(int argc, char **argv);
int log4c_setup_appenders(log4c_appeder_cfg_struct *appender_list[],
                          size_t appender_list_size);
int log4c_finalize(void);
void log4c_set_level_color(bool);

#endif
