#include "log4c.h"
#include <bits/pthreadtypes.h>
#include <ctype.h>
#include <malloc.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#define LOG4C_DEBUG_STR_C                                                      \
  ("\033[35m"                                                                  \
   "DEBUG"                                                                     \
   "\033[0m")
#define LOG4C_INFO_STR_C                                                       \
  ("\033[32m"                                                                  \
   "INFO"                                                                      \
   "\033[0m")
#define LOG4C_WARNING_STR_C                                                    \
  ("\033[33m"                                                                  \
   "WARN"                                                                      \
   "\033[0m")
#define LOG4C_ERROR_STR_C                                                      \
  ("\033[31m"                                                                  \
   "ERROR"                                                                     \
   "\033[0m")

#define LOG4C_DEBUG_STR "DEBUG"
#define LOG4C_INFO_STR "INFO"
#define LOG4C_WARNING_STR "WARN"
#define LOG4C_ERROR_STR "ERROR"

#define LOG4C_DEFAULT_THRES_LEVEL DEBUG

#define MAX_APPENDER_COUNT 16
#define MAX_MESSAGE_SIZE 1024

#define DROP_BACKTRACE_FRAMES_COUNT 3

#define LOG4C_CMD_LINE_PREFIX "-Dlog4c.appender"

#define LOG4C_CMD_TYPE_LITERAL "type"
#define LOG4C_CMD_FORMAT_LITERAL "format"
#define LOG4C_CMD_THRESHOLD_LITERAL "threshold"
#define LOG4C_CMD_FILENAME_LITERAL "filename"
#define LOG4C_CMD_ERROR_TO_STDERR_LITERAL "error_to_stderr"

#define LOG4C_CMDLINE_TYPE_HASH 0
#define LOG4C_CMDLINE_FORMAT_HASH 1
#define LOG4C_CMDLINE_THRESHOLD_HASH 2
#define LOG4C_CMDLINE_FILENAME_HASH 3
#define LOG4C_CMDLINE_ERROR_TO_STDERR_HASH 4

static log4c_appeder_cfg_struct *appenders[MAX_APPENDER_COUNT];

static volatile bool initialized = false;
static volatile atomic_int init_error = false;

static volatile bool use_colored_level = false;

pthread_mutex_t pthread_lock = PTHREAD_MUTEX_INITIALIZER;

void backtrace(FILE *outstream) {
  unw_cursor_t cursor;
  unw_context_t context;

  if (unw_getcontext(&context) < 0) {
    fprintf(stderr, "ERROR: cannot get local machine state\n");
    return;
  }
  if (unw_init_local(&cursor, &context) < 0) {
    fprintf(stderr, "ERROR: cannot initialize cursor for local unwinding\n");
    return;
  }

  size_t n = 0;
  size_t totalfn = 0;
  while (unw_step(&cursor)) {
    totalfn++;
    if (totalfn < DROP_BACKTRACE_FRAMES_COUNT) {
      continue;
    }
    unw_word_t ip, sp, off;

    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);

    char symbol[256] = {"<unknown>"};
    char *name = symbol;

    unw_get_proc_name(&cursor, symbol, sizeof(symbol), &off);

    fprintf(outstream,
            "#%-2zu 0x%016" PRIxPTR " sp=0x%016" PRIxPTR " %s + 0x%" PRIxPTR
            "\n",
            ++n, (uintptr_t)ip, (uintptr_t)sp, name, (uintptr_t)off);
  }
}

int log4c_finalize(void) {
  if (!initialized) {
    return LOG4C_ERROR_NOT_INITIALIZED;
  }
  for (size_t i = 0; i < MAX_APPENDER_COUNT; i++) {
    if (appenders[i] == NULL)
      break;
    int rcode = appenders[i]->release(appenders[i]);
    if (rcode != 0) {
      fprintf(stderr, "Error closing appender %zu: %d", i, rcode);
    }
    free(appenders[i]);
    appenders[i] = NULL;
  }
  return 0;
}

