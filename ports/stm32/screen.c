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

// uint8_t fb[DISPLAY_WIDTH * DISPLAY_HEIGHT]; // only for palette


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

STATIC void send_cmd(pyb_screen_obj_t *screen, uint8_t * buf, uint8_t len) {
    screen_delay();
    mp_hal_pin_low(screen->pin_cs1); // CS=0; enable
    mp_hal_pin_low(screen->pin_dc); // DC=0 for instruction
    screen_delay();
    HAL_SPI_Transmit(screen->spi->spi, buf, 1, 1000);
    //printf("cmd 0x%x\n", buf[0]);
    mp_hal_pin_high(screen->pin_dc);
    screen_delay();
    len--;
    buf++;
    if (len > 0){
        HAL_SPI_Transmit(screen->spi->spi, buf, len, 1000);
        //printf("v 0x%x len %d\n", buf[0], len);
    }
    mp_hal_pin_high(screen->pin_cs1); // CS=1; disable
}

/*
STATIC void draw_screen(pyb_screen_obj_t *screen){
    uint8_t cmdBuf[] = {ST7735_RAMWR};
    send_cmd(screen, cmdBuf, 1);

    uint8_t *p = fb;
    uint8_t colorBuf[2];

    mp_hal_pin_low(screen->pin_cs1); // CS=0; enable
    mp_hal_pin_high(screen->pin_dc); // DC=1
    for (int i = 0; i < DISPLAY_WIDTH; ++i) {
        for (int j = 0; j < DISPLAY_HEIGHT; ++j) {
            uint16_t color = palette[*p++ & 0xf];
            colorBuf[0] = color >> 8;
            colorBuf[1] = color & 0xff;
            HAL_SPI_Transmit(screen->spi->spi, colorBuf, 2, 1000);
        }
    }
    mp_hal_pin_high(screen->pin_cs1); // CS=1; disable

}
*/

/// \classmethod \constructor(skin_position)
///
/// Construct an LCD object in the given skin position.  `skin_position` can be 'X' or 'Y', and
/// should match the position where the LCD pyskin is plugged in.
STATIC mp_obj_t pyb_screen_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    // check arguments
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
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
    // uint spi_clock;
    // SPI2 and SPI3 are on APB1
    // spi_clock = HAL_RCC_GetPCLK1Freq();

    // data is sent bigendian, latches on rising clock
    init->BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
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
    send_cmd(screen, tmp, 1);
    mp_hal_delay_ms(100);
    tmp[0] = ST7735_SLPOUT;
    send_cmd(screen, tmp, 1);
    mp_hal_delay_ms(100);

    tmp[0] = ST7735_INVOFF;
    send_cmd(screen, tmp, 1);

    tmp[0] = ST7735_COLMOD;
    tmp[1] = 0x05;
    send_cmd(screen, tmp, 2);

    uint8_t tmp2[] ={
     ST7735_GMCTRP1,
      0x02, 0x1c, 0x07, 0x12,
      0x37, 0x32, 0x29, 0x2d,
      0x29, 0x25, 0x2B, 0x39,
      0x00, 0x01, 0x03, 0x10,
    };
    send_cmd(screen, tmp2, 17);

    uint8_t tmp3[] ={
     ST7735_GMCTRN1,
      0x03, 0x1d, 0x07, 0x06,
      0x2E, 0x2C, 0x29, 0x2D,
      0x2E, 0x2E, 0x37, 0x3F,
      0x00, 0x00, 0x02, 0x10,
    };
    send_cmd(screen, tmp3, 17);

    tmp[0] = ST7735_NORON;
    send_cmd(screen, tmp, 1);
    mp_hal_delay_ms(10);
    tmp[0] = ST7735_DISPON;
    send_cmd(screen, tmp, 1);
    mp_hal_delay_ms(10);

    uint32_t cfg0 = DISPLAY_CFG0;
    // uint32_t cfg2 = DISPLAY_CFG2;
    uint32_t frmctr1 = DISPLAY_CFG1;

    uint32_t madctl = cfg0 & 0xff;
    uint32_t offX = (cfg0 >> 8) & 0xff;
    uint32_t offY = (cfg0 >> 16) & 0xff;
    // uint32_t freq = (cfg2 & 0xff);

    uint8_t cmd0[] = {ST7735_MADCTL, madctl};
    uint8_t cmd1[] = {ST7735_FRMCTR1, (uint8_t)(frmctr1 >> 16), (uint8_t)(frmctr1 >> 8),
                      (uint8_t)frmctr1};
    send_cmd(screen, cmd0, sizeof(cmd0));
    send_cmd(screen, cmd1, cmd1[3] == 0xff ? 3 : 4);

    uint8_t cmd3[] = {ST7735_RASET, 0, (uint8_t)offX, 0, (uint8_t)(offX + DISPLAY_WIDTH - 1)};
    uint8_t cmd4[] = {ST7735_CASET, 0, (uint8_t)offY, 0, (uint8_t)(offY + DISPLAY_HEIGHT - 1)};
    send_cmd(screen, cmd3, sizeof(cmd3));
    send_cmd(screen, cmd4, sizeof(cmd4));

    //memset(fb, 10, sizeof(fb));
    //draw_screen(screen);
    return MP_OBJ_FROM_PTR(screen);
}


/// \method show()
///
/// Show the hidden buffer on the screen.
STATIC mp_obj_t pyb_screen_show(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    pyb_screen_obj_t *screen = MP_OBJ_TO_PTR(args[0]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);
    byte *p = bufinfo.buf;

    uint8_t cmdBuf[] = {ST7735_RAMWR};
    send_cmd(screen, cmdBuf, 1);

    mp_hal_pin_low(screen->pin_cs1); // CS=0; enable
    mp_hal_pin_high(screen->pin_dc); // DC=1
    HAL_SPI_Transmit(screen->spi->spi, p, bufinfo.len, 1000);

    mp_hal_pin_high(screen->pin_cs1); // CS=1; disable

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pyb_screen_show_obj, 2, 2, pyb_screen_show);

STATIC const mp_rom_map_elem_t pyb_screen_locals_dict_table[] = {
    // instance methods
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&pyb_screen_show_obj) }
};

STATIC MP_DEFINE_CONST_DICT(pyb_screen_locals_dict, pyb_screen_locals_dict_table);

const mp_obj_type_t pyb_screen_type = {
    { &mp_type_type },
    .name = MP_QSTR_SCREEN,
    .make_new = pyb_screen_make_new,
    .locals_dict = (mp_obj_dict_t*)&pyb_screen_locals_dict,
};

#endif // MICROPY_HW_HAS_SCREEN
