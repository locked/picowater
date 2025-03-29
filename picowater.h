#define I2C_RTC_PORT i2c0

// uart
#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_RX_PIN 17
#define UART_TX_PIN 16

#define LED_PIN 0

#define BATTERY_ADC_PIN 26   // 26 v0.9

#define HUMIDITY_POWER_PIN 1 // 1  v0.9
#define HUMIDITY_ADC_PIN 28  // 28 v0.9

#define DISTANCE_TRIG_PIN 4  // 4 for v0.9, 4 for v0.3, 1 for v0.2
#define DISTANCE_ECHO_PIN 5  // 5 for v0.9, 5 for v0.3, 2 for v0.2

#define SCREEN_SDA_PIN 18    // 18 for v0.9
#define SCREEN_SCL_PIN 19    // 19 for v0.9

#define RTC_POWER_PIN 10     // NC for v0.9, 10 for v0.3
#define RTC_WORK_DONE_PIN 10 // 10 for v0.9
#define RTC_SQW_PIN 14       // 14 for v0.9, 14 for v0.3, 11 for v0.2
#define RTC_SDA_PIN 12       // 12 for v0.9, 12 for v0.3
#define RTC_SCL_PIN 13       // 13 for v0.9, 13 for v0.3

#define PUMP_PIN 27          // 27 for v0.9, 21 for v0.3, 4 for v0.2

#define SERVER_IP "5.196.73.46"
#define SERVER_PORT 4242