static void log4c_finalize_atexit(void) {
  if (!initialized) {
    return;
  }
  LOG4C_DEBUG("Finalizing log4c");
  int rcode = log4c_finalize();
  if (rcode != 0) {
    fprintf(stderr, "Cannot finalize log4c, error: %d\n", rcode);
  }
}

void print_thread_id(FILE *output_stream, pthread_t id) {
  size_t i;
  for (i = sizeof(i); i; --i)
    fprintf(output_stream, "%02x", *(((unsigned char *)&id) + i - 1));
}

int write_message_to_stream(const log4c_message_type *message,
                            const char *format, FILE *output_stream) {
  size_t format_len = strlen(format);
  for (size_t i = 0; i < format_len; i++) {
    if (format[i] != '%' || i == format_len - 1) {
      fputc(format[i], output_stream);
      continue;
    }
    if (format[i + 1] == '%') {
      fputc('%', output_stream);
      i++;
      continue;
    }
    i++;
    int printed_count = 0;
    switch (format[i]) {
    case 't': {
      struct tm *time_to_print;
      time_to_print = localtime(&message->ts.tv_sec);
      printed_count = fprintf(
          output_stream, "%d-%02d-%02d %02d:%02d:%02d.%06lu",
          time_to_print->tm_year + 1900, time_to_print->tm_mon + 1,
          time_to_print->tm_mday, time_to_print->tm_hour, time_to_print->tm_min,
          time_to_print->tm_sec, message->ts.tv_usec);
    } break;
    case 'p': {
      pthread_t tid = pthread_self();
      print_thread_id(output_stream, tid);
    } break;
    case 'l': {
      char *level_str = LOG4C_INFO_STR;
      switch (message->level) {
      case DEBUG:
        level_str = use_colored_level ? LOG4C_DEBUG_STR_C : LOG4C_DEBUG_STR;
        break;
      case INFO:
        level_str = use_colored_level ? LOG4C_INFO_STR_C : LOG4C_INFO_STR;
        break;
      case WARNING:
        level_str = use_colored_level ? LOG4C_WARNING_STR_C : LOG4C_WARNING_STR;
        break;
      case ERROR:
        level_str = use_colored_level ? LOG4C_ERROR_STR_C : LOG4C_ERROR_STR;
        break;
      }
      printed_count = fprintf(output_stream, "%s", level_str);
    } break;
    case 'f':
      printed_count =
          fprintf(output_stream, "%s:%s", message->file, message->line);
      break;
    case 'c':
      printed_count = fprintf(output_stream, "%s", message->function);
      break;
    case 'm':
      printed_count = fprintf(output_stream, "%s", message->message_string);
      break;
    }
    if (printed_count < 0) {
      fprintf(stderr, "Cannot write to stream, error: %d", printed_count);
      perror("Error writing to file stream");
      return LOG4C_IO_ERROR;
    }
    uint32_t msize = 0;
    size_t readed_size_symbols = 0;
    if (i + 1 < format_len && isdigit(format[i + 1])) {
      char *endptr;
      msize = strtoul(format + i + 1, &endptr, 10);
      readed_size_symbols = endptr - (format + i + 1);
    }
    if (readed_size_symbols != 0) {
      int to_print_spaces = msize - printed_count;
      if (to_print_spaces < 0)
        to_print_spaces = 0;
      for (size_t j = 0; j < (size_t)to_print_spaces; j++) {
        fputc(' ', output_stream);
      }
      i += readed_size_symbols;
    }
  }
  if (fprintf(output_stream, "\n") < 0) {
    return LOG4C_IO_ERROR;
  }
  return 0;
}

