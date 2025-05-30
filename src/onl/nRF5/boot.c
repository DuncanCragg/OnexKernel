
#include <nrf_nvic.h>
#include <nrf_pwr_mgmt.h>
#include <nrf_bootloader_info.h>
#include <nrf_soc.h>
#include <onex-kernel/log.h>
#include <onex-kernel/boot.h>

void boot_init() {

  uint32_t reset_reason = NRF_POWER->RESETREAS;
  NRF_POWER->RESETREAS = reset_reason;  // clear by writing back

  if(reset_reason & POWER_RESETREAS_DOG_Msk) {
    log_write("\n"); log_write("\n");
    log_write("Watchdog reset!\n");
  }
  if(reset_reason & POWER_RESETREAS_LOCKUP_Msk) {
    log_write("\n"); log_write("\n");
    log_write("CPU lock-up reset!\n");
  }
  if(reset_reason & POWER_RESETREAS_SREQ_Msk) {
    log_write("\n"); log_write("\n");
    log_write("Soft reset\n");
  }
  if(reset_reason & POWER_RESETREAS_OFF_Msk) {
    log_write("\n"); log_write("\n");
    log_write("System OFF reset\n");
  }
  if(reset_reason & POWER_RESETREAS_LPCOMP_Msk) {
    log_write("\n"); log_write("\n");
    log_write("LPCOMP reset\n");
  }
  if(reset_reason & POWER_RESETREAS_NFC_Msk) {
    log_write("\n"); log_write("\n");
    log_write("NFC field detected reset\n");
  }
  if(reset_reason & POWER_RESETREAS_DIF_Msk) {
    log_write("\n"); log_write("\n");
    log_write("Debug reset\n");
  }

  // see errata [88] WDT: Increased current consumption when configured to pause in System ON idle
  NRF_WDT->CONFIG = NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Run  << WDT_CONFIG_SLEEP_Pos) |
                                      (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos );
  NRF_WDT->CRV = (5*32768); // 5s
  NRF_WDT->RREN |= WDT_RREN_RR0_Msk;
  NRF_WDT->TASKS_START = 1;

#if defined(SOFTDEVICE_PRESENT)
  sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
#endif
}

void boot_reset(bool enter_bootloader) {
  NRF_POWER->GPREGRET = enter_bootloader? 0x57: 0x6D;
  NVIC_SystemReset();
}

void boot_feed_watchdog()
{
  NRF_WDT->RR[0] = WDT_RR_RR_Reload;
}

void boot_dfu_start()
{
#if defined(SOFTDEVICE_PRESENT)
  sd_power_gpregret_clr(0, 0xffffffff);
  sd_power_gpregret_set(0, BOOTLOADER_DFU_START);
  nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_DFU);
#endif
}

static uint64_t running_time=0;
static uint64_t sleeping_time=0;
static uint64_t last_running_time=0;
static uint64_t last_sleeping_time=0;
static uint64_t dt=0;
static uint64_t cpu_calc_time=0;
static uint8_t  cpu_percent=0;

void boot_sleep()
{
  uint64_t ct;

  ct=time_ms();
  if(dt) running_time+=(ct-dt);
  dt=ct;

  nrf_pwr_mgmt_run();

  ct=time_ms();
  sleeping_time+=(ct-dt);
  dt=ct;

  if(ct>cpu_calc_time){
    cpu_calc_time=ct+997;

    uint64_t running_time_diff =(running_time -last_running_time);
    uint64_t sleeping_time_diff=(sleeping_time-last_sleeping_time);
    last_running_time=running_time;
    last_sleeping_time=sleeping_time;

    cpu_percent=(uint8_t)(100*running_time_diff/(running_time_diff+sleeping_time_diff));
  }
}

uint8_t boot_cpu()
{
  return cpu_percent;
}
