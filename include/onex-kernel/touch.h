#ifndef TOUCH_H
#define TOUCH_H

#define TOUCH_GESTURE_NONE       0x00
#define TOUCH_GESTURE_DOWN       0x01
#define TOUCH_GESTURE_UP         0x02
#define TOUCH_GESTURE_LEFT       0x03
#define TOUCH_GESTURE_RIGHT      0x04
#define TOUCH_GESTURE_TAP        0x05
#define TOUCH_GESTURE_TAP_DOUBLE 0x0b
#define TOUCH_GESTURE_TAP_LONG   0x0c

#define TOUCH_ACTION_DOWN    0
#define TOUCH_ACTION_UP      1
#define TOUCH_ACTION_CONTACT 2

typedef struct touch_info_t {
  uint16_t x;
  uint16_t y;
  uint8_t  gesture;
  uint8_t  action;
} touch_info_t;

typedef void (*touch_touched_cb)(touch_info_t);

void         touch_reset();
void         touch_init(touch_touched_cb);
touch_info_t touch_get_info();
void         touch_disable();

#endif