static int parse_cmdline_args(int argc, char **argv,
                              log4c_appeder_cfg_struct **parsed_cfgs,
                              size_t *parsed_cfg_size) {
  for (size_t i = 0; i < MAX_APPENDER_COUNT; i++) {
    parsed_cfgs[i] = NULL;
  }
  char *params_map[MAX_APPENDER_COUNT * 5];
  memset(params_map, 0, sizeof(char *) * 5 * MAX_APPENDER_COUNT);
  for (size_t i = 1; i < (unsigned int)argc; i++) {
    char *c_arg = argv[i];
    if (strncmp(LOG4C_CMD_LINE_PREFIX, c_arg, strlen(LOG4C_CMD_LINE_PREFIX)) !=
        0) {
      continue;
    }
    uint8_t appender_num =
        strtoul(c_arg + strlen(LOG4C_CMD_LINE_PREFIX), NULL, 10);
    if (appender_num == 0 || appender_num > MAX_APPENDER_COUNT) {
      continue;
    }
    char *dot_pos = strchr(c_arg + strlen(LOG4C_CMD_LINE_PREFIX), '.');
    if (dot_pos == NULL) {
      continue;
    }
    dot_pos++;
    char *equals_pos = strchr(dot_pos, '=');
    if (equals_pos == NULL) {
      continue;
    }
    char buf[255];
    size_t tocopy = (equals_pos - dot_pos > 254 ? 254 : equals_pos - dot_pos);
    memcpy(buf, dot_pos, tocopy);
    buf[tocopy] = '\0';
    int pos = -1;
    if (strcmp(buf, LOG4C_CMD_TYPE_LITERAL) == 0) {
      pos = LOG4C_CMDLINE_TYPE_HASH;
    } else if (strcmp(buf, LOG4C_CMD_FORMAT_LITERAL) == 0) {
      pos = LOG4C_CMDLINE_FORMAT_HASH;
    } else if (strcmp(buf, LOG4C_CMD_THRESHOLD_LITERAL) == 0) {
      pos = LOG4C_CMDLINE_THRESHOLD_HASH;
    } else if (strcmp(buf, LOG4C_CMD_FILENAME_LITERAL) == 0) {
      pos = LOG4C_CMDLINE_FILENAME_HASH;
    } else if (strcmp(buf, LOG4C_CMD_ERROR_TO_STDERR_LITERAL) == 0) {
      pos = LOG4C_CMDLINE_ERROR_TO_STDERR_HASH;
    }
    if (pos == -1) {
      continue;
    }
    params_map[(appender_num - 1) * 5 + pos] = equals_pos + 1;
  }
  int rcode = 0;
  size_t initialized_appenders_cnt = 0;
  for (size_t i = 0; i < MAX_APPENDER_COUNT; i++) {
    char *type = params_map[i * 5 + LOG4C_CMDLINE_TYPE_HASH];
    if (type == NULL) {
      break;
    }
    log4c_appeder_cfg_struct *base_new_appender = NULL;
    if (strcmp(type, "file") == 0) {
      log4c_file_appender_cfg_struct *created_appender =
          malloc(sizeof(log4c_file_appender_cfg_struct));
      if (created_appender == NULL) {
        rcode = LOG4C_MEMORY_ERROR;
        goto release_on_error;
      }
      created_appender->base.type = LOG4C_APPENDER_TYPE_FILE;
      created_appender->file_name =
          params_map[i * 5 + LOG4C_CMDLINE_FILENAME_HASH];
      base_new_appender = (log4c_appeder_cfg_struct *)created_appender;
    } else if (strcmp(type, "stdout") == 0) {
      log4c_stdout_appender_cfg_struct *created_appender =
          malloc(sizeof(log4c_stdout_appender_cfg_struct));
      if (created_appender == NULL) {
        rcode = LOG4C_MEMORY_ERROR;
        goto release_on_error;
      }
      created_appender->base.type = LOG4C_APPENDER_TYPE_STDOUT;
      created_appender->error_to_stderr =
          params_map[i * 5 + LOG4C_CMDLINE_ERROR_TO_STDERR_HASH] != NULL &&
          strcmp(params_map[i * 5 + LOG4C_CMDLINE_ERROR_TO_STDERR_HASH],
                 "true") == 0;
      base_new_appender = (log4c_appeder_cfg_struct *)created_appender;
    } else {
      fprintf(stderr, "Wrong appender type %s\n", type);
      rcode = LOG4C_CONFIG_ERROR;
      goto release_on_error;
    }
    parsed_cfgs[i] = base_new_appender;
    parsed_cfgs[i]->message_format = params_map[i * 5 + 1];
    char *parsed_threshold = params_map[i * 5 + LOG4C_CMDLINE_THRESHOLD_HASH];
    if (parsed_threshold != NULL) {
      if (strcmp(parsed_threshold, LOG4C_ERROR_STR) == 0) {
        parsed_cfgs[i]->threshold = ERROR;
      } else if (strcmp(parsed_threshold, LOG4C_INFO_STR) == 0) {
        parsed_cfgs[i]->threshold = INFO;
      } else if (strcmp(parsed_threshold, LOG4C_WARNING_STR) == 0) {
        parsed_cfgs[i]->threshold = WARNING;
      } else if (strcmp(parsed_threshold, LOG4C_DEBUG_STR) == 0) {
        parsed_cfgs[i]->threshold = DEBUG;
      } else {
        parsed_cfgs[i]->threshold = -1;
      }
    } else {
      parsed_cfgs[i]->threshold = -1;
    }
    initialized_appenders_cnt++;
  }
  if (initialized_appenders_cnt == 0) {
    fprintf(stderr, "No appenders defined - using default one\n");
    log4c_appeder_cfg_struct *default_appender =
        malloc(sizeof(log4c_stdout_appender_cfg_struct));
    if (default_appender == NULL) {
      perror("Cannot allocate memory");
      rcode = LOG4C_MEMORY_ERROR;
      goto release_on_error;
    }
    default_appender->type = LOG4C_APPENDER_TYPE_STDOUT;
    default_appender->threshold = DEBUG;
    default_appender->message_format = LOG4C_DEFAULT_MESSAGE_FORMAT;
    ((log4c_stdout_appender_cfg_struct *)default_appender)->error_to_stderr =
        true;
    parsed_cfgs[0] = default_appender;
    *parsed_cfg_size = 1;
    return 0;
  }
  *parsed_cfg_size = initialized_appenders_cnt;
  return rcode;
release_on_error:
  for (size_t i = 0; i < MAX_APPENDER_COUNT; i++) {
    if (parsed_cfgs[i] != NULL) {
      free(parsed_cfgs[i]);
      parsed_cfgs[i] = NULL;
    }
  }
  return rcode;
}

