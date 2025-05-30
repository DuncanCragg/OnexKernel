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

#define TOUCH_ACTION_NONE    0x00 // none
#define TOUCH_ACTION_DOWN    0x01 // up!
#define TOUCH_ACTION_UP      0x02 // up
#define TOUCH_ACTION_CONTACT 0x03 // down

extern char* touch_gestures[];
extern char* touch_actions[];

typedef struct touch_info_t {
  uint16_t x;
  uint16_t y;
  uint8_t  gesture;
  uint8_t  action;
} touch_info_t;

typedef void (*touch_touched_cb)(touch_info_t);

void         touch_init(touch_touched_cb);
touch_info_t touch_get_info();
void         touch_disable();
void         touch_enable();
void         touch_reset(uint8_t delay);
void         touch_sleep();
void         touch_wake();
void         touch_dump();

#endif

