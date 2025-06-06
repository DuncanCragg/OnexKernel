
#include <boards.h>

#include <onex-kernel/time.h>
#include <onex-kernel/log.h>
#include <onex-kernel/gpio.h>
#include <onex-kernel/i2c.h>
#include <onex-kernel/touch.h>

#define HYN_REG_POWER_MODE       0xA5
#define HYN_REG_POWER_MODE_SLEEP 0x03

#define TOUCH_GESTURE 1
#define TOUCH_ACTION 3
#define TOUCH_X_HIGH 3
#define TOUCH_X_LOW 4
#define TOUCH_Y_HIGH 5
#define TOUCH_Y_LOW 6

char* touch_gestures[]={ "none", "down", "up", "left", "right", "tap", "", "", "", "", "", "double", "long", };
char* touch_actions[]={ "none", "up", "up", "down" };

static touch_touched_cb touch_cb = 0;

static void* twip;

static touch_info_t ti={0};

static void touched(uint8_t pin, uint8_t type) {
  touch_get_info();
  if(touch_cb) touch_cb(ti);
}

void touch_init(touch_touched_cb cb)
{
  touch_cb = cb;

  nrf_gpio_cfg_output(TOUCH_RESET_PIN);
  nrf_gpio_pin_set(TOUCH_RESET_PIN);

  twip=i2c_init(400);

  gpio_mode_cb(TOUCH_IRQ_PIN, INPUT_PULLUP, FALLING, touched);
}

static bool pressed=false;

touch_info_t touch_get_info()
{
  uint8_t e;
  uint8_t xyga[9]={0};
  e=i2c_read(twip, TOUCH_ADDRESS, xyga, sizeof(xyga));
  if(e) return ti;

  ti.x = (xyga[TOUCH_X_HIGH] & 0x0f) << 8 | xyga[TOUCH_X_LOW];
  ti.y = (xyga[TOUCH_Y_HIGH] & 0x0f) << 8 | xyga[TOUCH_Y_LOW];

  ti.gesture = xyga[TOUCH_GESTURE];
  ti.action = (xyga[TOUCH_ACTION] >> 6)+1;

  return ti;
}

void touch_disable(){
  NVIC_DisableIRQ(GPIOTE_IRQn);
}

void touch_enable(){
  NVIC_EnableIRQ(GPIOTE_IRQn);
}

void touch_reset(uint8_t delay)
{
  nrf_gpio_pin_clear(TOUCH_RESET_PIN);
  time_delay_ms(delay);
  nrf_gpio_pin_set(TOUCH_RESET_PIN);
}

void touch_sleep() {

  touch_reset(5);
  time_delay_ms(50);

  i2c_write_register_byte(twip, TOUCH_ADDRESS, HYN_REG_POWER_MODE, HYN_REG_POWER_MODE_SLEEP);
}

void touch_wake() {

  touch_reset(5);
  time_delay_ms(50);
}

void touch_dump(touch_info_t ti)
{
  log_write("touch %d %d %s %s\n", ti.x, ti.y, touch_actions[ti.action], touch_gestures[ti.gesture]);
}

