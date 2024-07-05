//#define USE_PIO

#include <stdio.h>
#include "pico/stdlib.h"
#ifdef USE_PIO
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "gamepad.pio.h"

//#define GAMEPAD_AUTO

#define GAMEPAD_PIO pio0
#define GAMEPAD_SM  0
#endif

const char *BUTTONS[] = { "Right", "Left", "Down", "Up", "Start", "Select", "B", "A" };

#ifdef USE_PIO
static void gamepad_init(PIO pio, uint sm, uint8_t latch_pin, uint8_t data_pin) {
    static uint8_t offsets[2] = { 0xFF, 0xFF };

    pio_sm_config c;
    uint8_t offset;

    pio_gpio_init(pio, latch_pin);
    pio_gpio_init(pio, latch_pin + 1); // clock
    pio_gpio_init(pio, data_pin);
    pio_sm_set_consecutive_pindirs(pio, sm, latch_pin, 2, true);
    pio_sm_set_consecutive_pindirs(pio, sm, data_pin, 1, false);
    offset = offsets[pio_get_index(pio)];
    if (offset == 0xFF) {
#ifdef GAMEPAD_AUTO
        offset = pio_add_program(pio, &gamepad_read_auto_program);
#else
        offset = pio_add_program(pio, &gamepad_read_program);
#endif
        offsets[pio_get_index(pio)] = offset;
    }
#ifdef GAMEPAD_AUTO
    c = gamepad_read_auto_program_get_default_config(offset);
#else
    c = gamepad_read_program_get_default_config(offset);
#endif
    sm_config_set_set_pins(&c, latch_pin, 2);
    sm_config_set_in_pins(&c, data_pin);
#ifdef GAMEPAD_AUTO
    sm_config_set_in_shift(&c, false, false, 8);
#else
    sm_config_set_in_shift(&c, false, true, 8);
#endif
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / 1000000U);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

#ifdef GAMEPAD_AUTO
static inline bool gamepad_changed(PIO pio, uint sm) {
    return ! pio_sm_is_rx_fifo_empty(pio, sm);
}
#endif

static uint8_t gamepad_read(PIO pio, uint sm) {
#ifndef GAMEPAD_AUTO
    pio_sm_put_blocking(pio, sm, 0);
#endif
    return pio_sm_get_blocking(pio, sm);
}

#else
static void gamepad_init(uint8_t latch_pin, uint8_t clock_pin, uint8_t data_pin) {
    gpio_init_mask((1 << latch_pin) | (1 << clock_pin) | (1 << data_pin));
    gpio_set_dir_out_masked((1 << latch_pin) | (1 << clock_pin));
    gpio_put_masked((1 << latch_pin) | (1 << clock_pin), 0);
    gpio_set_dir(data_pin, false);
}

static uint8_t gamepad_read(uint8_t latch_pin, uint8_t clock_pin, uint8_t data_pin) {
    uint8_t result = 0;

    gpio_put(latch_pin, 1);
    sleep_us(12);
    gpio_put(latch_pin, 0);
    sleep_us(12);
    result = gpio_get(data_pin);
    for (uint8_t i = 0; i < 7; ++i) {
        gpio_put(clock_pin, 1);
        sleep_us(12);
        gpio_put(clock_pin, 0);
        sleep_us(12);
        result = (result << 1) | gpio_get(data_pin);
    }
    return result;
}
#endif

int main() {
    uint8_t last_data;

    stdio_init_all();

#ifdef USE_PIO
    gamepad_init(GAMEPAD_PIO, GAMEPAD_SM, 28, 27);
#else
    gamepad_init(28, 29, 27);
#endif

#ifdef USE_PIO
    last_data = gamepad_read(GAMEPAD_PIO, GAMEPAD_SM);
#else
    last_data = gamepad_read(28, 29, 27);
#endif

    while (true) {
        uint8_t data;

#ifdef GAMEPAD_AUTO
        if (gamepad_changed(GAMEPAD_PIO, GAMEPAD_SM)) {
            data = gamepad_read(GAMEPAD_PIO, GAMEPAD_SM);
#else
#ifdef USE_PIO
        data = gamepad_read(GAMEPAD_PIO, GAMEPAD_SM);
#else
        data = gamepad_read(28, 29, 27);
#endif
        if (data != last_data) {
#endif
            for (uint8_t i = 0; i < 8; ++i) {
                if ((data & (1 << i)) != (last_data & (1 << i))) {
                    printf(BUTTONS[i]);
                    if (data & (1 << i))
                        printf(" released\n");
                    else
                        printf(" pressed\n");
                }
            }
            last_data = data;
        }
    }
}
