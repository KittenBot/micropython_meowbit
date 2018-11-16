import pyb
import framebuf
from spiflash import *

fbuf = bytearray(160*128)
fb = framebuf.FrameBuffer(fbuf, 160, 128, framebuf.GS8)
tft = pyb.SCREEN()
fb.fill(0)

def showText(txt, x, y, col):
    b = bytearray(32)
    for c in txt:
        read_block(ord(c)*32, b)
        y0 = y
        for i in range(0,32,2):
            for j in range(7, -1, -1):
                if (b[i]>>j) & 0x1:
                    fb.pixel(x+7-j, y0, col)
                else:
                    fb.pixel(x+7-j, y0, 0)
            for j in range(7, -1, -1):
                if (b[i+1]>>j) & 0x1:
                    fb.pixel(x+8+7-j, y0, col)
                else:
                    fb.pixel(x+8+7-j, y0, 0)
            y0+=1
        x+=16

showText('你好世界', 20, 10, 0xFF0000)
fb.text('hello,world', 32, 30, 0x00FF00)
showText('안녕하세요', 35, 50, 0x0000FF)
showText('こんにちは世界', 24, 70, 0x007f7f)

tft.show(fb)