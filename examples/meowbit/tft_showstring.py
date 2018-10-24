import pyb
import framebuf
fbuf = bytearray(160*128*2)
fb = framebuf.FrameBuffer(fbuf, 160, 128, framebuf.RGB565)
tft = pyb.SCREEN()
fb.fill(0)
fb.text("hello world", 0, 0, 0x001F)
tft.show(fb)
