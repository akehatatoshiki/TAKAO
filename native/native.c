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

//#include "rdtsc.h"
#include "jam.h"
#include "TAKAO_TAKAONative.h"

#define KEY_SIZE 256
#define HASH_SIZE 10
#define LOG_SIZE 100000000
#define PAGESIZE sysconf(_SC_PAGE_SIZE)

static int IS_INITED = 0;
static int IS_LOG_INITED = 0;
static const char *hashpath = "PersistenceHash";
int ref_referent_offset = -1;

static inline long
rdtsc() {
  long ret;
  __asm__ volatile ("rdtsc" : "=A" (ret));
  return ret;
}

typedef struct phash{
  char key[KEY_SIZE];
  void *val;
} PHash;

typedef struct logdata{
  void *pval;
  void *old_data;
} Logdata;

typedef struct logs{
  Logdata *log;
  int index;
} Logs;

static PHash *hash;
static Logs *logs;
static int idx;

#define PHASH_SIZE sizeof(PHash)
#define LOGS_SIZE sizeof(Logdata)*LOG_SIZE

static int hashsize = 0;
static int logsize = 0;

/* Function for handle hash */

int syncHash(){
  if(msync(hash,hashsize,MS_SYNC) == -1){
    perror("msync");
    return -1;
  }
  return 0;
}

int initFiles(int fd,long size){
  if(fd == -1){
    perror("Invalid fd");
    return -1;
  }
  if(lseek(fd,size,SEEK_SET) < 0){
    printf("%d\n",errno);
    perror("lseek");
    return -1;
  }
  if(write(fd,"",1) == -1){
    perror("write");
    return -1;
  }
  return fd;
}


void clearHash(PHash *ptr){
  int i = 0;
  while(i<HASH_SIZE){
	   ptr[i].key[0] = '\0';
	   ptr[i].val = NULL;
	   i++;
  }
}

