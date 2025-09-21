import RPi.GPIO as GPIO
import time
import os

# --- Configurazione dei Pin ---
FAN_PIN_PWM = 14      # Pin di controllo PWM (filo blu) -> GPIO 14 (Pin 8)
FAN_PIN_TACHO = 25    # Pin di input del tachimetro (filo verde) -> GPIO 25 (Pin 22)
PWM_FREQ = 25         # Frequenza PWM in Hz

# --- Variabili Globali ---
tacho_pulse_count = 0 # Contatore per gli impulsi del tachimetro
rpm = 0               # Valore RPM calcolato

# --- Funzione di Callback per il Tachimetro ---
# Questa funzione viene chiamata ogni volta che il pin del tachimetro rileva un impulso
def count_pulse(channel):
    global tacho_pulse_count
    tacho_pulse_count += 1

# --- Funzione Principale ---
try:
    # Impostazione della modalità dei pin GPIO
    GPIO.setmode(GPIO.BCM)

    # Setup del pin PWM per il controllo della velocità
    GPIO.setup(FAN_PIN_PWM, GPIO.OUT)
    fan_pwm = GPIO.PWM(FAN_PIN_PWM, PWM_FREQ)
    fan_pwm.start(0) # Avvia la ventola con velocità 0

    # Setup del pin del tachimetro come input con pull-up
    # Il pull-up assicura un segnale stabile
    GPIO.setup(FAN_PIN_TACHO, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    # Aggiungi un "event detect" che chiama la funzione count_pulse ad ogni impulso (flanco di discesa)
    GPIO.add_event_detect(FAN_PIN_TACHO, GPIO.FALLING, callback=count_pulse)

    print("Controllo ventola avviato. Premi CTRL+C per uscire.")

    while True:
        # Resetta il contatore di impulsi prima di una nuova misurazione
        tacho_pulse_count = 0

        # Attendi per un intervallo di tempo definito (es. 5 secondi)
        # Durante questo tempo, l'interrupt conterà gli impulsi in background
        sleep_interval = 5
        time.sleep(sleep_interval)

        # Calcola gli RPM
        # RPM = (impulsi / intervallo_in_secondi) * 60 secondi / 2 impulsi_per_giro
        rpm = (tacho_pulse_count / sleep_interval) * 30

        # Leggi la temperatura della CPU
        temp_str = os.popen("vcgencmd measure_temp").readline()
        temp_c = float(temp_str.replace("temp=", "").replace("'C\n", ""))

        # Logica di controllo della velocità della ventola
        duty_cycle = 0 # Default duty cycle
        if temp_c > 70:
            duty_cycle = 100
        elif temp_c > 60:
            duty_cycle = 80
        elif temp_c > 50:
            duty_cycle = 50
        elif temp_c > 45:
            duty_cycle = 25
        else:
            duty_cycle = 0 # Ventola spenta a basse temperature

        fan_pwm.ChangeDutyCycle(duty_cycle)
        
        # Stampa le informazioni
        print(f"Temperatura: {temp_c:.1f}°C -> PWM: {duty_cycle}% -> Velocità Ventola: {int(rpm)} RPM")


except KeyboardInterrupt:
    print("\nScript terminato dall'utente.")
finally:
    # Pulisci i pin GPIO e ferma la ventola prima di uscire
    fan_pwm.stop()
    GPIO.cleanup()
