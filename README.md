# septic-iot
IoT project to automate operation and monitoring of home septic system.

## Hardware
NodeMCU ESP-12F [ESP8266] [http://nodemcu.com/index_en.html](http://nodemcu.com/index_en.html)

## Firmware
Mongoose-OS - [https://mongoose-os.com](https://mongoose-os.com)

### Languages
- Javascript (mJS)

### Project
This is an experimental branch to quickly dev using mjs (javascript) on mongoose-os. Unfortunately
this can't run in production because Cron and Crontab do not have mjs API's and using
`ffi()` I was not able to get the callback to work properly.

Leaving here for a rainy day just in case.