int initHash(){
  printf("Try to initialise PHash from java\n");
  hashsize = ((PHASH_SIZE*HASH_SIZE)/PAGESIZE + 1)*PAGESIZE;
  struct stat st;
  int fd;
  if(stat(hashpath,&st) == 0){
    if((fd = open (hashpath, O_RDWR | O_APPEND , S_IRUSR | S_IWUSR)) == -1){
      perror("open");
      return -1;
    }
    hash = (PHash *)mmap(0,hashsize,PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (hash == MAP_FAILED){
  		perror("error occourd at init Phash");
      return -1;
  	}
  }else{
    printf("Can't find Persistence Hashtable. Process is not persistence or wrong somewhere in JVM.\n");
    return -1;
  }
  IS_INITED = 1;
  close(fd);
  clearHash(hash);
  return 0;
}

unsigned int calcHash(const unsigned char *key){
	unsigned int calc = 0;
	while(*key) calc += (calc<<5) + *key++;
	return ((calc >> 5) + calc) % HASH_SIZE;
}

unsigned int rehash(unsigned int calc){
	return (calc+1) % HASH_SIZE;
}

int setHash(PHash *ptr, const char *key, jobject val){
	int i=1;
	unsigned int calc = calcHash(key);
	long start,end;


	if(*key == '\0'){ printf("NULL KEY error(set)\n"); return -1; }

	do{
		if(strncmp(ptr[calc].key, key, KEY_SIZE) == 0 || ptr[calc].key[0] == '\0') break;
		calc = rehash(calc);
	}while(i++ < HASH_SIZE);

	if(i>=HASH_SIZE){
		printf("error\n"); return -1;
	}
  if(ptr[calc].key[0] != '\0'){
    flush((Object*) val);
    ptr[calc].val = val;
    clflush_cache_range(ptr[calc].val);
  }else{
    //start = rdtsc();
    ptr[calc].val = val;
    flush((Object*) val);
    strncpy(ptr[calc].key, key, KEY_SIZE);
    clflush_cache_range(ptr[calc].key);
    //end = rdtsc();
    //printf("mesuared time(calcHash and set) : %I64d clock\n",end - start);
  }
  if(syncHash() == -1) return -1;
  return 0;
}

jobject getHash(PHash *ptr, const char *key){
	int i=1;
	unsigned int calc = calcHash(key);

	if(*key == '\0'){ printf("NULL KEY error(get)\n"); return NULL; }

	do{
		if(strncmp(ptr[calc].key, key, KEY_SIZE) == 0) break;
		calc = rehash(calc);
	}while(i++ < HASH_SIZE);

	if(i>=HASH_SIZE){
		printf("'%s' is not entry.\n", key);
    return NULL;
	}
  //printf("Found. %p\n",(jobject)ptr[calc].val);
	return (jobject)ptr[calc].val;
}

int delHashentry(PHash *ptr, const char *key){
  int i=1;
  unsigned int calc = calcHash(key);

  if(*key == '\0'){ printf("NULL KEY error(get)\n"); return -1; }

  do{
    if(strncmp(ptr[calc].key, key, KEY_SIZE) == 0) break;
    calc = rehash(calc);
  }while(i++ < HASH_SIZE);

  if(i>=HASH_SIZE){
    printf("'%s' is not entry.\n", key);
    return -1;
  }

  ptr[calc].val = NULL;
  ptr[calc].key[0] = '\0';
  clflush_cache_range(ptr);
  if(syncHash() == -1) return -1;

  return 0;

}

/* Up to here */

JNIEXPORT jint JNICALL Java_TAKAO_TAKAONative_addPersistenceObject
  (JNIEnv *env,jobject obj,jobject pobj,jstring jstr){
      if(IS_INITED == 0){
        if(initHash() == -1){
          return -1;
        }
      }
      long start,end;
      const char *str;
      str = (*env)->GetStringUTFChars(env,jstr,0);
      int res = setHash(hash,str,pobj);
      if(str != NULL) (*env)->ReleaseStringUTFChars(env,jstr,str);

      return res;
}

JNIEXPORT jint JNICALL Java_TAKAO_TAKAONative_deletePersistenceObject
  (JNIEnv *env,jobject obj,jstring jstr){
      if(IS_INITED == 0){
        if(initHash() == -1){
          return -1;
        }
      }
      const char *str;
      str = (*env)->GetStringUTFChars(env,jstr,0);
      int res = delHashentry(hash,str);
      if(str != NULL) (*env)->ReleaseStringUTFChars(env,jstr,str);
      return res;
}

JNIEXPORT jobject JNICALL Java_TAKAO_TAKAONative_getPersistenceObject
  (JNIEnv *env,jobject obj,jstring jstr){
      if(IS_INITED == 0){
        if(initHash() == -1){
          return NULL;
        }
      }
      const char *str;
      str = (*env)->GetStringUTFChars(env,jstr,0);
      jobject pobj = getHash(hash,str);
      if(str != NULL) (*env)->ReleaseStringUTFChars(env,jstr,str);
      printf("Found. object addr:%p\n",pobj);
      if(pobj != NULL) return pobj;
      else printf("Object is broken\n"); return NULL;
}

JNIEXPORT jint JNICALL Java_TAKAO_TAKAONative_isPersistence
  (JNIEnv *env,jobject obj,jstring jstr){
      if(IS_INITED == 0){
        if(initHash() == -1){
          return -1;
        }
      }
      const char *str;
      str = (*env)->GetStringUTFChars(env,jstr,0);
      jobject pobj = getHash(hash,str);
      if(str != NULL) (*env)->ReleaseStringUTFChars(env,jstr,str);

      if(pobj != NULL) return 0;
      else return -1;
}

JNIEXPORT void JNICALL Java_TAKAO_TAKAONative_clflush
  (JNIEnv *env,jobject obj,jobject ptr){
    flush(ptr);
}
JNIEXPORT void JNICALL Java_TAKAO_TAKAONative_init_1log
  (JNIEnv *env,jobject obj){
    initlogs();
}

JNIEXPORT void JNICALL Java_TAKAO_TAKAONative_push_1log
  (JNIEnv *env,jobject obj,jobject h,jobject v){
    jobject handle = (*env)->NewLocalRef(env,h);
    jobject value = (*env)->NewLocalRef(env,h);
    push(handle,value);
}

JNIEXPORT void JNICALL Java_TAKAO_TAKAONative_flush_1and_1delete
  (JNIEnv *env,jobject obj){
    flush_logs();
    delete_logs();
}

/* Method for test */
JNIEXPORT void JNICALL Java_TAKAO_TAKAONative_printObjectaddres
  (JNIEnv *env, jobject obj, jobject tgt){
    (*env)->GetObjectClass(env,tgt);
    printf("This objects addres is %p\n",(void *)tgt);
}
JNIEXPORT void JNICALL Java_TAKAO_TAKAONative_compaddrHashtoRaw
  (JNIEnv *env, jobject obj, jobject tgt, jstring jstr){
    if(!IS_INITED) return;
    const char *str = (*env)->GetStringUTFChars(env,jstr,0);
    jobject pobj = getHash(hash,str);
    (*env)->ReleaseStringUTFChars(env,jstr,str);
    printf("In hash obj:%p Raw:%p\n",(void *)pobj,(void *)tgt);
}
JNIEXPORT jclass JNICALL Java_TAKAO_TAKAONative_getClass
  (JNIEnv *env, jobject obj,jstring jstr){
    if(IS_INITED == 0){
      if(initHash() == -1){
        return NULL;
      }
    }
    const char *str = (*env)->GetStringUTFChars(env,jstr,0);
    jobject pobj = getHash(hash,str);
    (*env)->ReleaseStringUTFChars(env,jstr,str);
    jclass clz = (*env)->GetObjectClass(env,pobj);
    if(clz == NULL) printf("Object is Broken\n");return NULL;
    return clz;
}
JNIEXPORT void JNICALL Java_TAKAO_TAKAONative_nilMethod
  (JNIEnv *env, jobject obj){
    return;
}
JNIEXPORT void JNICALL Java_TAKAO_TAKAONative_turnOnTestingMode
  (JNIEnv *env, jobject obj){
    testing_mode = 1;
}
