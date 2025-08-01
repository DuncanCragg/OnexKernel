
#include <string.h>

#include <onex-kernel/config.h>
#include <onex-kernel/log.h>
#include <onex-kernel/time.h>
#include <onex-kernel/random.h>
#include <onn.h>

static bool button_pressed=false;

static void every_tick(void*){
  button_pressed=!button_pressed;
  onex_run_evaluators("uid-button", (void*)button_pressed);
}

bool evaluate_button(object* button, void* pressed) {
  char* s=(char*)(pressed? "down": "up");
  object_property_set(button, "state", s);
  log_write("evaluate_button: "); object_log(button);
  return true;
}

int main(int argc, char *argv[]) {

  properties* config = get_config(argc, argv, "button", "log-onp");
  if(!config) return -1;

  time_init();
  log_init(config);
  random_init();

  onex_init(config);

  log_write("\n------Starting Button Test Server-----\n");

  onex_set_evaluators("eval_button", evaluate_button, 0);
  object* button=object_new("uid-button", "eval_button", "button", 4);
  object_property_set(button, "state", "up");
  onex_run_evaluators("uid-button", 0);

  time_ticker(every_tick, 0, 2000);

  while(1){
    if(!onex_loop()){
      time_delay_ms(5);
    }
  }
}

