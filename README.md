# Animal Deterrent System — ESP32-CAM + Blynk + AI Detection

A WiFi-connected animal/intruder deterrent system built on the ESP32-CAM module. It streams live video,
sounds a buzzer + flash LED when triggered, sends push notifications via Blynk IoT, and uses a
Python + YOLOv8 script to automatically detect a person or animal in the camera feed and email an alert
with the captured photo.

## Features

- 📹 Live MJPEG video stream served directly from the ESP32-CAM over WiFi
- 🖥️ Full-screen web viewer with **Capture Photo** and **Test Alert** buttons
- 🔔 Push notifications via the Blynk IoT app
- 🔊 Buzzer + flash LED deterrent trigger
- 🤖 AI-based detection (YOLOv8, pre-trained — no custom training required) for people/animals
- 📧 Automatic email alert with the captured photo when something is detected

## How It Works

1. The ESP32-CAM captures and streams live video, and exposes a `/capture` endpoint for single photos.
2. A Python script (running on a PC on the same WiFi network) periodically fetches a photo and runs it
   through a pre-trained YOLOv8 object-detection model.
3. If a person or animal is detected, the script emails an alert with the photo attached.
4. The ESP32-CAM can also be triggered manually (via the web page or Blynk) to sound a buzzer and flash
   an LED as a deterrent, along with a Blynk push notification.

## Hardware Required

| Component | Notes |
|---|---|
| ESP32-CAM (AI-Thinker) | Main microcontroller + camera |
| FTDI / USB-TTL programmer (5V) | For uploading code only |
| Buzzer (5V) | Deterrent sound, connected to GPIO 13 |
| Jumper wires, breadboard | For connections |
| 5V power adapter / power bank | For running the device after programming |

## Repository Structure

```
├── esp32-cam/
│   └── CameraWebServer_AnimalDeterrent.ino   # ESP32-CAM firmware
├── python-detection/
│   ├── detect_and_alert.py                   # AI detection + email alert script
│   └── requirements.txt
├── docs/
│   ├── Animal_Deterrent_ESP32CAM_Report.pdf  # Full build report (circuit, code, setup)
│   └── ESP32CAM_Learning_Notes.pdf           # Protocols/concepts learning notes
└── README.md
```

## Setup

### 1. ESP32-CAM Firmware
1. Open `esp32-cam/CameraWebServer_AnimalDeterrent.ino` in Arduino IDE.
2. Install the ESP32 board package and the **Blynk** library.
3. Fill in your `BLYNK_TEMPLATE_ID`, `BLYNK_TEMPLATE_NAME`, `BLYNK_AUTH_TOKEN` (from [blynk.cloud](https://blynk.cloud)) and your WiFi `ssid` / `password`.
4. Select **Board: AI Thinker ESP32-CAM**, Flash Mode: DIO, Partition Scheme: Huge APP.
5. Upload, then open `http://<device-ip>:81/` in a browser to view the live stream.

### 2. Python AI Detection Script
```bash
cd python-detection
pip install -r requirements.txt
```
Edit the `CONFIG` section in `detect_and_alert.py`:
- `ESP32_CAM_URL` — your ESP32-CAM's `/capture` URL
- `SENDER_EMAIL` / `SENDER_APP_PASSWORD` — a Gmail address + [App Password](https://support.google.com/accounts/answer/185833)
- `RECEIVER_EMAIL` — where alerts should be sent

Run it:
```bash
python detect_and_alert.py
```

Full setup details, circuit diagrams, and troubleshooting are in
[`docs/Animal_Deterrent_ESP32CAM_Report.pdf`](docs/Animal_Deterrent_ESP32CAM_Report.pdf).

## Endpoints

| URL | Function |
|---|---|
| `http://<ip>:81/` | Full-screen live stream page |
| `http://<ip>:81/stream` | Raw MJPEG stream |
| `http://<ip>:81/capture` | Single JPEG snapshot |
| `http://<ip>:81/trigger` | Manually sound buzzer + send Blynk alert |

## Possible Improvements

- PIR motion sensor to trigger capture instead of continuous polling
- Deep sleep for battery-powered field deployment
- On-device (edge) AI using ESP-WHO / Edge Impulse
- Face recognition to distinguish known people from strangers
- MQTT instead of Blynk's proprietary protocol

See [`docs/ESP32CAM_Learning_Notes.pdf`](docs/ESP32CAM_Learning_Notes.pdf) for a deeper breakdown of
protocols used and further learning directions.

## Author

**Girdhari Chaturvedi**
Diploma — Electronics & Telecommunication (ET&T)
