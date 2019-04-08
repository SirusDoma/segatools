#include <windows.h>
#include <xinput.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "amex/jvs.h"

#include "board/io3.h"

#include "idzhook/jvs.h"

#include "jvs/jvs-bus.h"

#include "util/dprintf.h"

static void idz_jvs_read_switches(void *ctx, struct io3_switch_state *out);
static uint16_t idz_jvs_read_analog(void *ctx, uint8_t analog_no);
static uint16_t idz_jvs_consume_coins(void *ctx, uint8_t slot_no);

static const struct io3_ops idz_jvs_io3_ops = {
    .read_switches  = idz_jvs_read_switches,
    .read_analog    = idz_jvs_read_analog,
    .consume_coins  = idz_jvs_consume_coins,
};

static struct io3 idz_jvs_io3;
static bool idz_jvs_coin;
static bool idz_jvs_shifting;
static uint8_t idz_jvs_gear;

static const uint16_t idz_jvs_gear_signals[] = {
    /* Neutral */
    0x0000,
    /* 1: Left|Up */
    0x2800,
    /* 2: Left|Down */
    0x1800,
    /* 3: Up */
    0x2000,
    /* 4: Down */
    0x1000,
    /* 5: Right|Up */
    0x2400,
    /* 6: Right|Down */
    0x1400,
};

void idz_jvs_init(void)
{
    io3_init(&idz_jvs_io3, NULL, &idz_jvs_io3_ops, NULL);
    jvs_attach(&idz_jvs_io3.jvs);
}

static void idz_jvs_read_switches(void *ctx, struct io3_switch_state *out)
{
    bool shift_inc;
    bool shift_dec;
    XINPUT_STATE xi;
    WORD xb;

    assert(out != NULL);

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);
    xb = xi.Gamepad.wButtons;

    /* Update gameplay buttons */

    if (xb & XINPUT_GAMEPAD_START) {
        out->p1 |= 1 << 15;
        idz_jvs_gear = 0; /* Reset to Neutral when start is pressed */
    }

    if (xb & XINPUT_GAMEPAD_DPAD_UP) {
        out->p1 |= 1 << 13;
    }

    if (xb & XINPUT_GAMEPAD_DPAD_DOWN) {
        out->p1 |= 1 << 12;
    }

    if (xb & XINPUT_GAMEPAD_DPAD_LEFT) {
        out->p1 |= 1 << 11;
    }

    if (xb & XINPUT_GAMEPAD_DPAD_RIGHT) {
        out->p1 |= 1 << 10;
    }

    if (xb & XINPUT_GAMEPAD_BACK) {
        out->p1 |= 1 << 9;
    }

    /* Update simulated six-speed shifter */

    shift_inc = xb & XINPUT_GAMEPAD_X;
    shift_dec = xb & XINPUT_GAMEPAD_Y;

    if (!idz_jvs_shifting) {
        if (shift_inc && idz_jvs_gear < 6) {
            idz_jvs_gear++;
        }

        if (shift_dec && idz_jvs_gear > 0) {
            idz_jvs_gear--;
        }
    }

    idz_jvs_shifting = shift_inc || shift_dec;
    out->p2 = idz_jvs_gear_signals[idz_jvs_gear];

    /* Update test/service buttons */

    if (GetAsyncKeyState('1')) {
        out->system = 0x80;
    } else {
        out->system = 0;
    }

    if (GetAsyncKeyState('2')) {
        out->p1 |= 1 << 14;
    }
}

static uint16_t idz_jvs_read_analog(void *ctx, uint8_t analog_no)
{
    XINPUT_STATE xi;
    int tmp;

    if (analog_no > 2) {
        return 0;
    }

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);

    switch (analog_no) {
    case 0:
        /* Wheel */
        tmp = xi.Gamepad.sThumbLX;

        if (abs(tmp) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
            return tmp + 0x8000;
        } else {
            return 0x8000;
        }

    case 1:
        /* Accel */
        return xi.Gamepad.bRightTrigger << 8;

    case 2:
        /* Brake */
        return xi.Gamepad.bLeftTrigger << 8;

    default:
        return 0;
    }
}

static uint16_t idz_jvs_consume_coins(void *ctx, uint8_t slot_no)
{
    if (slot_no > 0) {
        return 0;
    }

    if (GetAsyncKeyState('3')) {
        if (idz_jvs_coin) {
            return 0;
        } else {
            dprintf("IDZero JVS: Coin drop\n");
            idz_jvs_coin = true;

            return 1;
        }
    } else {
        idz_jvs_coin = false;

        return 0;
    }
}
