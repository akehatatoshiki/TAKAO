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

/* The mark stack is fixed size.  If it overflows (which
   shouldn't normally happen except in extremely nested
   structures), marking falls back to a slower heap scan */
#define MARK_STACK_SIZE 16384
static int mark_stack_count = 0;
static Object *mark_stack[MARK_STACK_SIZE];

/* Macros for manipulating the mark bit array */

#define REHASH(key)        (key++^1901) % MARK_STACK_SIZE

static int HASH(ptr){
  int hash =  ((uintptr_t)ptr^8677) % MARK_STACK_SIZE;
  while(mark_stack_count < MARK_STACK_SIZE){
    if(!mark_stack[hash] || ptr == mark_stack[hash]){
      break;
    }else{
      hash = REHASH(hash);
    }
    mark_stack_count++;
  }
  mark_stack_count = 0;
  return hash;
}

#define MARK(ptr)          mark_stack[HASH(ptr)] = ptr;

#define IS_MARKED(ptr)     mark_stack[HASH(ptr)]

#define UNMARK(ptr)        mark_stack[HASH(ptr)] = (void *)"UNMARKED";


/* Function for handle cache */

static inline void
md(){
  asm volatile("mfence");
}

static inline void
clflush(volatile void *p){
  asm volatile("clflush (%0)" :: "r" (p));
}

void clflush_cache_range(void *ptr){
  md();
  clflush(ptr);
  md();
  if(testing_mode) printf("Flushed\n");
}

void doFlushMark(Object *ob) {
    long start,end;
    start = rdtsc();
    Class *class = ob->class;
    ClassBlock *cb = CLASS_CB(class);

    if(class == NULL){
      printf("error on doFlushMark\n");
      return;
    }

    if(cb->name[0] == '[') {
        if((cb->name[1] == 'L') || (cb->name[1] == '[')) {
            Object **body = ARRAY_DATA(ob, Object*);
            int len = ARRAY_LEN(ob);
            int i;

            if(testing_mode) printf("Scanning Array object @%p class is %s len is %d\n",
                     ob, cb->name, len);

            for(i = 0; i < len; i++) {
                Object *ob = *body++;
                if(testing_mode) printf("Object at index %d is @%p\n", i, ob);

                if(ob != NULL) MARK(ob);
            }
        } else {
            if(testing_mode) printf("Array object @%p class is %s  - Not Scanning...\n",ob, cb->name);
	          if(ob != NULL) MARK(ob);
        }
    } else {
        int i;

        if(IS_SPECIAL(cb)) {
            if(IS_CLASS_CLASS(cb)) {
                if(testing_mode) printf("Found class object @%p name is %s\n", ob,
                         CLASS_CB(ob)->name);
                markClassData(ob);
            } else if(IS_REFERENCE(cb)) {
                Object *referent = INST_DATA(ob, Object*, ref_referent_offset);

                if(testing_mode) printf("Mark found Reference object @%p class %s"
                         " flags %d referent @%p\n",
                         ob, cb->name, cb->flags, referent);

                markClassData(ob);

            }
        }
        for(i = 0; i < cb->refs_offsets_size; i++) {
            int offset = cb->refs_offsets_table[i].start;
            int end = cb->refs_offsets_table[i].end;

            for(; offset < end; offset += sizeof(Object*)) {
                Object *ref = INST_DATA(ob, Object*, offset);

                if(ref != NULL) MARK(ref);
            }
        }
    }
    end = rdtsc();
    //printf("mesuared time(searchChildren) : %I64d clock\n",end - start);

}

#define MARK_CLASSBLOCK_FIELD(cb, field, mark)           \
    if(cb->field != NULL && !IS_MARKED(cb->field)) \
        MARK(cb->field)

