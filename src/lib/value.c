
#include <stdlib.h>
#include <string.h>

#if defined(NRF5)
#include <app_util_platform.h>
#else
#include <pthread.h>
#endif

#include <items.h>

#include <onex-kernel/mem.h>
#include <onex-kernel/log.h>

char* unknown_to_text(char* b){ *b='?'; *(b+1)=0; return b; }

typedef struct value {
  item_type type;
  char*  val;
  uint16_t refs;
} value;

static properties* all_values=0;

#if defined(NRF5)
#define MAX_VALUES 256
#define MAX_TEXT_LEN 64
#else
#define MAX_VALUES 4096
#define MAX_TEXT_LEN 4096
#endif

#if defined(NRF5)

#define ENTER_LOCKING                                  \
        uint8_t __CR_NESTED = 0;                       \
        app_util_critical_region_enter(&__CR_NESTED)

#define RETURN_UNLOCKING(x)                            \
        app_util_critical_region_exit(__CR_NESTED);    \
        return x

#else

static pthread_mutex_t value_lock;
#define ENTER_LOCKING       pthread_mutex_lock(&value_lock)
#define RETURN_UNLOCKING(x) pthread_mutex_unlock(&value_lock); return x

#endif

value* value_new(char* val)
{
  if(!val) return 0;
  if(!all_values) all_values=properties_new(MAX_VALUES);
  ENTER_LOCKING;
  value* ours=(value*)properties_get(all_values, val);
  if(ours){
    ours->refs++;
    if(ours->refs==0) log_write("V0%s\n", ours->val); // 65536 references so more likely not being freed
    RETURN_UNLOCKING(ours);
  }
  ours=(value*)mem_alloc(sizeof(value));
  if(!ours){
    log_write("VALS!!\n"); // this is serious
    RETURN_UNLOCKING(0);
  }
  if(strchr(val, ' ') || strchr(val, '\n')){
    log_write("VALS ! '%s'\n", val); // this is serious
 // RETURN_UNLOCKING(0); // it happens, so...
  }
  ours->type=ITEM_VALUE;
  ours->val=mem_strdup(val);
  ours->refs=1; // don't count our own 2 uses in all_values

  if(!ours->val){
    log_write("!VALS!\n"); // this is serious
    mem_free(ours);
    RETURN_UNLOCKING(0);
  }
  if(!properties_set(all_values, ours->val, ours)){
    log_write("!!VALS\n"); // this is serious
    mem_freestr(ours->val);
    mem_free(ours);
    RETURN_UNLOCKING(0);
  }
  RETURN_UNLOCKING(ours);
}

value* value_ref(value* v)
{
  if(!v) return 0;
  ENTER_LOCKING;
  v->refs++;
  RETURN_UNLOCKING(v);
}

void value_free(value* v)
{
  if(!v) return;
  ENTER_LOCKING;
  v->refs--;
  if(v->refs){
    RETURN_UNLOCKING();
  }
  properties_delete(all_values, v->val);
  mem_freestr(v->val);
  mem_free(v);
  RETURN_UNLOCKING();
}

char* value_string(value* v)
{
  if(!v) return 0;
  return v->val;
}

bool value_equal(value* v1, value* v2)
{
  if(!v1) return !v2;
  if(!v2) return false;
  if(v1==v2) return true;
  return v1->val == v2->val;
}

bool value_is(value* v, char* s)
{
  if(!v) return !s;
  return !strcmp(v->val, s);
}

char* value_to_text(value* v, char* b, uint16_t s)
{
  if(!v){ *b = 0; return b; }
  int ln=snprintf(b,s,"%s",v->val);
  if(ln>=s){ *b = 0; return b; }
  if(b[ln-1]==':'){
    ln++;
    if(ln>=s){ *b = 0; return b; }
    b[ln-2]='\\';
    b[ln-1]=':';
    b[ln]=0;
  }
  return b;
}

void value_log(value* v)
{
  char buf[MAX_TEXT_LEN];
  log_write("%s\n", value_to_text(v,buf,MAX_TEXT_LEN));
}

void value_dump()
{
  uint16_t s=properties_size(all_values);
  for(uint16_t i=1; i<=s; i++){
    char* key=properties_key_n(all_values, i);
    value* val=(value*)properties_get_n(all_values, i);
    log_write("[%d|%s|%d]\n", i, key, val->refs);
    log_flush();
  }
}

