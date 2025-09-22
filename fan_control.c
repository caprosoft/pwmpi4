#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pigpiod_if2.h>
#include <signal.h>

#define FAN_PWM 18    // GPIO18 (PWM out) -> cavo blu
#define FAN_TACH 17   // GPIO17 (tachimetro in) -> cavo verde
#define FREQ 25000    // 25 kHz per ventola Noctua

volatile int tach_counter = 0;
int pi;

// Callback tachimetro
void tach_callback(int pi, unsigned user_gpio, unsigned level, uint32_t tick, void *user) {
    tach_counter++;
}

// Lettura temperatura CPU
float get_cpu_temp() {
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!fp) return -1;
    int temp_milli;
    fscanf(fp, "%d", &temp_milli);
    fclose(fp);
    return temp_milli / 1000.0;
}

// Imposta velocità ventola in %
void set_fan_speed(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int duty = (int)((percent / 100.0) * 255);
    set_PWM_dutycycle(pi, FAN_PWM, duty);
}

// Calcolo RPM
int get_rpm(int interval) {
    tach_counter = 0;
    sleep(interval);
    int pulses = tach_counter;
    int rpm = (pulses / 2) * (60 / interval); // 2 impulsi per giro
    return rpm;
}

// Pulizia all’uscita
void cleanup(int sig) {
    printf("Stop script, spengo ventola...\n");
    set_fan_speed(0);
    pigpio_stop(pi);
    exit(0);
}

int main() {
    signal(SIGINT, cleanup); // Ctrl+C handler

    pi = pigpio_start(NULL, NULL);
    if (pi < 0) {
        fprintf(stderr, "Errore: pigpiod non connesso. Avvia con: sudo systemctl start pigpiod\n");
        return 1;
    }

    // Configurazione pin
    set_mode(pi, FAN_PWM, PI_OUTPUT);
    set_PWM_frequency(pi, FAN_PWM, FREQ);
    set_mode(pi, FAN_TACH, PI_INPUT);
    set_pull_up_down(pi, FAN_TACH, PI_PUD_UP);

    // Callback tachimetro
    callback_ex(pi, FAN_TACH, FALLING_EDGE, tach_callback, NULL);

    while (1) {
        float temp = get_cpu_temp();
        int speed;

        if (temp < 50)
            speed = 0;
        else if (temp < 60)
            speed = 40;
        else if (temp < 70)
            speed = 70;
        else
            speed = 100;

        set_fan_speed(speed);
        int rpm = get_rpm(2);

        printf("Temp: %.1f°C | Ventola: %d%% | RPM: %d\n", temp, speed, rpm);
    }

    cleanup(0);
    return 0;
}
