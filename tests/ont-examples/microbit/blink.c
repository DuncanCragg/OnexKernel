// --------------------------------------------------------------------

#include <boards.h>

#include <onex-kernel/log.h>
#include <onex-kernel/gpio.h>
#include <onex-kernel/time.h>
#include <onex-kernel/serial.h>
#include <onex-kernel/random.h>

static uint16_t speed = 128;

static void serial_received(unsigned char* chars, size_t size)
{
  if(!size) return;
  if(chars[0]=='o') speed/=2;
  if(chars[0]=='i') speed*=2;
  if(!speed)  speed=1;
}

const uint8_t leds_list[LEDS_NUMBER] = LEDS_LIST;

static void loop_serial(void*){ serial_loop(); }

int main()
{
  time_init();
  log_init();
  random_init();
  serial_init((serial_recv_cb)serial_received,0);

  gpio_init();

  for(uint8_t l=0; l< LEDS_NUMBER; l++) gpio_mode(leds_list[l], OUTPUT);
  for(uint8_t l=0; l< LEDS_NUMBER; l++) gpio_set( leds_list[l], !LEDS_ACTIVE_STATE);

  time_ticker(loop_serial, 0, 1);

  serial_printf("Type 'o' or 'i'\n");

  while(1){

    serial_printf("%dms %d %d\n", time_ms(), speed, random_ish_byte());

    gpio_set(leds_list[3], 0);
    gpio_set(leds_list[10], 0);

    time_delay_ms(speed);

    gpio_set(leds_list[3], 1);
    gpio_set(leds_list[10], 1);

    time_delay_ms(speed);
/*
    for(uint8_t l=0; l<LEDS_NUMBER; l++){ gpio_toggle(leds_list[l]); time_delay_ms(speed); }

    for(uint8_t r=0; r<LEDS_NUMBER; r++){
      gpio_set(leds_list[r], 0);
      for(uint8_t c=0; c<LEDS_NUMBER; c++){
        serial_printf("row %d col %d\n", r, c);
        gpio_set(leds_list[c], 0);
        time_delay_ms(speed);
        gpio_set(leds_list[c], 1);
      }
      gpio_set(leds_list[r], 1);
    }
*/
  }
}

// --------------------------------------------------------------------
