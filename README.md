# ESP32 Mesh Network
Research project exploring wireless communication protocols between ESP32 microcontrollers.

## Objectives
- Implement ESP-NOW device-to-device communication
- Analyze protocol security and encryption
- Build a simple mesh network

## Architecture
```
    Handler (ESP32 #1)
        |
        | ESP-NOW broadcasts
        |
    +---+---+
    |       |
Agent #2  Agent #3
```

The Handler sends commands/messages wirelessly to multiple Agent nodes using ESP-NOW protocol.

## Requirerments
- 3x ESP32 DevKit boards
- USB cables
- Development environment: ESP-IDF

## Project Structure
```
esp32-mesh-network/
├── handler/          # Master node (sender)
│   └── main/
│       └── main.c
├── agent/            # Slave nodes (receivers)
│   └── main/
│       └── main.c
└── README.md
```

## Building & flashing
> Note: Hold BOOT button before flashing (At least, I have to do this.)

### Prerequisites
Install CP210x/CH340 USB drivers:
- [Silicon Labs CP210x Drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)

### MacOS (arm64 in my case)
```bash
brew install cmake ninja dfu-util

# setup ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32

# bbuild and flash
cd handler && get_idf
idf.py build
idf.py -p /dev/cu.usbserial-* flash monitor
```

### Windows
Manually install the [IDF tools](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/windows-setup.html#get-started-windows-tools-installer)

```powershell
# open "ESP-IDF PowerShell"

# build and flash
cd handler
idf.py build
idf.py -p COM3 flash monitor
```

## Usage
Flash Handler to one ESP32, Agent firmware to the others. Monitor serial output to observe communication.
