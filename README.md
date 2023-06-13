cd build

cmake -DPICO_BOARD=pico_w -DWIFI_SSID="<wifi>" -DWIFI_PASSWORD="<secret>" -DSERVER_IP="<ip>" -DSERVER_PORT=<port> ..

make


# Sources

https://github.com/iiot2k/pico-lib2/blob/main/src/dev/dev_ds3231/dev_ds3231.c

https://ghubcoder.github.io/posts/awaking-the-pico/

https://github.com/ghubcoder/PicoSleepRtc/blob/master/ds3232rtc.cpp

https://github.com/raspberrypi/pico-examples/blob/master/adc/hello_adc/hello_adc.c
