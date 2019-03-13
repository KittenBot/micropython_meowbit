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

// defined for fs operation
#include "lib/oofatfs/ff.h"

#include "extmod/vfs.h"
#include "extmod/vfs_fat.h"

#define LCD_INSTR (0)
//#define LCD_CHAR_BUF_W (16)
//#define LCD_CHAR_BUF_H (4)

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

static uint8_t cmdBuf[20];

typedef struct _pyb_screen_obj_t {
    mp_obj_base_t base;

    // hardware control for the LCD
    const spi_t *spi;
    const pin_obj_t *pin_cs1;
    const pin_obj_t *pin_rst;
    const pin_obj_t *pin_dc;
    const pin_obj_t *pin_bl;

    // character buffer for stdout-like output
    //char char_buffer[LCD_CHAR_BUF_W * LCD_CHAR_BUF_H];
    //int line;
    //int column;
    //int next_line;

} pyb_screen_obj_t;
_pic_info picinfo;  //图片信息
#define DELAY 0x80

static const uint8_t initCmds[] = {
    ST7735_SWRESET,   DELAY,  //  1: Software reset, 0 args, w/delay
      120,                    //     150 ms delay
    ST7735_SLPOUT ,   DELAY,  //  2: Out of sleep mode, 0 args, w/delay
      120,                    //     500 ms delay
    ST7735_INVOFF , 0      ,  // 13: Don't invert display, no args, no delay
    ST7735_INVCTR , 1      ,  // inverse, riven
      0x03,
    ST7735_COLMOD , 1      ,  // 15: set color mode, 1 arg, no delay:
      0x05,                  //     16-bit color 565
    ST7735_GMCTRP1, 16      , //  1: Magical unicorn dust, 16 args, no delay:
    0x02, 0x1c, 0x07, 0x12,
    0x37, 0x32, 0x29, 0x2d,
    0x29, 0x25, 0x2B, 0x39,
    0x00, 0x01, 0x03, 0x10,
    ST7735_GMCTRN1, 16      , //  2: Sparkles and rainbows, 16 args, no delay:
    0x03, 0x1d, 0x07, 0x06,
    0x2E, 0x2C, 0x29, 0x2D,
    0x2E, 0x2E, 0x37, 0x3F,
    0x00, 0x00, 0x02, 0x10,
    ST7735_NORON  ,    DELAY, //  3: Normal display on, no args, w/delay
      10,                     //     10 ms delay
    ST7735_DISPON ,    DELAY, //  4: Main screen turn on, no args w/delay
      10,
    0, 0 // END
};

extern fs_user_mount_t fs_user_mount_flash;

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

static void sendCmdSeq(pyb_screen_obj_t *screen, const uint8_t *buf) {
    while (*buf) {
        cmdBuf[0] = *buf++;
        int v = *buf++;
        int len = v & ~DELAY;
        // note that we have to copy to RAM
        memcpy(cmdBuf + 1, buf, len);
        send_cmd(screen, cmdBuf, len + 1);
        buf += len;
        if (v & DELAY) {
            mp_hal_delay_ms(*buf++);
        }
    }
}

static void setAddrWindow(pyb_screen_obj_t *screen, int x, int y, int w, int h) {
    uint8_t cmd0[] = {ST7735_RASET, 0, (uint8_t)x, 0, (uint8_t)(x + w - 1)};
    uint8_t cmd1[] = {ST7735_CASET, 0, (uint8_t)y, 0, (uint8_t)(y + h - 1)};
    send_cmd(screen, cmd1, sizeof(cmd1));
    send_cmd(screen, cmd0, sizeof(cmd0));
}

static void configure(pyb_screen_obj_t *screen, uint8_t madctl) {
    uint8_t cmd0[] = {ST7735_MADCTL, madctl};
    // 0x00 0x06 0x03: blue tab
    uint8_t cmd1[] = {ST7735_FRMCTR1, 0x00, 0x06, 0x03};
    send_cmd(screen, cmd0, sizeof(cmd0));
    send_cmd(screen, cmd1, sizeof(cmd1));
}

