cd build

cmake -DPICO_BOARD=pico_w -DWIFI_SSID="<wifi>" -DWIFI_PASSWORD="<secret>" ..

make
