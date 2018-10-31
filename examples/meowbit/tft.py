import pyb
import framebuf
from spiflash import *

fbuf = bytearray(160*128*2)
fb = framebuf.FrameBuffer(fbuf, 160, 128, framebuf.RGB565)
tft = pyb.SCREEN()
fb.fill(0)
fb.text("hello world", 0, 0, 0x001F)


def showText(txt, x, y, col):
    b = bytearray(32)
    for c in txt:
        read_block(ord(c)*32, b)
        print (ubinascii.hexlify(b))
        y0 = y
        for i in range(0,32,2):
            for j in range(7, -1, -1):
                if (b[i]>>j) & 0x1:
                    fb.pixel(x+7-j, y0, col)
                else:
                    fb.pixel(x+7-j, y0, 0x0000)
            for j in range(7, -1, -1):
                if (b[i+1]>>j) & 0x1:
                    fb.pixel(x+8+7-j, y0, col)
                else:
                    fb.pixel(x+8+7-j, y0, 0x0000)
            y0+=1
        x+=16

showText('你好世界',50,50,0x001F)
tft.show(fb)