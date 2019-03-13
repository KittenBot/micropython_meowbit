#ifndef __BMP_H__
#define __BMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// defined for bmp decoder
#define BMP_DBUF_SIZE   2048

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

#endif
