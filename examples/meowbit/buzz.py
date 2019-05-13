from pyb import Pin, Timer, delay

Do = 261
Re = 294
Mi = 329
Fa = 349
So = 392
Ra = 440
Xi = 494

buzz = Pin('BUZZ')
tim = Timer(4, freq=3000)
ch = tim.channel(3, Timer.PWM, pin=buzz)
noteMap = [440, 494, 262, 294, 330, 349, 392]

def tone(freq, d=1000):
    if freq == 0:
        delay(d)
        return
    tim.freq(freq)
    ch.pulse_width_percent(30)
    delay(d)
    ch.pulse_width_percent(0)

def music(m):
    m = m.lower()
    octave = 4
    duration = 500
    n = 0
    while n < len(m):
        note = ord(m[n])
        if note >= ord('a') and note <= ord('g'):
            freq = noteMap[note-ord('a')]
        elif note == ord('r'):
            freq = 0
        elif note >= ord('2') and note <= ord('6'):
            octave = note - ord('0')
        elif note == ord(':'):
            n+=1
            note = ord(m[n])
            duration = (note - ord('0'))*125
        elif note == ord(' '):
            freq *= pow(2, octave-4)
            tone(freq, duration)
        n+=1
