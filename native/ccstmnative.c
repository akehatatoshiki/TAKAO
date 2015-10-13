#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <jni.h>
#include <errno.h>

#include "jam.h"
#include "TAKAO_TAKAONative.h"

#define KEY_SIZE 256
#define LOG_SIZE 10
#define PAGESIZE sysconf(_SC_PAGE_SIZE)

static int IS_INITED = 0;
static int IS_LOG_INITED = 0;
static const char *logspath = "Redologs";

typedef struct logs{
  void *handle;
  void *value;
} Logs;

static int idx = 0;

static Logs *logs;

#define LOGS_SIZE sizeof(Logs)

static int logsize = 0;

int synclogs(){
  if(msync(logs,logsize,MS_SYNC) == -1){
    perror("msync");
    return -1;
  }
  return 0;
}

/* Function for handle logs */

int initlogs(){
  logsize = ((LOGS_SIZE*LOG_SIZE)/PAGESIZE + 1)*PAGESIZE;
  logs = (Logs *)mmap(0,logsize,PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
  if (logs == NULL){
  		perror("error occourd at init logs");
      return -1;
  }
  return 0;
}

void push(jobject h,jobject v){
  if(idx < 100){
    logs[idx].handle = h;
    logs[idx].value = v;
    idx++;
  }
}

void flush_logs(){
  int i;
  for(i = 0; i < idx; i++){
    if(logs[i].handle)
      clflush_cache_range(logs[i].handle);
  }
}

void delete_logs(){
  munmap(logs,logsize);
}

/* Up to here */
