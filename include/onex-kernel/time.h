#ifndef TIME_H
#define TIME_H

#include <stdint.h>
#include <stdbool.h>

void     time_init();
void     time_init_set(uint64_t es);

uint32_t time_s();  // seconds since startup
uint64_t time_es(); // Unix epoch seconds
uint64_t time_ms(); // ms since startup
uint64_t time_us(); // us since startup

void time_es_set(uint64_t es); // set current epoch seconds

typedef void (*time_up_cb)(void* arg);

uint16_t time_ticker(time_up_cb cb, void* arg, uint32_t every); // cb every ms; or 1 tick if every=0
uint16_t time_timeout(time_up_cb cb, void* arg);                // cb after ms; or 1 tick if timeout=0
void     time_start_timer(uint16_t id, uint32_t timeout);
void     time_stop_timer(uint16_t id);
void     time_end();

#if defined(NRF5)

void time_delay_ms(uint32_t ms);
void time_delay_us(uint32_t us);
void time_delay_ns(uint32_t ns);

#else

 #include <unistd.h>
 #include <time.h>

 #define time_delay_ms(m) usleep((m) * 1000)
 #define time_delay_us(u) usleep((u)       )
 #define time_delay_ns(n) usleep((n) / 1000)

#endif

#endif
