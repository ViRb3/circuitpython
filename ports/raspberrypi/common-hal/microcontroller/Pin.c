/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Scott Shawcroft for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/runtime.h"

#include "common-hal/microcontroller/__init__.h"
#include "shared-bindings/microcontroller/Pin.h"

#include "src/rp2_common/hardware_gpio/include/hardware/gpio.h"

#if CIRCUITPY_CYW43
#include "bindings/cyw43/__init__.h"
#include "pico/cyw43_arch.h"

bool cyw_ever_init;
static uint32_t cyw_pin_claimed;

void reset_pin_number_cyw(uint8_t pin_no) {
    cyw_pin_claimed &= ~(1 << pin_no);
}
#endif

STATIC uint32_t never_reset_pins;

void reset_all_pins(void) {
    for (size_t i = 0; i < TOTAL_GPIO_COUNT; i++) {
        if ((never_reset_pins & (1 << i)) != 0) {
            continue;
        }
        reset_pin_number(i);
    }
    #if CIRCUITPY_CYW43
    if (cyw_ever_init) {
        for (size_t i = 0; i < 1; i++) {
            cyw43_arch_gpio_put(i, 0);
        }
    }
    cyw_pin_claimed = 0;
    #endif
}

void never_reset_pin_number(uint8_t pin_number) {
    if (pin_number >= TOTAL_GPIO_COUNT) {
        return;
    }

    never_reset_pins |= 1 << pin_number;
}

void reset_pin_number(uint8_t pin_number) {
    if (pin_number >= TOTAL_GPIO_COUNT) {
        return;
    }

    never_reset_pins &= ~(1 << pin_number);

    // We are very aggressive in shutting down the pad fully. Both pulls are
    // disabled and both buffers are as well.
    gpio_init(pin_number);
    hw_clear_bits(&padsbank0_hw->io[pin_number], PADS_BANK0_GPIO0_IE_BITS |
        PADS_BANK0_GPIO0_PUE_BITS |
        PADS_BANK0_GPIO0_PDE_BITS);
    hw_set_bits(&padsbank0_hw->io[pin_number], PADS_BANK0_GPIO0_OD_BITS);
}

void common_hal_never_reset_pin(const mcu_pin_obj_t *pin) {
    never_reset_pin_number(pin->number);
}

void common_hal_reset_pin(const mcu_pin_obj_t *pin) {
    #if CIRCUITPY_CYW43
    if (pin->base.type == &cyw43_pin_type) {
        reset_pin_number_cyw(pin->number);
        return;
    }
    #endif
    reset_pin_number(pin->number);
}

void claim_pin(const mcu_pin_obj_t *pin) {
    #if CIRCUITPY_CYW43
    if (pin->base.type == &cyw43_pin_type) {
        cyw_pin_claimed |= (1 << pin->number);
    }
    #endif
    // Nothing to do because all changes will set the GPIO settings.
}

bool pin_number_is_free(uint8_t pin_number) {
    if (pin_number >= TOTAL_GPIO_COUNT) {
        return false;
    }

    uint32_t pad_state = padsbank0_hw->io[pin_number];
    return (pad_state & PADS_BANK0_GPIO0_IE_BITS) == 0 &&
           (pad_state & PADS_BANK0_GPIO0_OD_BITS) != 0;
}

bool common_hal_mcu_pin_is_free(const mcu_pin_obj_t *pin) {
    #if CIRCUITPY_CYW43
    if (pin->base.type == &cyw43_pin_type) {
        return !(cyw_pin_claimed & (1 << pin->number));
    }
    #endif
    return pin_number_is_free(pin->number);
}

uint8_t common_hal_mcu_pin_number(const mcu_pin_obj_t *pin) {
    return pin->number;
}

void common_hal_mcu_pin_claim(const mcu_pin_obj_t *pin) {
    return claim_pin(pin);
}

void common_hal_mcu_pin_reset_number(uint8_t pin_no) {
    reset_pin_number(pin_no);
}
