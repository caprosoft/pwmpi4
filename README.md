# Raspberry Pi 4 PWM Fan Controller Service

This project provides a lightweight and efficient solution for controlling 5V PWM fans (such as the Noctua NF-A4x10) on a Raspberry Pi 4.

Written in C and leveraging the **pigpio** library, it offers precise `fan speed regulation based on CPU temperature`, real-time tachometer feedback (RPM monitoring), and configurable hysteresis for smooth operation.

It comes with a systemd service file to ensure the fan controller starts immediately during early boot.

## Key Features
*   **Lightweight & Fast**: Written in pure C for minimal resource usage.
*   **Smart Control**: Adjusts fan speed dynamically based on CPU temperature.
*   **RPM Monitoring**: Reads fan speed via the tachometer pin (GPIO17).
*   **Smooth Operation**: Implements hysteresis to prevent rapid speed switching.
*   **Systemd Integration**: Includes a service unit for automatic startup at boot.
*   **Hardware Support**: Optimized for Raspberry Pi 4 and 5V PWM fans.

---

## 1. Requirements

### Hardware
- Raspberry Pi 4
- 5V PWM Fan (e.g. Noctua NF-A4x10 5V PWM)
- Connections:
  - 🔵 **GPIO18 (Pin 12) (PWM)** → blue wire
  - 🟢 **GPIO17 (Pin 11) (Tachometer)** → green wire
  - 🟡 **5V (Pin 4)** → yellow wire
  - ⚫ **GND (Pin 6)** → black wire

### Software
- Raspberry Pi OS (Raspbian)
- `pigpiod` active at boot (pigpio daemon)
- Disable the 'fan' option in the Performance settings of the Raspberry Pi configuration (`raspi-config`)

---

## 2. Installing development libraries

Open a terminal and run:

```bash
sudo apt update
sudo apt install build-essential libpigpiod-if2-1 libpigpiod-if2-dev pigpio
```

- `build-essential` → C compiler and make utilities
- `libpigpiod-if2-dev` → headers and C library to interface with `pigpiod`
- `pigpio` → daemon for GPIO/PWM management

---

## 3. Preparing the project directory

Clone the repository:

```bash
git clone https://github.com/caprosoft/pwmpi4.git
cd pwmpi4
```

Create the installation directory:

```bash
sudo mkdir -p /opt/fancontrol
sudo chown $(whoami):$(whoami) /opt/fancontrol
```

Copy the source files into the folder:

```bash
cp fan_control.c /opt/fancontrol/
```

Enter the folder:

```bash
cd /opt/fancontrol
```

---

## 4. Compiling the program

Compile the C code using the `pigpiod_if2` library:

```bash
gcc fan_control.c -lpigpiod_if2 -o fan_control
```

Make it executable:

```bash
chmod +x fan_control
```

---

## 5. Manual start

Start the `pigpiod` daemon (if it is not already running):

```bash
sudo systemctl start pigpiod
```

Run the program manually for a quick test:

```bash
./fan_control
```

You should see output similar to:

```
Temp: 51.2°C | Fan: 20% | RPM: 3500
Temp: 53.0°C | Fan: 35% | RPM: 4200
...
```

To stop manually: `Ctrl+C` (the program will perform a clean shutdown of the fan before exiting).

---

## 6. Installing the systemd service (early boot)

Create the systemd unit file `/etc/systemd/system/fancontrol.service` using `nano`:

```bash
sudo nano /etc/systemd/system/fancontrol.service
```

Paste the following content into the file:

```ini
[Unit]
Description=Custom PWM Fan Controller Service for Raspberry Pi
Requires=pigpiod.service
After=pigpiod.service
Before=multi-user.target
DefaultDependencies=no

[Service]
Type=simple
ExecStart=/opt/fancontrol/fan_control
ExecStartPre=/bin/sh -c 'for i in 0 1 2 3 4; do systemctl is-active --quiet pigpiod && exit 0 || sleep 1; done; exit 1'
WorkingDirectory=/opt/fancontrol
User=pi
Group=pi
Restart=always
RestartSec=2
SyslogIdentifier=fancontrol
ProtectHome=true
NoNewPrivileges=true

[Install]
WantedBy=sysinit.target
```

Save and exit (`Ctrl+O`, `Enter`, `Ctrl+X`).

**Note:** if you use a user other than `pi`, modify `User=` and `Group=` accordingly.

Enable and start the service:

```bash
sudo systemctl daemon-reload
sudo systemctl enable fancontrol
sudo systemctl start fancontrol
```

Check the status:

```bash
systemctl status fancontrol
journalctl -u fancontrol -f
```

---

## 7. Debug and monitoring

Check that `pigpiod` is active:

```bash
systemctl status pigpiod
```

Real-time service logs:

```bash
journalctl -u fancontrol -f
```

If the service crashes repeatedly, check `journalctl -xe` for error messages and the executable's logs.

---

## 8. Stopping and removing the service

To stop temporarily:

```bash
sudo systemctl stop fancontrol
```

To disable at startup:

```bash
sudo systemctl disable fancontrol
```

To remove the unit file:

```bash
sudo rm /etc/systemd/system/fancontrol.service
sudo systemctl daemon-reload
```

---

## 9. Useful customizations

- **Code parameters**: hysteresis, thresholds, ramp step, and MIN_TIME_AT_LEVEL are defined in the C source as `#define` — modify `fan_control.c` and recompile to change behavior.
- **Executable path**: change it to `/opt/fancontrol` or wherever you prefer, but update `ExecStart` in the `.service` file.
- **Logging to file**: if you want logs in `/var/log/fancontrol.log` as well, you can add to the `[Service]` block:
  ```ini
  StandardOutput=append:/var/log/fancontrol.log
  StandardError=inherit
  ```
  and create the file with correct permissions:
  ```bash
  sudo touch /var/log/fancontrol.log
  sudo chown pi:pi /var/log/fancontrol.log
  ```
- **RPM Test**: if the tachometer does not provide pulses, check wiring (pull-up) and that the configured pin is correct.

---

## 10. Example of quick procedures (one-liners)

Compile, copy, and install service (quick example):

```bash
# from the folder containing fan_control.c
gcc fan_control.c -lpigpiod_if2 -o fan_control
sudo mkdir -p /opt/fancontrol
sudo cp fan_control /opt/fancontrol/
sudo chown -R pi:pi /opt/fancontrol
sudo chmod +x /opt/fancontrol/fan_control
# copy the service (assuming fancontrol.service already created in cwd)
sudo cp fancontrol.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now fancontrol
```

---
