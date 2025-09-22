#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pigpiod_if2.h>
#include <signal.h>
#include <time.h>

#define FAN_PWM  18   // GPIO18 (PWM out) -> cavo blu
#define FAN_TACH 17   // GPIO17 (tach in)  -> cavo verde
#define FREQ     25000 // 25 kHz Noctua

volatile int tach_counter = 0;
int pi;  // handle pigpio

// Callback tachimetro
void tach_callback(int pi, unsigned gpio, unsigned level, uint32_t tick, void *userdata) {
    if (level == 0) { // conteggio solo fronti di discesa
        tach_counter++;
    }
}

// Leggi temperatura CPU
double get_cpu_temp() {
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!fp) return -1.0;
    int temp_milli;
    fscanf(fp, "%d", &temp_milli);
    fclose(fp);
    return temp_milli / 1000.0;
}

// Imposta velocità ventola (percentuale)
void set_fan_speed(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int duty = (percent * 255) / 100;  // duty range 0-255
    set_PWM_dutycycle(pi, FAN_PWM, duty);
}

// Calcola RPM su un intervallo in secondi
int get_rpm(int interval) {
    tach_counter = 0;
    sleep(interval);
    int pulses = tach_counter;
    int rpm = (pulses / 2) * (60 / interval); // Noctua = 2 impulsi/giro
    return rpm;
}

// Pulizia all'uscita
void handle_sigint(int sig) {
    printf("\nStop programma, spengo ventola...\n");
    set_fan_speed(0);
    pigpio_stop(pi);
    exit(0);
}

int main() {
    signal(SIGINT, handle_sigint);

    // Connessione a pigpiod
    pi = pigpio_start(NULL, NULL);
    if (pi < 0) {
        fprintf(stderr, "Errore: pigpiod non connesso. Avvia con: sudo systemctl start pigpiod\n");
        return 1;
    }

    // Configurazione pin
    set_mode(pi, FAN_PWM, PI_OUTPUT);
    set_PWM_frequency(pi, FAN_PWM, FREQ);
    set_PWM_range(pi, FAN_PWM, 255);

    set_mode(pi, FAN_TACH, PI_INPUT);
    set_pull_up_down(pi, FAN_TACH, PI_PUD_UP);
    callback(pi, FAN_TACH, FALLING_EDGE, tach_callback);

    while (1) {
        double temp = get_cpu_temp();
        int speed;

        // Logica a gradini
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

        printf("Temp: %.1f °C | Ventola: %d%% | RPM: %d\n", temp, speed, rpm);
        fflush(stdout);
    }

    // Cleanup
    pigpio_stop(pi);
    return 0;
}
