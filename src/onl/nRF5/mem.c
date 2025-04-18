
#include <mem_manager.h>
#include <string.h>
#include <onex-kernel/mem.h>
#include <onex-kernel/log.h>

#define LOG_MEM 0

static bool initialized=false;

static char* top_alloc=0;

void* Mem_alloc(char* func, int line, size_t n) {
  if(!n) return 0;
  if(!initialized){ initialized=true; nrf_mem_init(); }
  void* p=nrf_calloc(1,n);
  if(!p){
    p=calloc(1,n);
    if(LOG_MEM) log_write("****** mem_alloc using calloc %p", p);
    if((char*)p > top_alloc){
      top_alloc=p;
      if(log_to_gfx){
        if(LOG_MEM) log_write("clc %lu %s:%d %p\n", n, func, line, p);
      }else{
        if(LOG_MEM) log_write("=============== new calloc top: %p %lu %s:%d\n", p, n, func, line);
      }
    }
  }
  if(LOG_MEM) log_write("mem_alloc   %p %lu %s:%d\n", p, n, func, line);
  return p;
}

void Mem_free(char* func, int line, void* p) {
  if(!p) return;
  if(!initialized){ initialized=true; nrf_mem_init(); }
  if(LOG_MEM) log_write("mem_free    %p %lu %s:%d\n", p, (size_t)0, func, line);
  if(nrf_free(p)){
    if(LOG_MEM) log_write("****** mem_free using free %p", p);
    free(p);
  }
}

char* Mem_strdup(char* func, int line, const char* s) {
  if(!s) return 0;
  if(!initialized){ initialized=true; nrf_mem_init(); }
  size_t n=strlen(s)+1;
  char* p=nrf_malloc(n);
  if(!p){
    p=malloc(n);
    if(LOG_MEM) log_write("****** mem_strdup using malloc %p", p);
    if((char*)p > top_alloc){
      top_alloc=p;
      if(log_to_gfx){
        if(LOG_MEM) log_write("mlc %lu %s:%d %p\n", n, func, line, p);
      }else{
        if(LOG_MEM) log_write("=============== new malloc top: %p %lu %s:%d\n", p, n, func, line);
      }
    }
  }
  if(p) memcpy(p,s,n);
  char b[20]; mem_strncpy(b, s, 18);
  if(LOG_MEM) log_write("mem_strdup  %p %lu [%s] %s:%d\n", p, n, b, func, line);
  return p;
}

void Mem_freestr(char* func, int line, char* p) {
  if(!p) return;
  if(!initialized){ initialized=true; nrf_mem_init(); }
  size_t n=strlen(p)+1;
  char b[20]; mem_strncpy(b, p, 18);
  if(LOG_MEM) log_write("mem_freestr %p %lu [%s] %s:%d\n", p, n, b, func, line);
  if(nrf_free(p)){
    if(LOG_MEM) log_write("****** mem_freestr using free %p", p);
    free(p);
  }
}

void mem_strncpy(char* dst, const char* src, size_t count) {
  if(count < 1) return;
  strncpy(dst, src, count);
  dst[count-1] = 0;
}

