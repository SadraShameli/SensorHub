## Sensor Hub Firmware

This project is created as a full-stack application and is intended to gather various data such as temperature, humidity, air pressure, gas resistance, altitude, noise recordings, loudness levels and RPM values. The devices are placed at various locations in The Netherlands and are continuously sending data to my website: [sadra.nl](https://sadra.nl)

> [!NOTE]
> Sensor Hub is powered by ESP32 boards and it has in total 4 different device types:
>
> - Sensor Unit
> - Sound Unit
> - HMI Unit
> - RPM Unit

### Features

- Supporting **multithreading** for all core functionality. Tasks are created, suspended and deleted when needed.
  - This results to a boot time of only **100ms** when running in release mode.
- Advanced **failsafe** system and logging mechanisms to notify users of potential errors and bugs.
  - Saving a list of last **25** failures that occurred during the runtime.
- Using **XOR** bitwise operations with the **mac address** to encrypt user data before saving to the storage.
- Custom **Pin**, **WiFi**, **HTTP** and **Gui** classes to provide simple APIs for ease of us.
- Two runtime modes: **normal** and a **configuration** mode that creates a new access point and HTTP server.
  - You can connect to the WiFi created by the device and open the configuration page by navigating to the URL: [192.168.4.1](http://192.168.4.1)
  - Once you submit, the device will connect to the backend, fetch it's configuration, save it to the storage and reboot in normal mode.
- Handling runtime and backend errors and notifying users with a failsafe when they occur.

### Supported modules

- Climate
  - BME680
- Display
  - SSD1306
  - HD44780
- Microphone
  - INMP441
- LEDs
  - RGB LED Strip

---

Disclaimer: Please note that this project is currently under development. Various tests are conducted to make it bug-free, but there is always the possibility of errors.

Created with ♥ and maintained by Sadra Shameli. All Rights Reserved.
