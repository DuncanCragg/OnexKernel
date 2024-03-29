
#include <stdio.h>
#include <stdarg.h>

#include "app_error.h"
#include "nrf_log_default_backends.h"

#if defined(LOG_TO_SERIAL)
#include <onex-kernel/serial.h>
#endif
#include <onex-kernel/time.h>
#include <onex-kernel/log.h>

void log_init()
{
#if defined(NRF_LOG_ENABLED)
  NRF_LOG_INIT(NULL);
  NRF_LOG_DEFAULT_BACKENDS_INIT();
#endif
}

bool log_loop()
{
#if defined(NRF_LOG_ENABLED)
  return NRF_LOG_PROCESS();
#else
  return false;
#endif
}

#if defined(LOG_TO_GFX) || defined(LOG_TO_RTT)
#define LOG_BUF_SIZE 1024
static volatile char log_buffer[LOG_BUF_SIZE];
#if defined(LOG_TO_GFX)
volatile char* event_log_buffer=0;
#endif
#endif

int log_write_current_file_line(char* file, uint32_t line, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  int r=0;
#if defined(LOG_TO_SERIAL)
//size_t n=snprintf((char*)log_buffer, LOG_BUF_SIZE, "LOG: %s", fmt);
//if(n>=LOG_BUF_SIZE) n=LOG_BUF_SIZE-1;
  r=serial_vprintf(fmt, args);
  time_delay_ms(1);
#elif defined(LOG_TO_GFX)
  uint16_t ln=file? snprintf((char*)log_buffer, LOG_BUF_SIZE, "%s:%ld:", file, line): 0;
  vsnprintf((char*)log_buffer+ln, LOG_BUF_SIZE-ln, fmt, args);
  event_log_buffer=log_buffer;
#elif defined(LOG_TO_RTT)
  vsnprintf((char*)log_buffer, LOG_BUF_SIZE, fmt, args);
  NRF_LOG_DEBUG("%s", (char*)log_buffer);
  time_delay_ms(2);
  NRF_LOG_FLUSH();
  time_delay_ms(2);
#endif
  va_end(args);
  return r;
}

void log_flush()
{
#if defined(NRF_LOG_ENABLED)
  NRF_LOG_FLUSH();
  time_delay_ms(5);
#endif
}


