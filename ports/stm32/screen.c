#include <stdio.h>
#include <string.h>

#include "py/mphal.h"
#include "py/runtime.h"

#if MICROPY_HW_HAS_SCREEN

#include "pin.h"
#include "bufhelper.h"
#include "spi.h"
#include "font_petme128_8x8.h"
#include "screen.h"

#define LCD_INSTR (0)
#define LCD_CHAR_BUF_W (16)
#define LCD_CHAR_BUF_H (4)

#define COL0(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
#define COL(c) COL0((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff)

const uint16_t palette[] = {
    COL(0x000000), // 0
    COL(0xffffff), // 1
    COL(0xff2121), // 2
    COL(0xff93c4), // 3
    COL(0xff8135), // 4
    COL(0xfff609), // 5
    COL(0x249ca3), // 6
    COL(0x78dc52), // 7
    COL(0x003fad), // 8
    COL(0x87f2ff), // 9
    COL(0x8e2ec4), // 10
    COL(0xa4839f), // 11
    COL(0x5c406c), // 12
    COL(0xe5cdc4), // 13
    COL(0x91463d), // 14
    COL(0x000000), // 15
};

uint8_t fb[168 * 128]; // only for palette


typedef struct _pyb_screen_obj_t {
    mp_obj_base_t base;

    // hardware control for the LCD
    const spi_t *spi;
    const pin_obj_t *pin_cs1;
    const pin_obj_t *pin_rst;
    const pin_obj_t *pin_a0;
    const pin_obj_t *pin_bl;

    // character buffer for stdout-like output
    char char_buffer[LCD_CHAR_BUF_W * LCD_CHAR_BUF_H];
    int line;
    int column;
    int next_line;

} pyb_screen_obj_t;


/// \classmethod \constructor(skin_position)
///
/// Construct an LCD object in the given skin position.  `skin_position` can be 'X' or 'Y', and
/// should match the position where the LCD pyskin is plugged in.
STATIC mp_obj_t pyb_screen_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {

    // create screen object
    pyb_screen_obj_t *screen = m_new_obj(pyb_screen_obj_t);

    return MP_OBJ_FROM_PTR(screen);
}


STATIC const mp_rom_map_elem_t pyb_screen_locals_dict_table[] = {
    // instance methods
};

STATIC MP_DEFINE_CONST_DICT(pyb_screen_locals_dict, pyb_screen_locals_dict_table);

const mp_obj_type_t pyb_screen_type = {
    { &mp_type_type },
    .name = MP_QSTR_SCREEN,
    .make_new = pyb_screen_make_new,
    .locals_dict = (mp_obj_dict_t*)&pyb_screen_locals_dict,
};

#endif // MICROPY_HW_HAS_SCREEN
