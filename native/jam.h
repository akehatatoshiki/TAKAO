#include <inttypes.h>
#include <stdarg.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>

#define VMTHREAD     512
#define VMTHROWABLE 1024

/* Class states */

#define CLASS_LOADED            1
#define CLASS_LINKED            2
#define CLASS_BAD               3
#define CLASS_INITING           4
#define CLASS_INITED            5

#define CLASS_ARRAY             6
#define CLASS_PRIM              7
#define CLASSLIB_CLASS_PAD_SIZE 4*sizeof(Object*)
#define CLASSLIB_CLASS_SPECIAL (VMTHREAD | VMTHROWABLE)

#define CLASSLIB_CLASS_EXTRA_FIELDS \
    /* NONE */

#define CLASSLIB_THREAD_EXTRA_FIELDS \
    unsigned short state;

#define CLASSLIB_CLASSBLOCK_REFS_DO(action, cb, ...) \
    /* NONE */

/* Internal */
#define CONSTANT_Locked                100
#define CONSTANT_Resolved              101
#define CONSTANT_ResolvedMethod        102
#define CONSTANT_ResolvedInvokeDynamic 103
#define CONSTANT_ResolvedClass         104
#define CONSTANT_ResolvedString        105
#define CONSTANT_ResolvedMethodType    106
#define CONSTANT_ResolvedMethodHandle  107
#define CONSTANT_ResolvedPolyMethod    108

/* Macros for accessing constant pool entries */

#define CP_TYPE(cp,i)                   cp->type[i]
#define CP_INFO(cp,i)                   cp->info[i]
#define CP_CLASS(cp,i)                  (u2)cp->info[i]
#define CP_STRING(cp,i)                 (u2)cp->info[i]
#define CP_METHOD_CLASS(cp,i)           (u2)cp->info[i]
#define CP_METHOD_NAME_TYPE(cp,i)       (u2)(cp->info[i]>>16)
#define CP_INTERFACE_CLASS(cp,i)        (u2)cp->info[i]
#define CP_INTERFACE_NAME_TYPE(cp,i)    (u2)(cp->info[i]>>16)
#define CP_FIELD_CLASS(cp,i)            (u2)cp->info[i]
#define CP_FIELD_NAME_TYPE(cp,i)        (u2)(cp->info[i]>>16)
#define CP_NAME_TYPE_NAME(cp,i)         (u2)cp->info[i]
#define CP_NAME_TYPE_TYPE(cp,i)         (u2)(cp->info[i]>>16)
#define CP_UTF8(cp,i)                   (char *)(cp->info[i])
#define CP_METHOD_TYPE(cp,i)            (u2)cp->info[i]
#define CP_METHOD_HANDLE_KIND(cp,i)     (u1)cp->info[i]
#define CP_METHOD_HANDLE_REF(cp,i)      (u2)(cp->info[i]>>16)
#define CP_INVDYN_BOOT_MTHD(cp,i)       (u2)cp->info[i]
#define CP_INVDYN_NAME_TYPE(cp,i)       (u2)(cp->info[i]>>16)

#define CP_INTEGER(cp,i)                (int)(cp->info[i])
#define CP_FLOAT(cp,i)                  *((float *)&(cp->info[i]) + IS_BE64)
#define CP_LONG(cp,i)                   *(long long *)&(cp->info[i])
#define CP_DOUBLE(cp,i)                 *(double *)&(cp->info[i])

/* Class flags */

#define CLASS_CLASS             1
#define REFERENCE               2
#define SOFT_REFERENCE          4
#define WEAK_REFERENCE          8
#define PHANTOM_REFERENCE      16
#define FINALIZED              32
#define CLASS_LOADER           64
#define CLASS_CLASH           128
#define ANONYMOUS             256

typedef unsigned char          u1;
typedef unsigned short         u2;
typedef unsigned int           u4;
typedef unsigned long long     u8;

typedef uintptr_t ConstantPoolEntry;

typedef struct constant_pool {
    volatile u1 *type;
    ConstantPoolEntry *info;
} ConstantPool;

typedef struct exception_table_entry {
    u2 start_pc;
    u2 end_pc;
    u2 handler_pc;
    u2 catch_type;
} ExceptionTableEntry;