void markClassData(Class *class) {
    ClassBlock *cb = CLASS_CB(class);
    ConstantPool *cp = &cb->constant_pool;
    FieldBlock *fb = cb->fields;
    int i;
    /* Static fields are initialised to default values during
       preparation (done in the link phase).  Therefore, don't
       scan if the class hasn't been linked */
    if(cb->state >= CLASS_LINKED)
        for(i = 0; i < cb->fields_count; i++, fb++)
            if((fb->access_flags & ACC_STATIC) &&
                        ((*fb->type == 'L') || (*fb->type == '['))) {
                Object *ob = fb->u.static_value.p;
                if(testing_mode) printf("Field %s %s object @%p\n", fb->name, fb->type, ob);
                if(ob != NULL && !IS_MARKED(ob))
                    MARK(ob);
            }

    if(testing_mode) printf("Marking constant pool resolved objects for class %s\n", cb->name);

    /* Scan the constant pool and mark all resolved object references */
    for(i = 1; i < cb->constant_pool_count; i++) {
        int type = CP_TYPE(cp, i);

        if(type >= CONSTANT_ResolvedString) {
            Object *ob;

            if(type == CONSTANT_ResolvedPolyMethod)
                ob = ((PolyMethodBlock*)CP_INFO(cp, i))->appendix;
            else
                ob = (Object *)CP_INFO(cp, i);
            if(testing_mode) printf("Resolved object @ constant pool idx %d type %d @%p\n",
                     i, type, ob);
            if(ob != NULL && !IS_MARKED(ob))
                MARK(ob);
        } else if(type == CONSTANT_ResolvedInvokeDynamic) {
            ResolvedInvDynCPEntry *entry = (ResolvedInvDynCPEntry*)
                                           CP_INFO(cp, i);
            InvDynMethodBlock *idmb;

            for(idmb = entry->idmb_list; idmb != NULL; idmb = idmb->next) {
                Object *ob = idmb->appendix;
                if(testing_mode) printf("InvokeDynamic appendix @ constant pool idx %d @%p\n",
                         i, ob);
                if(ob != NULL && !IS_MARKED(ob))
                    MARK(ob);
            }
        }
    }
}

void doflush(Object *ob) {
    long start,end;
    start = rdtsc();
    Class *class = ob->class;
    ClassBlock *cb = CLASS_CB(class);

    if(class == NULL){
      printf("error on serchChildren\n");
      return;
    }

    if(cb->name[0] == '[') {
        if((cb->name[1] == 'L') || (cb->name[1] == '[')) {
            Object **body = ARRAY_DATA(ob, Object*);
            int len = ARRAY_LEN(ob);
            int i;

            if(testing_mode) printf("Scanning Array object @%p class is %s len is %d\n",
                     ob, cb->name, len);

            for(i = 0; i < len; i++) {
                Object *ob = *body++;
                if(testing_mode) printf("Object at index %d is @%p\n", i, ob);
                if(ob != NULL && IS_MARKED(ob)){
                  clflush_cache_range(ob);
                  UNMARK(ob);
                }
            }
        } else {
            if(testing_mode) printf("Array object @%p class is %s  - Not Scanning...\n",
                     ob, cb->name);
	          if(ob != NULL && IS_MARKED(ob)){
              clflush_cache_range(ob);
              UNMARK(ob);
            }
       }
    } else {
        int i;

        if(IS_SPECIAL(cb)) {
            if(IS_CLASS_CLASS(cb)) {
                if(testing_mode) printf("Found class object @%p name is %s\n", ob,
                         CLASS_CB(ob)->name);
                if(IS_MARKED(ob)){
                  clflush_cache_range(ob);
                  UNMARK(ob);
                }
            } else if(IS_REFERENCE(cb)) {
                Object *referent = INST_DATA(ob, Object*, ref_referent_offset);
                if(IS_MARKED(ob)){
                  clflush_cache_range(ob);
                  UNMARK(ob);
                }
            }
        }

        if(testing_mode) printf("Scanning object @%p class is %s\n", ob, cb->name);

        for(i = 0; i < cb->refs_offsets_size; i++) {
            int offset = cb->refs_offsets_table[i].start;
            int end = cb->refs_offsets_table[i].end;

            for(; offset < end; offset += sizeof(Object*)) {
                Object *ref = INST_DATA(ob, Object*, offset);

                if(ref != NULL && IS_MARKED(ref)) clflush_cache_range(ref);
            }
        }
    }
    end = rdtsc();
    //printf("mesuared time(searchChildren) : %I64d clock\n",end - start);

}

void flush(Object* ob){
    mark_stack = (Object *)malloc(sizeof(Object) * MARK_STACK_SIZE);
    if(mark_stack == NULL) perror("In flush\n");return;
    doFlushMark(ob);
    doFlush(ob);
    free(mark_stack);
}
