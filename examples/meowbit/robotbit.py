from time import sleep
import math, ustruct

# Registers/etc:
PCA9685_ADDRESS    = 0x40
MODE1              = 0x00
MODE2              = 0x01
SUBADR1            = 0x02
SUBADR2            = 0x03
SUBADR3            = 0x04
PRESCALE           = 0xFE
LED0_ON_L          = 0x06
LED0_ON_H          = 0x07
LED0_OFF_L         = 0x08
LED0_OFF_H         = 0x09
ALL_LED_ON_L       = 0xFA
ALL_LED_ON_H       = 0xFB
ALL_LED_OFF_L      = 0xFC
ALL_LED_OFF_H      = 0xFD

S1 = 0x1
S2 = 0x2
S3 = 0x3
S4 = 0x4
S5 = 0x5
S6 = 0x6
S7 = 0x7
S8 = 0x8

M1A = 0x1
M1B = 0x2
M2A = 0x3
M2B = 0x4

# Bits:
RESTART            = 0x80
SLEEP              = 0x10
ALLCALL            = 0x01
INVRT              = 0x10
OUTDRV             = 0x04
RESET              = 0x00


class RobotBit(object):
    """PCA9685 PWM LED/servo controller."""

    def __init__(self, i2c, address=PCA9685_ADDRESS):
        """Initialize the PCA9685."""
        self.i2c = i2c
        self.address = address
        self.i2c.init()
        self.i2c.send(bytearray([MODE1, RESET]), self.address) # reset not sure if needed but other libraries do it
        self.set_all_pwm(0, 0)
        self.i2c.send(bytearray([MODE2, OUTDRV]), self.address)
        self.i2c.send(bytearray([MODE1, ALLCALL]), self.address)
        sleep(0.005)  # wait for oscillator
        # self.i2c.send(bytearray([MODE1]), self.address) # write register we want to read from first
        # mode1 = self.i2c.recv(1, self.address)[0]
        mode1 = self.i2c.mem_read(1, self.address, MODE1)[0]
        mode1 = mode1 & ~SLEEP  # wake up (reset sleep)
        self.i2c.send(bytearray([MODE1, mode1]), self.address)
        sleep(0.005)  # wait for oscillator
        self.set_pwm_freq(50)

    def set_pwm_freq(self, freq_hz):
        """Set the PWM frequency to the provided value in hertz."""
        prescaleval = 25000000.0    # 25MHz
        prescaleval /= 4096.0       # 12-bit
        prescaleval /= float(freq_hz)
        prescaleval -= 1.0
        # print('Setting PWM frequency to {0} Hz'.format(freq_hz))
        # print('Estimated pre-scale: {0}'.format(prescaleval))
        prescale = int(math.floor(prescaleval + 0.5))
        # print('Final pre-scale: {0}'.format(prescale))
        # self.i2c.send(bytearray([MODE1]), self.address) # write register we want to read from first
        oldmode = self.i2c.mem_read(1, self.address, MODE1)[0]
        newmode = (oldmode & 0x7F) | 0x10    # sleep
        self.i2c.send(bytearray([MODE1, newmode]), self.address)  # go to sleep
        self.i2c.send(bytearray([PRESCALE, prescale]), self.address)
        self.i2c.send(bytearray([MODE1, oldmode]), self.address)
        sleep(0.005)
        self.i2c.send(bytearray([MODE1, oldmode | 0x80]), self.address)

    def set_pwm(self, channel, on, off):
        """Sets a single PWM channel."""
        if on is None or off is None:
            # self.i2c.send(bytearray([LED0_ON_L+4*channel]), self.address) # write register we want to read from first
            data = self.i2c.mem_read(4, self.address, LED0_ON_L+4*channel)
            return ustruct.unpack('<HH', data)
        self.i2c.send(bytearray([LED0_ON_L+4*channel, on & 0xFF]), self.address)
        self.i2c.send(bytearray([LED0_ON_H+4*channel, on >> 8]), self.address)
        self.i2c.send(bytearray([LED0_OFF_L+4*channel, off & 0xFF]), self.address)
        self.i2c.send(bytearray([LED0_OFF_H+4*channel, off >> 8]), self.address)

    def set_all_pwm(self, on, off):
        """Sets all PWM channels."""
        self.i2c.send(bytearray([ALL_LED_ON_L, on & 0xFF]), self.address)
        self.i2c.send(bytearray([ALL_LED_ON_H, on >> 8]), self.address)
        self.i2c.send(bytearray([ALL_LED_OFF_L, off & 0xFF]), self.address)
        self.i2c.send(bytearray([ALL_LED_OFF_H, off >> 8]), self.address)

    def duty(self, index, value=None, invert=False):
        if value is None:
            pwm = self.set_pwm(index)
            if pwm == (0, 4096):
                value = 0
            elif pwm == (4096, 0):
                value = 4095
            value = pwm[1]
            if invert:
                value = 4095 - value
            return value
        if not 0 <= value <= 4095:
            raise ValueError("Out of range")
        if invert:
            value = 4095 - value
        if value == 0:
            self.set_pwm(index, 0, 4096)
        elif value == 4095:
            self.set_pwm(index, 4096, 0)
        else:
            self.set_pwm(index, 0, value)

    def servo(self, index, degree):
        # 50hz: 20,000 us
        v_us = (degree*1000/180+1000)
        value = int(v_us*4096/20000) * 2
        self.set_pwm(index+7, 0, value)
        
    def motor(self, index, speed):
        speed = speed * 16 # map from 256 to 4096
        if index>4 or index<=0:
            return
        pp = (index-1)*2
        pn = (index-1)*2+1
        if speed >= 0:
            self.set_pwm(pp, 0, speed)
            self.set_pwm(pn, 0, 0)
        else:
            self.set_pwm(pp, 0, 0)
            self.set_pwm(pn, 0, -speed)
        

