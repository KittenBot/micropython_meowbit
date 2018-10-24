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
    const pin_obj_t *pin_dc;
    const pin_obj_t *pin_bl;

    // character buffer for stdout-like output
    char char_buffer[LCD_CHAR_BUF_W * LCD_CHAR_BUF_H];
    int line;
    int column;
    int next_line;

} pyb_screen_obj_t;

STATIC void screen_delay(void) {
    __asm volatile ("nop\nnop");
}

STATIC void screen_out(pyb_screen_obj_t *screen, int instr_data, uint8_t * buf, uint8_t len) {
    screen_delay();
    mp_hal_pin_low(screen->pin_cs1); // CS=0; enable
    if (instr_data == LCD_INSTR) {
        mp_hal_pin_low(screen->pin_dc); // A0=0; select instr reg
    } else {
        mp_hal_pin_high(screen->pin_dc); // A0=1; select data reg
    }
    screen_delay();
    HAL_SPI_Transmit(screen->spi->spi, buf, len, 1000);
    screen_delay();
    mp_hal_pin_high(screen->pin_cs1); // CS=1; disable
}


/// \classmethod \constructor(skin_position)
///
/// Construct an LCD object in the given skin position.  `skin_position` can be 'X' or 'Y', and
/// should match the position where the LCD pyskin is plugged in.
STATIC mp_obj_t pyb_screen_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    // check arguments
    mp_arg_check_num(n_args, n_kw, 1, 1, false);
    // create screen object
    pyb_screen_obj_t *screen = m_new_obj(pyb_screen_obj_t);
    screen->base.type = &pyb_screen_type;

    // configure pins, tft bind to spi2 on f4
    screen->spi = &spi_obj[1];
    screen->pin_cs1 = pyb_pin_PB12;
    screen->pin_rst = pyb_pin_PC4;
    screen->pin_dc = pyb_pin_PC5;
    screen->pin_bl = pyb_pin_PA8; // todo: remove back light in next hardware interation

    // init the SPI bus
    SPI_InitTypeDef *init = &screen->spi->spi->Init;
    init->Mode = SPI_MODE_MASTER;

    // compute the baudrate prescaler from the desired baudrate
    uint spi_clock;
    // SPI2 and SPI3 are on APB1
    spi_clock = HAL_RCC_GetPCLK1Freq();

    uint br_prescale = spi_clock / 16000000; // datasheet says LCD can run at 20MHz, but we go for 16MHz
    if (br_prescale <= 2) { init->BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2; }
    else if (br_prescale <= 4) { init->BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4; }
    else if (br_prescale <= 8) { init->BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; }
    else if (br_prescale <= 16) { init->BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; }
    else if (br_prescale <= 32) { init->BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32; }
    else if (br_prescale <= 64) { init->BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64; }
    else if (br_prescale <= 128) { init->BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128; }
    else { init->BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; }

    // data is sent bigendian, latches on rising clock
    init->CLKPolarity = SPI_POLARITY_HIGH;
    init->CLKPhase = SPI_PHASE_2EDGE;
    init->Direction = SPI_DIRECTION_2LINES;
    init->DataSize = SPI_DATASIZE_8BIT;
    init->NSS = SPI_NSS_SOFT;
    init->FirstBit = SPI_FIRSTBIT_MSB;
    init->TIMode = SPI_TIMODE_DISABLED;
    init->CRCCalculation = SPI_CRCCALCULATION_DISABLED;
    init->CRCPolynomial = 0;

    // init the SPI bus
    spi_init(screen->spi, false);
    // set the pins to default values
    mp_hal_pin_high(screen->pin_cs1);
    mp_hal_pin_high(screen->pin_rst);
    mp_hal_pin_high(screen->pin_dc);
    mp_hal_pin_low(screen->pin_bl);

    // init the pins to be push/pull outputs
    mp_hal_pin_output(screen->pin_cs1);
    mp_hal_pin_output(screen->pin_rst);
    mp_hal_pin_output(screen->pin_dc);
    mp_hal_pin_output(screen->pin_bl);
    // init the LCD
    mp_hal_delay_ms(1); // wait a bit
    mp_hal_pin_low(screen->pin_rst); // RST=0; reset
    mp_hal_delay_ms(1); // wait for reset; 2us min
    mp_hal_pin_high(screen->pin_rst); // RST=1; enable
    mp_hal_delay_ms(1); // wait for reset; 2us min
    // set backlight
    mp_hal_pin_high(screen->pin_bl);

    uint8_t tmp[16];
    tmp[0] = ST7735_SWRESET;
    screen_out(screen, LCD_INSTR, tmp, 1);
    mp_hal_delay_ms(100);
    tmp[0] = ST7735_SLPOUT;
    screen_out(screen, LCD_INSTR, tmp, 1);
    mp_hal_delay_ms(100);

    tmp[0] = ST7735_INVOFF;
    screen_out(screen, LCD_INSTR, tmp, 1);

    tmp[0] = ST7735_COLMOD;
    tmp[1] = 0x05;
    screen_out(screen, LCD_INSTR, tmp, 1);

    uint8_t tmp2[] ={
     ST7735_GMCTRP1,
      0x02, 0x1c, 0x07, 0x12,
      0x37, 0x32, 0x29, 0x2d,
      0x29, 0x25, 0x2B, 0x39,
      0x00, 0x01, 0x03, 0x10,
    };
    screen_out(screen, LCD_INSTR, tmp2, 17);

    uint8_t tmp3[] ={
     ST7735_GMCTRN1,
      0x03, 0x1d, 0x07, 0x06,
      0x2E, 0x2C, 0x29, 0x2D,
      0x2E, 0x2E, 0x37, 0x3F,
      0x00, 0x00, 0x02, 0x10,
    };
    screen_out(screen, LCD_INSTR, tmp3, 17);


    tmp[0] = ST7735_NORON;
    screen_out(screen, LCD_INSTR, tmp, 1);
    mp_hal_delay_ms(10);
    tmp[0] = ST7735_DISPON;
    screen_out(screen, LCD_INSTR, tmp, 1);
    mp_hal_delay_ms(10);


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
