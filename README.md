# Water Level RX

This is a custom firmware for Wemos D1 (Arduino, esp8266) to act as a water level sensor with a web UI and HTTP API to access current readings.

## Device circuit

![Circuit diagram](./circuit/water-level-rx.svg)

## Build notes

To rebuild web assets, run `npm i` in `water-level-frontend` to install dependencies, then `npm run build` to re-generate `water-level-rx/server_assets_generated.cpp` file.