int log4c_stdout_open(__attribute__((unused))
                      log4c_appeder_cfg_struct *appender) {
  return 0;
}

int log4c_stdout_write(const log4c_appeder_cfg_struct *appender,
                       const log4c_message_type *message) {
  log4c_stdout_appender_cfg_struct *stdout_appender =
      (log4c_stdout_appender_cfg_struct *)appender;

  FILE *output_stream = stdout;
  if (stdout_appender->error_to_stderr && message->level == ERROR) {
    output_stream = stderr;
  }
  int rcode =
      write_message_to_stream(message, appender->message_format, output_stream);
  if (rcode != 0) {
    return rcode;
  }
  if (message->level == ERROR) {
    backtrace(output_stream);
  }
  return 0;
}

int log4c_stdout_release(__attribute__((unused))
                         log4c_appeder_cfg_struct *appender) {
  return 0;
}

int log4c_file_open(log4c_appeder_cfg_struct *appender) {
  log4c_file_appender_cfg_struct *file_appender =
      (log4c_file_appender_cfg_struct *)appender;
  FILE *fhandle = fopen(file_appender->file_name, "w+");
  if (fhandle == NULL) {
    fprintf(stderr, "Cannot open file %s\n", file_appender->file_name);
    perror("Error opening file");
    return LOG4C_IO_ERROR;
  }
  file_appender->file_handle = fhandle;
  return 0;
}