typedef struct line_no_table_entry {
    u2 start_pc;
    u2 line_no;
} LineNoTableEntry;

typedef struct object Class;

typedef struct object {
   uintptr_t lock;
   Class *class;
} Object;

typedef struct attribute_data {
   u1 *data;
   int len;
} AttributeData;

typedef union extra_attributes {
    struct {
        AttributeData *class_annos;
        AttributeData **field_annos;
        AttributeData **method_annos;
        AttributeData **method_parameter_annos;
        AttributeData **method_anno_default_val;
#ifdef JSR308
        AttributeData *class_type_annos;
        AttributeData **field_type_annos;
        AttributeData **method_type_annos;
#endif
#ifdef JSR901
        AttributeData **method_parameters;
#endif
    };
    void *data[0];
} ExtraAttributes;

#ifdef DIRECT
typedef union ins_operand {
    uintptr_t u;
    int i;
    struct {
        signed short i1;
        signed short i2;
    } ii;
    struct {
        unsigned short u1;
        unsigned short u2;
    } uu;
    struct {
        unsigned short u1;
        unsigned char u2;
        char i;
    } uui;
    void *pntr;
} Operand;

typedef struct instruction {
#ifdef DIRECT_DEBUG
    unsigned char opcode;
    char cache_depth;
    short bytecode_pc;
#endif
    const void *handler;
    Operand operand;
} Instruction;

typedef struct switch_table {
    int low;
    int high;
    Instruction *deflt;
    Instruction **entries;
} SwitchTable;

typedef struct lookup_entry {
    int key;
    Instruction *handler;
} LookupEntry;

typedef struct lookup_table {
    int num_entries;
    Instruction *deflt;
    LookupEntry *entries;
} LookupTable;

#ifdef INLINING
typedef struct opcode_info {
    unsigned char opcode;
    unsigned char cache_depth;
} OpcodeInfo;

typedef struct profile_info ProfileInfo;

typedef struct basic_block {
    union {
        struct {
            int quickened;
            ProfileInfo *profiled;
        } profile;
        struct {
            char *addr;
            struct basic_block *next;
        } patch;
    } u;
    int length;
    Instruction *start;
    OpcodeInfo *opcodes;
    struct basic_block *prev;
    struct basic_block *next;
} BasicBlock;

typedef struct quick_prepare_info {
    BasicBlock *block;
    Instruction *quickened;
    struct quick_prepare_info *next;
} QuickPrepareInfo;

typedef struct prepare_info {
    BasicBlock *block;
    Operand operand;
} PrepareInfo;

struct profile_info {
    BasicBlock *block;
    int profile_count;
    const void *handler;
    struct profile_info *next;
    struct profile_info *prev;
};
#endif

typedef Instruction *CodePntr;
#else
typedef unsigned char *CodePntr;
#endif

typedef struct methodblock MethodBlock;
typedef uintptr_t *(*NativeMethod)(Class*, MethodBlock*, uintptr_t*);

struct methodblock {
   Class *class;
   char *name;
   char *type;
   char *signature;
   u1 state;
   u1 flags;
   u2 access_flags;
   u2 max_stack;
   u2 max_locals;
   u2 args_count;
   u2 throw_table_size;
   u2 *throw_table;
   void *code;
   int code_size;
   union {
       struct {
           u2 exception_table_size;
           u2 line_no_table_size;
           ExceptionTableEntry *exception_table;
           union {
               LineNoTableEntry *line_no_table;
               MethodBlock *miranda_mb;
           };
       };
       struct {
           union {
               struct {
                   char *simple_sig;
                   int native_extra_arg;
               };
               struct {
                   int ref_count;
                   int ret_slot_size;
               };
           };
           NativeMethod native_invoker;
       };
   };
   int method_table_index;
#ifdef INLINING
   QuickPrepareInfo *quick_prepare_info;
   ProfileInfo *profile_info;
#endif
};

typedef struct poly_methodblock  {
    char *name;
    char *type;
    Object *appendix;
    MethodBlock *invoker;
} PolyMethodBlock;

typedef struct invdyn_methodblock  {
#ifndef DIRECT
    int id;
#endif
    Object *appendix;
    MethodBlock *invoker;
    struct invdyn_methodblock *next;
} InvDynMethodBlock;

