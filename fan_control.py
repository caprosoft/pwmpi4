#!/usr/bin/env python3
import pigpio
import time

# Pin di controllo
FAN_PWM = 18    # GPIO18 (PWM out) -> cavo blu
FAN_TACH = 17   # GPIO17 (tachimetro in) -> cavo verde
FREQ = 25000    # 25 kHz per ventola Noctua

pi = pigpio.pi()
if not pi.connected:
    exit("Errore: pigpiod non connesso. Avvia con: sudo systemctl start pigpiod")

# Configurazione pin
pi.set_mode(FAN_PWM, pigpio.OUTPUT)
pi.set_PWM_frequency(FAN_PWM, FREQ)
pi.set_mode(FAN_TACH, pigpio.INPUT)
pi.set_pull_up_down(FAN_TACH, pigpio.PUD_UP)  # tach open-collector

# Variabili per conteggio RPM
tach_counter = 0

def tach_callback(gpio, level, tick):
    global tach_counter
    tach_counter += 1  # ogni fronte (alto-basso o basso-alto)

cb = pi.callback(FAN_TACH, pigpio.FALLING_EDGE, tach_callback)

def get_cpu_temp():
    with open("/sys/class/thermal/thermal_zone0/temp", "r") as f:
        return int(f.read().strip()) / 1000

def set_fan_speed(percent):
    duty = int((percent / 100.0) * 255)
    pi.set_PWM_dutycycle(FAN_PWM, duty)

def get_rpm(interval=2):
    global tach_counter
    tach_counter = 0
    time.sleep(interval)
    pulses = tach_counter
    # Noctua = 2 impulsi per giro
    rpm = (pulses / 2) * (60 / interval)
    return int(rpm)

try:
    while True:
        temp = get_cpu_temp()

        # Logica di controllo (a gradini)
        if temp < 50:
            speed = 0
        elif temp < 60:
            speed = 40
        elif temp < 70:
            speed = 70
        else:
            speed = 100

        set_fan_speed(speed)
        rpm = get_rpm()

        print(f"Temp: {temp:.1f}°C | Ventola: {speed}% | RPM: {rpm}")
        
except KeyboardInterrupt:
    print("Stop script, spengo ventola...")
    set_fan_speed(0)
    pi.stop()
