#ifndef __GIF_H__
#define __GIF_H__

#include <stdint.h>

#define MAX_NUM_LWZ_BITS 	12

#define GIF_INTRO_TERMINATOR ';'	//0X3B   GIF文件结束符
#define GIF_INTRO_EXTENSION  '!'    //0X21
#define GIF_INTRO_IMAGE      ','	//0X2C

#define GIF_COMMENT     	0xFE
#define GIF_APPLICATION 	0xFF
#define GIF_PLAINTEXT   	0x01
#define GIF_GRAPHICCTL  	0xF9


typedef struct __attribute__((packed))
{
	uint8_t    aBuffer[258];                     // Input buffer for data block 
	short aCode  [(1 << MAX_NUM_LWZ_BITS)]; // This array stores the LZW codes for the compressed strings 
	uint8_t    aPrefix[(1 << MAX_NUM_LWZ_BITS)]; // Prefix character of the LZW code.
	uint8_t    aDecompBuffer[3000];              // Decompression buffer. The higher the compression, the more bytes are needed in the buffer.
	uint8_t *  sp;                               // Pointer into the decompression buffer 
	int   CurBit;
	int   LastBit;
	int   GetDone;
	int   LastByte;
	int   ReturnClear;
	int   CodeSize;
	int   SetCodeSize;
	int   MaxCode;
	int   MaxCodeSize;
	int   ClearCode;
	int   EndCode;
	int   FirstCode;
	int   OldCode;
}LZW_INFO;

//逻辑屏幕描述块
typedef struct __attribute__((packed))
{
	uint16_t width;		//GIF宽度
	uint16_t height;		//GIF高度
	uint8_t flag;		//标识符  1:3:1:3=全局颜色表标志(1):颜色深度(3):分类标志(1):全局颜色表大小(3)
	uint8_t bkcindex;	//背景色在全局颜色表中的索引(仅当存在全局颜色表时有效)
	uint8_t pixratio;	//像素宽高比
}LogicalScreenDescriptor;


//图像描述块
typedef struct __attribute__((packed))
{
	uint16_t xoff;		//x方向偏移
	uint16_t yoff;		//y方向偏移
	uint16_t width;		//宽度
	uint16_t height;		//高度
	uint8_t flag;		//标识符  1:1:1:2:3=局部颜色表标志(1):交织标志(1):保留(2):局部颜色表大小(3)
}ImageScreenDescriptor;

//图像描述
typedef struct __attribute__((packed))
{
	LogicalScreenDescriptor gifLSD;	//逻辑屏幕描述块
	ImageScreenDescriptor gifISD;	//图像描述快
	uint16_t colortbl[256];				//当前使用颜色表
	uint16_t bkpcolortbl[256];			//备份颜色表.当存在局部颜色表时使用
	uint16_t numcolors;					//颜色表大小
	uint16_t delay;					    //延迟时间
	LZW_INFO *lzw;					//LZW信息
}gif89a;

#endif
