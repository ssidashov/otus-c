#include "log4c.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <stdatomic.h>

#define WORKER_THREAD_COUNT 20

pthread_t threads[WORKER_THREAD_COUNT];
short worker_thread_ids[WORKER_THREAD_COUNT];

volatile sig_atomic_t is_running = true;
volatile atomic_short running_thread_cnt = 0;

void * thread_func(void *vptr_args) {
  LOG4C_DEBUG("Started worker thread");
  while(is_running) {
    struct timespec tw = {0, 500000000};
    nanosleep(&tw, NULL);
    LOG4C_DEBUG("Spinning...");
    int current_val = rand();
    bool is_error = (current_val % 131) == 0;
    bool is_warning = (current_val % 21) == 0;
    if(is_warning) {
      LOG4C_WARNING("Warning condition triggered, %d", current_val);
    }
    if(is_error) {
      LOG4C_ERROR("Error condition triggered, %d", current_val);
    }
  }
  LOG4C_DEBUG("Ending execution of worker thread %d", *((int*)vptr_args));
  atomic_fetch_sub(&running_thread_cnt, 1);
  return NULL;
}

void int_handler(int signal_number) {
  signal(SIGINT, int_handler);
  if(signal_number != SIGINT) {
    return;
  }
  LOG4C_INFO("TRIGGERED SHUTDOWN...");
  is_running = false;
  while(running_thread_cnt > 0) {
    LOG4C_INFO("Waiting for worker threads to stop...");
    sleep(1);
  }
}

int main(int argc, char** argv) {
  signal(SIGINT, int_handler);
  log4c_set_level_color(true);
  int rcode = log4c_setup_cmdline(argc, argv);
  if(rcode != 0) {
    fprintf(stderr, "Cannot initialize log4c, code %d", rcode);
    return -1;
  }
  LOG4C_INFO("Started application, starting worker threads");
  for(size_t i = 0; i < WORKER_THREAD_COUNT; i++) {
    LOG4C_DEBUG("Starting worker thread %d", i);
    worker_thread_ids[i] = i;
    if ((rcode = pthread_create(&threads[i], NULL, thread_func, &worker_thread_ids[i])) != 0) {
      LOG4C_ERROR("Cannot start thread %d", i);
      worker_thread_ids[i] = -1;
    }
    running_thread_cnt++;
  }
  short total_started_threads = running_thread_cnt;
  LOG4C_INFO("%d worker thread started", running_thread_cnt);
  for (size_t i = 0; i < WORKER_THREAD_COUNT; i++) {
    if(worker_thread_ids[i] != -1){
      pthread_join(threads[i], NULL);
    }
  }
  LOG4C_INFO("All worker thread finished, count = %u", total_started_threads);
  log4c_finalize();
  return 0;
}