int log4c_file_write(const log4c_appeder_cfg_struct *appender,
                     const log4c_message_type *message) {
  log4c_file_appender_cfg_struct *file_appender =
      (log4c_file_appender_cfg_struct *)appender;
  if (file_appender->file_handle == NULL) {
    return LOG4C_ERROR_NOT_INITIALIZED;
  }
  int rcode = write_message_to_stream(message, appender->message_format,
                                      file_appender->file_handle);
  if (rcode != 0) {
    return rcode;
  }
  if (message->level == ERROR) {
    backtrace(file_appender->file_handle);
  }
  return 0;
}

int log4c_file_release(log4c_appeder_cfg_struct *appender) {
  log4c_file_appender_cfg_struct *file_appender =
      (log4c_file_appender_cfg_struct *)appender;
  if (file_appender->file_handle != NULL) {
    int rcode = fclose(file_appender->file_handle);
    file_appender->file_handle = NULL;
    if (rcode != 0) {
      fprintf(stderr, "Cannot close log file %s\n", file_appender->file_name);
      return LOG4C_IO_ERROR;
    }
  }
  return 0;
}

static int log4c_init_appenders(log4c_appeder_cfg_struct *appender_list[],
                                size_t appender_list_size) {
  size_t result_count = 0;
  for (size_t i = 0; i < appender_list_size; i++) {
    log4c_appeder_cfg_struct *current_cfg = appender_list[i];
    switch (current_cfg->type) {
    case LOG4C_APPENDER_TYPE_STDOUT:
      current_cfg->open = log4c_stdout_open;
      current_cfg->write = log4c_stdout_write;
      current_cfg->release = log4c_stdout_release;
      break;
    case LOG4C_APPENDER_TYPE_FILE:
      current_cfg->open = log4c_file_open;
      current_cfg->write = log4c_file_write;
      current_cfg->release = log4c_file_release;
      break;
    default:
      fprintf(stderr, "Wrong appender type %d\n", current_cfg->type);
      continue;
    }
    int rcode = current_cfg->open(current_cfg);
    if (rcode != 0) {
      fprintf(stderr, "Cannot open appender %zu\n", i);
      goto error_open;
    }
    appenders[result_count] = current_cfg;
    result_count++;
  }
  return result_count;
error_open:
  for (size_t i = 0; i < result_count; i++) {
    appender_list[i]->release(appender_list[i]);
  }
  return 0;
}

int log4c_setup_appenders(log4c_appeder_cfg_struct *appender_list[],
                          size_t appender_list_size) {
  if (appender_list_size == 0 || appender_list_size > MAX_APPENDER_COUNT) {
    fprintf(stderr, "Wrong appender list size %zu\n", appender_list_size);
    return LOG4C_CONFIG_ERROR;
  }
  // validate
  for (size_t i = 0; i < appender_list_size; i++) {
    if (appender_list[i] == NULL) {
      fprintf(stderr, "Null appender at position %zu\n", i);
      return LOG4C_CONFIG_ERROR;
    }
    if (appender_list[i]->type != LOG4C_APPENDER_TYPE_STDOUT &&
        appender_list[i]->type != LOG4C_APPENDER_TYPE_FILE) {
      fprintf(stderr, "Wrong appender type %d at position %zu\n",
              appender_list[i]->type, i);
      return LOG4C_CONFIG_ERROR;
    }
    if (appender_list[i]->threshold < DEBUG ||
        appender_list[i]->threshold > ERROR) {
      // fprintf(stderr,
      //         "Assigning default threshold level to appender at potision
      //         %zu\n", i);
      appender_list[i]->threshold = LOG4C_DEFAULT_THRES_LEVEL;
    }
    if (appender_list[i]->type == LOG4C_APPENDER_TYPE_FILE &&
        (((log4c_file_appender_cfg_struct *)appender_list[i])->file_name ==
             NULL ||
         (strlen(((log4c_file_appender_cfg_struct *)appender_list[i])
                     ->file_name) == 0))) {
      fprintf(stderr, "Null or empty file name in appender at position %zu\n",
              i);
      return LOG4C_CONFIG_ERROR;
    }
    if (appender_list[i]->message_format == NULL) {
      appender_list[i]->message_format = LOG4C_DEFAULT_MESSAGE_FORMAT;
    }
  }
  atexit(log4c_finalize_atexit);
  int init_count = log4c_init_appenders(appender_list, appender_list_size);
  if (init_count == 0) {
    init_error = true;
    return LOG4C_INIT_ERROR;
  }
  initialized = true;
  return 0;
}

