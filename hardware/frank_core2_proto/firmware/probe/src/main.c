/* Throwaway bisect probe: minimum firmware that proves USB stdio works
 * on this part at this clock, then adds one suspect stage at a time. */
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include <stdio.h>

int main(void) {
    vreg_set_voltage(CPU_VOLTAGE);
    sleep_ms(10);
    bool clk_ok = set_sys_clock_khz(CPU_CLOCK_MHZ * 1000, false);

    stdio_init_all();

    for (int i = 0;; i++) {
        printf("[probe %d] clk_req=%d ok=%d actual=%u MHz\n",
               i, CPU_CLOCK_MHZ, (int)clk_ok,
               (unsigned)(clock_get_hz(clk_sys) / 1000000u));
        sleep_ms(1000);
    }
}
