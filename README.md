# ESP32 Command Control
A simple handler-agent like network with encryption, uses the ESP-NOW protocol.

## Objectives
- Implement ESP-NOW device-to-device communication
- Analyze protocol security and encryption
- Build a simple network

retrospect: i got it all done

## Architecture
```
    Handler
        |
        | ESP-NOW broadcasts
        |
    +---+---+
    |       |
Agent #1  Agent #2
```

The Handler listens for special messages containing a shared secret (base64 encoded string) with certain types,
agent sends a type 0 msg then the hander receives, validates and adds the agent to a local list, waits for type2 messages (heartbeats)), which the first 1 is unencrypted the rest aare all encrypted.


## Requirerments
- 3x ESP32 DevKit boards
- USB cables
- Development environment: ESP-IDF

## Project Structure
```
esp32-mesh-network/
├── handler/          # (sender)
│   └── main/
│       └── main.c
├── agent/            # (receivers)
│   └── main/
│       └── main.c
└── README.md
```

## Building & flashing
> Note: Hold BOOT button before flashing (At least, I have to do this, on windows...)

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
cd handler / agent
# activate esp dev env (i use an alias)
idf.py build
idf.py -p /dev/cu.usbserial-* flash monitor
```

### Windows
Manually install the [IDF tools](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/windows-setup.html#get-started-windows-tools-installer)

```powershell
# open "ESP-IDF PowerShell"

# build and flash
cd handler/agent
idf.py build
idf.py -p COM3 flash monitor
```

## Usage
Flash Handler to one ESP32, Agent firmware to the others. Monitor serial output to observe communication.
