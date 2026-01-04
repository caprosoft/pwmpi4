#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pigpiod_if2.h>
#include <time.h>

/*
 * fan_controller.c
 * ----------------
 */

// =======================
// Configurable parameters
// =======================

#define HYST 3                   // °C hysteresis
#define MIN_TIME_AT_LEVEL 20     // minimum seconds before changing level
#define RAMP_STEP 8              // increment % per ramp step
#define RAMP_DELAY 400000        // microseconds between ramp steps (0.4s)
#define MAX_SAFE_TEMP 70         // beyond this temp: 100%
#define EMERGENCY_TEMP 80        // emergency: force 100%

// =================
// Pin and PWM freq
// =================

#define FAN_PWM 18   // GPIO18 (PWM)
#define FAN_TACH 17  // GPIO17 (tachometer)
#define FREQ 25000   // 25 kHz

/*
 * Temperature→duty mapping table
 */
typedef struct {
    int threshold;
    int percent;
} fan_entry;

fan_entry FAN_MAP[] = {
    {0,   0},    // 0% (off) up to 45°C
    {45,  0},
    {50,  30},   // 30% speed above 45°C
    {55,  45},
    {60,  60},
    {65,  75},
    {70,  90},
    {999, 100}
};
int FAN_MAP_LEN = sizeof(FAN_MAP)/sizeof(FAN_MAP[0]);

// ==================
// Global variables
// ==================

volatile int tach_counter = 0;   // incremented by tachometer callback
int pi;                          // pigpio handle

// ======================
// Callback: tachometer
// ======================

void tach_callback(int pi, unsigned user_gpio, unsigned level, uint32_t tick) {
    if (level == 0) tach_counter++; // count falling edges only
}

// =====================
// Read CPU temperature
// =====================

float get_cpu_temp() {
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!fp) return -1;
    int t;
    fscanf(fp, "%d", &t);
    fclose(fp);
    return t / 1000.0;
}

// =================
// Set PWM duty (%)
// =================

void set_fan_speed(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int duty = (int)((percent / 100.0) * 255);
    set_PWM_dutycycle(pi, FAN_PWM, duty);
}

// =======================
// Measure RPM (blocking)
// =======================

int get_rpm(int interval) {
    tach_counter = 0;
    sleep(interval);
    int pulses = tach_counter;
    int rpm = (pulses / 2) * (60 / interval); // Noctua = 2 pulses/rev
    return rpm;
}

// ==========================
// Lookup: temp -> target pct
// ==========================

int target_speed_from_temp(float temp) {
    for (int i=0; i<FAN_MAP_LEN; i++) {
        if (temp <= FAN_MAP[i].threshold)
            return FAN_MAP[i].percent;
    }
    return 100;
}

// =================
// Soft-ramp routine
// =================

int ramp_to(int current_pct, int target_pct) {
    if (current_pct == target_pct) return current_pct;
    int step = (target_pct > current_pct) ? RAMP_STEP : -RAMP_STEP;
    int pct = current_pct;
    while (pct != target_pct) {
        int next_pct = pct + step;
        if ((step > 0 && next_pct > target_pct) || (step < 0 && next_pct < target_pct)) {
            next_pct = target_pct;
        }
        set_fan_speed(next_pct);
        pct = next_pct;
        if (pct == target_pct) break;
        usleep(RAMP_DELAY);
    }
    return pct;
}

// ========
// main()
// ========

int main() {
    // Connect to pigpiod
    pi = pigpio_start(NULL, NULL);
    if (pi < 0) {
        fprintf(stderr, "Error: pigpiod not connected\n");
        return 1;
    }

    // Setup pins
    set_mode(pi, FAN_PWM, PI_OUTPUT);
    set_PWM_frequency(pi, FAN_PWM, FREQ);
    set_mode(pi, FAN_TACH, PI_INPUT);
    set_pull_up_down(pi, FAN_TACH, PI_PUD_UP);
    callback(pi, FAN_TACH, FALLING_EDGE, tach_callback);

    int current_pct = 0;
    time_t last_change_time = 0;

    while (1) {
        float temp = get_cpu_temp();
        int target_pct;

        if (temp >= EMERGENCY_TEMP) {
            printf("[EMERGENCY] Temp %.1f°C >= %d°C — FORCING 100%%\n", temp, EMERGENCY_TEMP);
            target_pct = 100;
        } else {
            int base_target = target_speed_from_temp(temp);

            if (base_target < current_pct) {
                int base_thresh = 0;
                for (int i=0; i<FAN_MAP_LEN; i++) {
                    if (FAN_MAP[i].percent == base_target) {
                        base_thresh = FAN_MAP[i].threshold;
                        break;
                    }
                }
                if (temp > (base_thresh - HYST)) {
                    target_pct = current_pct;
                } else {
                    target_pct = base_target;
                }
            } else if (base_target > current_pct) {
                target_pct = base_target;
            } else {
                target_pct = current_pct;
            }
        }

        time_t now = time(NULL);
        if (target_pct != current_pct && (now - last_change_time) < MIN_TIME_AT_LEVEL) {
            target_pct = current_pct;
        }

        if (target_pct != current_pct) {
            current_pct = ramp_to(current_pct, target_pct);
            last_change_time = now;
        }

        int rpm = get_rpm(1);
        printf("Temp: %.1f°C | Fan: %d%% | RPM: %d\n", temp, current_pct, rpm);
    }

    set_fan_speed(0);
    pigpio_stop(pi);
    return 0;
}
