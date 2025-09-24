# Guida compilazione ed esecuzione - Fan Control in C (Raspberry Pi 4)

Questa guida spiega come compilare ed eseguire il programma `fan_control.c` che gestisce una ventola PWM (es. Noctua NF-A4x10) su Raspberry Pi 4 tramite la libreria **pigpio**. Include anche l'unit systemd per avvio early-boot.

---

## 1. Requisiti

### Hardware
- Raspberry Pi 4  
- Ventola PWM 5V (es. Noctua NF-A4x10)  
- Collegamenti:
  - **GPIO18 (PWM)** → filo blu
  - **GPIO17 (Tachimetro)** → filo verde
  - **5V / GND** → fili giallo e nero

### Software
- Raspberry Pi OS (Raspbian)
- `pigpiod` attivo al boot (demone pigpio)

---

## 2. Installazione librerie di sviluppo

Apri un terminale e esegui:

```bash
sudo apt update
sudo apt install build-essential libpigpiod-if2-1 libpigpiod-if2-dev pigpio
```

- `build-essential` → compilatore C e utilità make
- `libpigpiod-if2-dev` → header e libreria C per interfacciarsi con `pigpiod`
- `pigpio` → demone per gestione GPIO/PWM

---

## 3. Preparare la cartella progetto

```bash
sudo mkdir -p /opt/fancontrol
sudo chown $(whoami):$(whoami) /opt/fancontrol
```

Copia i file sorgente/eseguibile nella cartella:

```bash
# se hai solo il .c
cp fan_control.c /opt/fancontrol/

# se hai compilato localmente l'eseguibile
# cp fan_control /opt/fancontrol/
```

Entra nella cartella:

```bash
cd /opt/fancontrol
```

---

## 4. Compilazione del programma

Compila il C usando la libreria `pigpiod_if2`:

```bash
gcc fan_control.c -lpigpiod_if2 -o fan_control
```

Rendi eseguibile:

```bash
chmod +x fan_control
```

---

## 5. Avvio manuale

Avvia il demone `pigpiod` (se non è già in esecuzione):

```bash
sudo systemctl start pigpiod
```

Esegui il programma manualmente per test rapido:

```bash
./fan_control
```

Dovresti vedere un output simile a:

```
Temp: 51.2°C | Fan: 20% | RPM: 3500
Temp: 53.0°C | Fan: 35% | RPM: 4200
...
```

Per fermare manualmente: `Ctrl+C` (il programma farà lo shutdown pulito della ventola prima di uscire).

---

## 6. Installazione del servizio systemd (early boot)

Crea il file di unit systemd `/etc/systemd/system/fancontrol.service`. Puoi farlo con `sudo nano` oppure con `tee`:

```bash
sudo tee /etc/systemd/system/fancontrol.service > /dev/null <<'EOF'
[Unit]
Description=Custom PWM Fan Control (C) for Raspberry Pi - Early Boot
Requires=pigpiod.service
After=pigpiod.service
Before=multi-user.target
DefaultDependencies=no
WantedBy=sysinit.target

[Service]
Type=simple
ExecStart=/opt/fancontrol/fan_control
ExecStartPre=/bin/sh -c 'for i in 0 1 2 3 4; do systemctl is-active --quiet pigpiod && exit 0 || sleep 1; done; exit 1'
WorkingDirectory=/opt/fancontrol
User=pi
Group=pi
Restart=always
RestartSec=2
StartLimitIntervalSec=60
StartLimitBurst=5
SyslogIdentifier=fancontrol
ProtectHome=true
NoNewPrivileges=true

[Install]
WantedBy=sysinit.target
EOF
```

**Nota:** se usi un utente diverso da `pi`, modifica `User=` e `Group=` di conseguenza.

Abilita e avvia il servizio:

```bash
sudo systemctl daemon-reload
sudo systemctl enable fancontrol
sudo systemctl start fancontrol
```

Verifica lo stato:

```bash
systemctl status fancontrol
journalctl -u fancontrol -f
```

---

## 7. Debug e monitoraggio

Controlla che `pigpiod` sia attivo:

```bash
systemctl status pigpiod
```

Log in tempo reale del servizio:

```bash
journalctl -u fancontrol -f
```

Se il servizio va in crash ripetuto, guarda `journalctl -xe` per messaggi di errore e i log dell'eseguibile.

---

## 8. Arresto e rimozione del servizio

Per fermare temporaneamente:

```bash
sudo systemctl stop fancontrol
```

Per disabilitare all'avvio:

```bash
sudo systemctl disable fancontrol
```

Per rimuovere il file unit:

```bash
sudo rm /etc/systemd/system/fancontrol.service
sudo systemctl daemon-reload
```

---

## 9. Personalizzazioni utili

- **Parametri del codice**: isteresi, soglie, ramp step e MIN_TIME_AT_LEVEL sono definiti nel sorgente C come `#define` — modifica `fan_control.c` e ricompila per cambiare comportamento.  
- **Percorso eseguibile**: cambialo in `/opt/fancontrol` o dove preferisci, ma aggiorna `ExecStart` nel file `.service`.  
- **Logging su file**: se vuoi log anche in `/var/log/fancontrol.log`, puoi aggiungere al blocco `[Service]`:
  ```ini
  StandardOutput=append:/var/log/fancontrol.log
  StandardError=inherit
  ```
  e creare il file con i permessi corretti:
  ```bash
  sudo touch /var/log/fancontrol.log
  sudo chown pi:pi /var/log/fancontrol.log
  ```
- **Test di RPM**: se il tachimetro non fornisce impulsi, controlla wiring (pull-up) e che il pin configurato sia corretto.

---

## 10. Esempio di procedure rapide (one-liners)

Compilare, copiare e installare servizio (esempio rapido):

```bash
# dalla cartella contenente fan_control.c
gcc fan_control.c -lpigpiod_if2 -o fan_control
sudo mkdir -p /opt/fancontrol
sudo cp fan_control /opt/fancontrol/
sudo chown -R pi:pi /opt/fancontrol
sudo chmod +x /opt/fancontrol/fan_control
# copia del service (assumendo fancontrol.service già creato nella cwd)
sudo cp fancontrol.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now fancontrol
```

---