/// \classmethod \constructor(skin_position)
///
/// Construct an LCD object in the given skin position.  `skin_position` can be 'X' or 'Y', and
/// should match the position where the LCD pyskin is plugged in.
STATIC mp_obj_t pyb_screen_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    // check arguments
    int madctl = 0x60; // riven, adapt to new screen
    // inverse for framebuffer define
    int offX = 0x0;
    int offY = 0x0;
    int width = DISPLAY_HEIGHT;
    int height = DISPLAY_WIDTH;
    mp_arg_check_num(n_args, n_kw, 0, 5, false);
    if (n_args >= 1) {
        madctl = mp_obj_get_int(args[0]);
        if (n_args == 5) {
            offX = mp_obj_get_int(args[1]);
            offY = mp_obj_get_int(args[2]);
            width = mp_obj_get_int(args[3]);
            height = mp_obj_get_int(args[4]);
        }
    }

    // create screen object
    pyb_screen_obj_t *screen = m_new_obj(pyb_screen_obj_t);
    screen->base.type = &pyb_screen_type;

    // configure pins, tft bind to spi2 on f4
    screen->spi = &spi_obj[1];
    screen->pin_cs1 = pyb_pin_PB12;
    screen->pin_rst = pyb_pin_PB10;
    screen->pin_dc = pyb_pin_PA8;
    screen->pin_bl = pyb_pin_PB3;

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

    sendCmdSeq(screen, initCmds);

    madctl = madctl & 0xff;
    configure(screen, madctl);
    setAddrWindow(screen, offX, offY, width, height);

    //memset(fb, 10, sizeof(fb));
    //draw_screen(screen);
    picinfo.lcdwidth = 160;
    picinfo.lcdheight = 128;

    return MP_OBJ_FROM_PTR(screen);
}


/// \method show()
///
/// Show the hidden buffer on the screen.
STATIC mp_obj_t pyb_screen_show(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    bool usePalette = 0;
    pyb_screen_obj_t *screen = MP_OBJ_TO_PTR(args[0]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);
    byte *p = bufinfo.buf;

    if (n_args >= 3){
        usePalette = 1;
    }

    uint8_t cmdBuf[] = {ST7735_RAMWR};
    send_cmd(screen, cmdBuf, 1);

    mp_hal_pin_low(screen->pin_cs1); // CS=0; enable
    mp_hal_pin_high(screen->pin_dc); // DC=1
    if (usePalette){
        for (int i=0;i<bufinfo.len;i++){
            uint16_t color = palette[p[i] & 0xf];
            uint8_t cc[] = {color >> 8, color & 0xff};
            HAL_SPI_Transmit(screen->spi->spi, (uint8_t*)&cc, 2, 1000);
        }
    } else {
        HAL_SPI_Transmit(screen->spi->spi, p, bufinfo.len, 1000);
    }


    mp_hal_pin_high(screen->pin_cs1); // CS=1; disable

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pyb_screen_show_obj, 2, 3, pyb_screen_show);