int log4c_setup_cmdline(int argc, char **argv) {
  int rcode = 0;
  if (initialized) {
    return LOG4C_ERROR_ALREADY_INITIALIZED;
  }
  size_t parsed_size;
  log4c_appeder_cfg_struct *parsed_cfgs[MAX_APPENDER_COUNT];
  rcode = parse_cmdline_args(argc, argv, parsed_cfgs, &parsed_size);
  if (rcode != 0) {
    return rcode;
  }
  rcode = log4c_setup_appenders(parsed_cfgs, parsed_size);
  if (rcode != 0) {
    for (size_t i = 0; i < parsed_size; i++) {
      if (parsed_cfgs[i] == NULL) {
        continue;
      }
      free(parsed_cfgs[i]);
      parsed_cfgs[i] = NULL;
    }
  }
  return rcode;
}

int log4c_lazy_initialize(void) {
  int rcode = 0;
  pthread_mutex_lock(&pthread_lock);
  if (initialized) {
    rcode = LOG4C_ERROR_ALREADY_INITIALIZED;
    goto release;
  }
  fprintf(
      stderr,
      "LOG4C: No comfig specified, initializing log4c with default config\n");
  log4c_appeder_cfg_struct *default_appender =
      malloc(sizeof(log4c_stdout_appender_cfg_struct));
  if (default_appender == NULL) {
    perror("Cannot allocate memory");
    rcode = LOG4C_MEMORY_ERROR;
    goto release;
  }
  default_appender->type = LOG4C_APPENDER_TYPE_STDOUT;
  default_appender->threshold = DEBUG;
  default_appender->message_format = LOG4C_DEFAULT_MESSAGE_FORMAT;
  ((log4c_stdout_appender_cfg_struct *)default_appender)->error_to_stderr =
      true;
  rcode = log4c_setup_appenders(&default_appender, 1);
  if (rcode != 0) {
    free(default_appender);
  }
release:
  pthread_mutex_unlock(&pthread_lock);
  return rcode;
}

void log4c_log(log4c_level level, const char *file, const char *line,
               const char *func, const char *message_format, ...) {
  if (init_error) {
    return;
  }
  if (!initialized) {
    int rcode = log4c_lazy_initialize();
    if (rcode != 0) {
      return;
    }
  }

  log4c_message_type *message = malloc(sizeof(log4c_message_type));
  if (message == NULL) {
    fprintf(stderr, "Cannot allocate memory for message\n");
    return;
  }

  struct timeval current_time;
  int rcode = gettimeofday(&current_time, NULL);
  if (rcode != 0) {
    fprintf(stderr, "Cannot get event time\n");
    return;
  }
  message->file = file;
  message->level = level;
  message->line = line;
  message->function = func;
  message->ts = current_time;
  message->print_stacktrace = (level == ERROR);

  char message_string[MAX_MESSAGE_SIZE];
  va_list(args);
  va_start(args, message_format);
  vsnprintf(message_string, MAX_MESSAGE_SIZE, message_format, args);
  message->message_string = message_string;

  pthread_mutex_lock(&pthread_lock);
  for (size_t i = 0; i < MAX_APPENDER_COUNT; i++) {
    if (appenders[i] == NULL)
      break;
    if (appenders[i]->threshold > level) {
      continue;
    }
    int rcode = appenders[i]->write(appenders[i], message);
    if (rcode != 0) {
      fprintf(stderr, "Error writing message to appender %zu: %d\n", i, rcode);
      fprintf(stderr, "%s\n", message->message_string);
    }
  }
  pthread_mutex_unlock(&pthread_lock);
  free(message);
}

void log4c_set_level_color(bool p_use_colored_level) {
  use_colored_level = p_use_colored_level;
}