typedef struct resolved_inv_dyn_cp_entry {
    char *name;
    char *type;
    int boot_method_cp_idx;
    InvDynMethodBlock *idmb_list;
#ifndef DIRECT
    InvDynMethodBlock *cache;
#endif
} ResolvedInvDynCPEntry;

typedef struct fieldblock {
   Class *class;
   char *name;
   char *type;
   char *signature;
   u2 access_flags;
   u2 constant;
   union {
       union {
           char data[8];
           uintptr_t u;
           long long l;
           void *p;
           int i;
       } static_value;
       u4 offset;
   } u;
} FieldBlock;

typedef struct itable_entry {
   Class *interface;
   int *offsets;
} ITableEntry;

typedef struct refs_offsets_entry {
    int start;
    int end;
} RefsOffsetsEntry;

typedef struct classblock {
   char pad[CLASSLIB_CLASS_PAD_SIZE];
   u1 state;
   u2 flags;
   u2 access_flags;
   u2 declaring_class;
   u2 enclosing_class;
   u2 enclosing_method;
   u2 inner_access_flags;
   u2 fields_count;
   u2 methods_count;
   u2 interfaces_count;
   u2 inner_class_count;
   u2 constant_pool_count;
   int object_size;
   int method_table_size;
   int imethod_table_size;
   int initing_tid;
   union {
       int dim;
       int refs_offsets_size;
   };
   char *name;
   char *signature;
   char *source_file_name;
   Class *super;
   FieldBlock *fields;
   MethodBlock *methods;
   Class **interfaces;
   MethodBlock **method_table;
   ITableEntry *imethod_table;
   Object *class_loader;
   u2 *inner_classes;
   char *bootstrap_methods;
   ExtraAttributes *extra_attributes;
   union {
       Class *element_class;
       RefsOffsetsEntry *refs_offsets_table;
   };
   ConstantPool constant_pool;
   CLASSLIB_CLASS_EXTRA_FIELDS
} ClassBlock;

typedef struct frame {
   CodePntr last_pc;
   uintptr_t *lvars;
   uintptr_t *ostack;
   MethodBlock *mb;
   struct frame *prev;
} Frame;

typedef struct jni_frame {
   Object **next_ref;
   Object **lrefs;
   uintptr_t *ostack;
   MethodBlock *mb;
   struct frame *prev;
} JNIFrame;

typedef struct exec_env {
    Object *exception;
    char *stack;
    char *stack_end;
    int stack_size;
    Frame *last_frame;
    Object *thread;
    char overflow;
} ExecEnv;

typedef struct prop {
    char *key;
    char *value;
} Property;

#define CLASS_CB(classRef)           ((ClassBlock*)(classRef+1))

#define IS_CLASS(object)             (object->class && IS_CLASS_CLASS( \
                                                  CLASS_CB(object->class)))
#define IS_CLASS_LOADER(cb)          (cb->flags & CLASS_LOADER)
#define IS_REFERENCE(cb)             (cb->flags & REFERENCE)
#define IS_CLASS_CLASS(cb)           (cb->flags & CLASS_CLASS)
#define IS_SPECIAL(cb)               (cb->flags & (CLASSLIB_CLASS_SPECIAL | \
                                                   CLASS_CLASS | REFERENCE | \
                                                   CLASS_LOADER))

#define INST_DATA(obj, type, offset) *(type*)&((char*)obj)[offset]
#define INST_BASE(obj, type)         ((type*)(obj+1))

#define ARRAY_DATA(arrayRef, type)   ((type*)(((uintptr_t*)(arrayRef+1))+1))
#define ARRAY_LEN(arrayRef)          *(uintptr_t*)(arrayRef+1)

/* Test mode */

extern static int testing_mode = 0;

/* Function for handle file */

extern int initFiles(int fd,long size);

/* RDTSC */

extern static inline long rdtsc();

/* Function for handle cache */

extern void flush(Object *ptr);
extern void clflush_cache_range(Object *ptr);

/* Function for CCSTM */

extern int initlogs();
extern void push(jobject h,jobject v);
extern void flush_logs();
extern void delete_logs();