STATIC mp_obj_t pyb_screen_showBMP(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    pyb_screen_obj_t *screen = MP_OBJ_TO_PTR(args[0]);
    const char *filename = mp_obj_str_get_str(args[1]);
    
    fs_user_mount_t *vfs_fat = &fs_user_mount_flash;
    uint8_t res;
    UINT br;
    uint8_t rgb ,color_byte;
    uint16_t x, y, color;
    uint16_t count;
    FIL fp;
    uint8_t * databuf;
    uint16_t readlen=BMP_DBUF_SIZE;

    uint16_t countpix=0;//记录像素 
    //x,y的实际坐标	
    //uint16_t  realx=0;
    //uint16_t realy=0;
    //uint8_t  yok=1;			   

    uint8_t *bmpbuf;
    uint8_t biCompression=0;

    uint16_t rowlen;
    BITMAPINFO *pbmp;

    databuf = (uint8_t*)malloc(readlen);
    res = f_open(&vfs_fat->fatfs, &fp, filename, FA_READ);
    if (res == 0){
        res = f_read(&fp, databuf, readlen, &br);
        
        pbmp=(BITMAPINFO*)databuf;
        count=pbmp->bmfHeader.bfOffBits;        	//数据偏移,得到数据段的开始地址
        color_byte=pbmp->bmiHeader.biBitCount/8;	//彩色位 16/24/32  
        biCompression=pbmp->bmiHeader.biCompression;//压缩方式
        picinfo.ImgHeight=pbmp->bmiHeader.biHeight;	//得到图片高度
        picinfo.ImgWidth=pbmp->bmiHeader.biWidth;  	//得到图片宽度 
        printf("bmp %d %d %ld %ld\n", biCompression, color_byte, pbmp->bmiHeader.biHeight, pbmp->bmiHeader.biWidth);
        // ai_draw_init();//初始化智能画图			
        //水平像素必须是4的倍数!!
        if((picinfo.ImgWidth*color_byte)%4)rowlen=((picinfo.ImgWidth*color_byte)/4+1)*4;
        else rowlen=picinfo.ImgWidth*color_byte;
        //开始解码BMP   
        color=0;//颜色清空	 													 
        x=0 ;
        y=picinfo.ImgHeight;
        rgb=0;      
        //对于尺寸小于等于设定尺寸的图片,进行快速解码
        //realy=(y*picinfo.Div_Fac)>>13;
        bmpbuf=databuf;

        // init write
        uint8_t cmdBuf[] = {ST7735_RAMWR};
        send_cmd(screen, cmdBuf, 1);
        mp_hal_pin_low(screen->pin_cs1); // CS=0; enable
        mp_hal_pin_high(screen->pin_dc); // DC=1

        while(1){
            while(count<readlen){  //读取一簇1024扇区 (SectorsPerClust 每簇扇区数)
                if(color_byte==3){   //24位颜色图
                    switch (rgb){
                        case 0:				  
                            color=bmpbuf[count]>>3; //B
                            break ;	   
                        case 1: 	 
                            color+=((uint16_t)bmpbuf[count]<<3)&0X07E0;//G
                            break;	  
                        case 2 : 
                            color+=((uint16_t)bmpbuf[count]<<8)&0XF800;//R	  
                            break ;			
                    }
                } else if (color_byte == 2){  //16位颜色图
                    switch(rgb){
                        case 0 : 
                            if(biCompression==BI_RGB)//RGB:5,5,5
                            {
                                color=((uint16_t)bmpbuf[count]&0X1F);	 	//R
                                color+=(((uint16_t)bmpbuf[count])&0XE0)<<1; //G
                            }else		//RGB:5,6,5
                            {
                                color=bmpbuf[count];  			//G,B
                            }  
                            break ;   
                        case 1 : 			  			 
                            if(biCompression==BI_RGB)//RGB:5,5,5
                            {
                                color+=(uint16_t)bmpbuf[count]<<9;  //R,G
                            }else  		//RGB:5,6,5
                            {
                                color+=(uint16_t)bmpbuf[count]<<8;	//R,G
                            }  									 
                            break ;	 
                    }
                } else if(color_byte==4){//32位颜色图
					switch (rgb){
						case 0:				  
							color=bmpbuf[count]>>3; //B
							break ;	   
						case 1: 	 
							color+=((uint16_t)bmpbuf[count]<<3)&0X07E0;//G
							break;	  
						case 2 : 
							color+=((uint16_t)bmpbuf[count]<<8)&0XF800;//R	  
							break ;			
						case 3 :
							//alphabend=bmpbuf[count];//不读取  ALPHA通道
							break ;  		  	 
					}	
				} else if(color_byte==1){}//8位色,暂时不支持,需要用到颜色表.
                rgb++;	  
                count++;
                if(rgb==color_byte){ //水平方向读取到1像素数数据后显示	
                    if(x<picinfo.ImgWidth){	
                        uint8_t cc[] = {color >> 8, color & 0xff};
                        HAL_SPI_Transmit(screen->spi->spi, (uint8_t*)&cc, 2, 1000);
                        //realx=(x*picinfo.Div_Fac)>>13;//x轴实际值
                        // todo: draw to screen
                        //if(is_element_ok(realx,realy,1)&&yok){//符合条件
                            //pic_phy.draw_point(realx+picinfo.S_XOFF,realy+picinfo.S_YOFF-1,color);//显示图片	
                            //POINT_COLOR=color;
                            //LCD_DrawPoint(realx+picinfo.S_XOFF,realy+picinfo.S_YOFF); 
                            //SRAMLCD.Draw_Point(realx+picinfo.S_XOFF,realy+picinfo.S_YOFF,color);
                        //}   									    
                    }
                    x++;//x轴增加一个像素
                    color=0x00;
                    rgb=0;
                }
                countpix++;//像素累加
                if(countpix>=rowlen){//水平方向像素值到了.换行
                    y--; 
                    if(y==0)break;
                    //realy=(y*picinfo.Div_Fac)>>13;//实际y值改变	 
                    //if(is_element_ok(realx,realy,0))yok=1;//此处不改变picinfo.staticx,y的值	 
                    //else yok=0; 
                    x=0; 
                    countpix=0;
                    color=0x00;
                    rgb=0;
                }
            }
            res=f_read(&fp, databuf, readlen, &br);//读出readlen个字节
            if(br!=readlen)readlen=br;	//最后一批数据		  
            if(res||br==0)break;		//读取出错
            bmpbuf=databuf;
            count=0;
        }
        f_close(&fp);
    }
    free(databuf);
    mp_hal_pin_high(screen->pin_cs1); // CS=1; disable
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pyb_screen_showbmp_obj, 2, 3, pyb_screen_showBMP);

STATIC const mp_rom_map_elem_t pyb_screen_locals_dict_table[] = {
    // instance methods
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&pyb_screen_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_showBMP), MP_ROM_PTR(&pyb_screen_showbmp_obj) },
};

STATIC MP_DEFINE_CONST_DICT(pyb_screen_locals_dict, pyb_screen_locals_dict_table);

const mp_obj_type_t pyb_screen_type = {
    { &mp_type_type },
    .name = MP_QSTR_SCREEN,
    .make_new = pyb_screen_make_new,
    .locals_dict = (mp_obj_dict_t*)&pyb_screen_locals_dict,
};

#endif // MICROPY_HW_HAS_SCREEN
