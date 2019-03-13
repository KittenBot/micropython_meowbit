#ifndef MICROPY_INCLUDED_STM32_SCREEN_H
#define MICROPY_INCLUDED_STM32_SCREEN_H

#include <stdint.h>

#define ST7735_NOP 0x00
#define ST7735_SWRESET 0x01
#define ST7735_RDDID 0x04
#define ST7735_RDDST 0x09

#define ST7735_SLPIN 0x10
#define ST7735_SLPOUT 0x11
#define ST7735_PTLON 0x12
#define ST7735_NORON 0x13

#define ST7735_INVOFF 0x20
#define ST7735_INVON 0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON 0x29
#define ST7735_CASET 0x2A
#define ST7735_RASET 0x2B
#define ST7735_RAMWR 0x2C
#define ST7735_RAMRD 0x2E

#define ST7735_PTLAR 0x30
#define ST7735_COLMOD 0x3A
#define ST7735_MADCTL 0x36

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR 0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1 0xC0
#define ST7735_PWCTR2 0xC1
#define ST7735_PWCTR3 0xC2
#define ST7735_PWCTR4 0xC3
#define ST7735_PWCTR5 0xC4
#define ST7735_VMCTR1 0xC5

#define ST7735_RDID1 0xDA
#define ST7735_RDID2 0xDB
#define ST7735_RDID3 0xDC
#define ST7735_RDID4 0xDD

#define ST7735_PWCTR6 0xFC

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

#define DISPLAY_CFG0 0x00020108 // 0x00020140
#define DISPLAY_CFG1 0x00000603

#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 128

extern const mp_obj_type_t pyb_screen_type;

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// defined for bmp decoder
#define BMP_DBUF_SIZE   2048
//图像信息
typedef struct __attribute__((packed))
{		
	uint16_t lcdwidth;	//LCD的宽度
	uint16_t lcdheight;	//LCD的高度
	uint32_t ImgWidth; 	//图像的实际宽度和高度
	uint32_t ImgHeight;

	uint32_t Div_Fac;  	//缩放系数 (扩大了8192倍的)
	
	uint32_t S_Height; 	//设定的高度和宽度
	uint32_t S_Width;
	
	uint32_t	S_XOFF;	  	//x轴和y轴的偏移量
	uint32_t S_YOFF;

	uint32_t staticx; 	//当前显示到的ｘｙ坐标
	uint32_t staticy;																 	
}_pic_info;

#define BI_RGB	 		0  //没有压缩.RGB 5,5,5.
#define BI_RLE8 		1  //每个象素8比特的RLE压缩编码，压缩格式由2字节组成(重复象素计数和颜色索引)；
#define BI_RLE4 		2  //每个象素4比特的RLE压缩编码，压缩格式由2字节组成
#define BI_BITFIELDS 	3  //每个象素的比特由指定的掩码决定。  
//BMP信息头
typedef struct __attribute__((packed))
{
    uint32_t biSize ;		   	//说明BITMAPINFOHEADER结构所需要的字数。
    uint32_t  biWidth ;		   	//说明图象的宽度，以象素为单位 
    uint32_t  biHeight ;	   	//说明图象的高度，以象素为单位 
    uint16_t  biPlanes ;	   		//为目标设备说明位面数，其值将总是被设为1 
    uint16_t  biBitCount ;	   	//说明比特数/象素，其值为1、4、8、16、24、或32
    uint32_t biCompression ;  	//说明图象数据压缩的类型。其值可以是下述值之一：
	//BI_RGB：没有压缩；
	//BI_RLE8：每个象素8比特的RLE压缩编码，压缩格式由2字节组成(重复象素计数和颜色索引)；  
    //BI_RLE4：每个象素4比特的RLE压缩编码，压缩格式由2字节组成
  	//BI_BITFIELDS：每个象素的比特由指定的掩码决定。
    uint32_t biSizeImage ;		//说明图象的大小，以字节为单位。当用BI_RGB格式时，可设置为0  
    uint32_t  biXPelsPerMeter ;	//说明水平分辨率，用象素/米表示
    uint32_t  biYPelsPerMeter ;	//说明垂直分辨率，用象素/米表示
    uint32_t biClrUsed ;	  	 	//说明位图实际使用的彩色表中的颜色索引数
    uint32_t biClrImportant ; 	//说明对图象显示有重要影响的颜色索引的数目，如果是0，表示都重要。 
}BITMAPINFOHEADER ;
//BMP头文件
typedef struct __attribute__((packed))
{
    uint16_t  bfType ;     //文件标志.只对'BM',用来识别BMP位图类型
    uint32_t  bfSize ;	  //文件大小,占四个字节
    uint16_t  bfReserved1 ;//保留
    uint16_t  bfReserved2 ;//保留
    uint32_t  bfOffBits ;  //从文件开始到位图数据(bitmap data)开始之间的的偏移量
}BITMAPFILEHEADER ;
//彩色表 
typedef struct __attribute__((packed))
{
    uint8_t rgbBlue ;    //指定蓝色强度
    uint8_t rgbGreen ;	//指定绿色强度 
    uint8_t rgbRed ;	  	//指定红色强度 
    uint8_t rgbReserved ;//保留，设置为0 
}RGBQUAD ;
//位图信息头
typedef struct __attribute__((packed))
{ 
	BITMAPFILEHEADER bmfHeader;
	BITMAPINFOHEADER bmiHeader;  
	uint32_t RGB_MASK[3];			//调色板用于存放RGB掩码.
	//RGBQUAD bmiColors[256];  
}BITMAPINFO; 
typedef RGBQUAD * LPRGBQUAD;//彩色表  

#ifdef __cplusplus
}
#endif

#endif // MICROPY_INCLUDED_STM32_LCD_H