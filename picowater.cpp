#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/sleep.h"

#include "hardware/rtc.h"

#include "hardware/uart.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"

#include "hardware/i2c.h"

#include "pico/util/datetime.h"

#include "hardware/clocks.h"
#include "hardware/rosc.h"
#include "hardware/structs/scb.h"

#include "wifi.h"
//#include "ds3232rtc.hpp"

//#include "sys_fn.h"
//#include "sys_time.h"
//#include "sys_i2c.h"
//#include "dev_ds3231.h"


// uart
#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_RX_PIN 17
#define UART_TX_PIN 16

#define ADC_PIN 26

#define RTC_SQW_PIN 11
#define RTC_SDA_PIN 12
#define RTC_SCL_PIN 13

#define I2C_RTC_PORT i2c0
#define I2C_DS3231_ADDR 0x68
#define I2C_TIMEOUT_CHAR 500

// mutex for i2c
//auto_init_mutex(i2c_mutex);
//#define ENTER_SECTION mutex_enter_blocking(&i2c_mutex)
//#define EXIT_SECTION  mutex_exit(&i2c_mutex);

// convert BCD to number
static inline uint8_t bcdnum(uint8_t bcd)
{
    return ((bcd/16) * 10) + (bcd % 16);
}
// convert number to BCD
static inline uint8_t numbcd(uint8_t num)
{
    return ((num/10) * 16) + (num % 10);
}
bool dev_ds3231_setdatetime(i2c_inst_t* i2c, datetime_t *dt)
{
    uint8_t dt_buffer[8];
    dt_buffer[0] = 0;
    dt_buffer[1] = numbcd(dt->sec);
    dt_buffer[2] = numbcd(dt->min);
    dt_buffer[3] = numbcd(dt->hour);
    dt_buffer[4] = numbcd(dt->dotw + 1);
    dt_buffer[5] = numbcd(dt->day);
    dt_buffer[6] = numbcd(dt->month);
    dt_buffer[7] = numbcd(dt->year - 2000);

    uint len = sizeof(dt_buffer);
	return i2c_write_timeout_us(i2c, I2C_DS3231_ADDR, dt_buffer, len, false, len * I2C_TIMEOUT_CHAR) != len;
}
int dev_ds3231_getdatetime(i2c_inst_t* i2c, datetime_t *dt)
{
    uint8_t dt_buffer[7];
    uint len = sizeof(dt_buffer);

    //ENTER_SECTION;
    int32_t ret = i2c_write_timeout_us(i2c, I2C_DS3231_ADDR, 0, 1, true, I2C_TIMEOUT_CHAR);
    if (ret > 0)
        ret = i2c_read_timeout_us(i2c, I2C_DS3231_ADDR, dt_buffer, len, false, len * I2C_TIMEOUT_CHAR);
    //EXIT_SECTION;

    if (ret != len)
        return 1;

    dt->sec = bcdnum(dt_buffer[0]);
    dt->min = bcdnum(dt_buffer[1]);
    dt->hour = bcdnum(dt_buffer[2]);
    dt->dotw = bcdnum(dt_buffer[3]) - 1;
    dt->day = bcdnum(dt_buffer[4]);
    dt->month = bcdnum(dt_buffer[5] & 0x1F);
    dt->year = bcdnum(dt_buffer[6]) + 2000;

    return 0;
}
void _i2c_init(i2c_inst_t* i2c, uint32_t sda, uint32_t scl, uint32_t baudrate)
{
    uint8_t idx = (i2c == i2c0) ? 0 : 1;
   
	//static uint32_t i2c_baudrate[] = {0, 0};
    // init i2c
    //i2c_baudrate[idx] = baudrate;
    
    i2c_init(i2c, baudrate);

    // init pins
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);

	gpio_pull_up(sda);
	gpio_pull_up(scl);
}

static void sleep_callback(void) {
    printf("RTC woke us up\n");
}

void recover_from_sleep(uint scb_orig, uint clock0_orig, uint clock1_orig){

    //Re-enable ring Oscillator control
    rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);

    //reset procs back to default
    scb_hw->scr = scb_orig;
    clocks_hw->sleep_en0 = clock0_orig;
    clocks_hw->sleep_en1 = clock1_orig;

    //reset clocks
    clocks_init();
    stdio_init_all();

    return;
}

int main() {
	uart_init(UART_ID, BAUD_RATE);
	gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
	gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uint scb_orig = scb_hw->scr;
    uint clock0_orig = clocks_hw->sleep_en0;
    uint clock1_orig = clocks_hw->sleep_en1;


	const uint LED_PIN = 25;
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);
	gpio_put(LED_PIN, 1);

	uart_puts(UART_ID, "==START==\r\n");

	stdio_init_all();
	sleep_ms(3000);
	printf("==START==\n");
	uart_default_tx_wait_blocking();


	// ADC
	uart_puts(UART_ID, "== ADC read ==\r\n");
	printf("==ADC read==\n");
	//gpio_set_dir_all_bits(0);
	//gpio_set_function(26, GPIO_FUNC_SIO);
	adc_init();
    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(ADC_PIN);
    // Select ADC input 0 (GPIO26)
    adc_select_input(0);
	uint32_t result = adc_read();
	const float conversion_factor = 3.3f / (1 << 12);
	char buf[50] = "";
	sprintf(buf, "\n0x%03x -> %f V\n", result, result * conversion_factor);
	uart_puts(UART_ID, buf);
	printf(buf);


	// RTC (own lib)
    printf("RTC_init\n");
    _i2c_init(I2C_RTC_PORT, RTC_SDA_PIN, RTC_SCL_PIN, 100000);

	/*
	// To set time
	datetime_t tset;
    tset.year = 2023;
    tset.month = 6;
    tset.day = 12;
    tset.dotw = 1;
    tset.hour = 20;
    tset.min = 56;
    tset.sec = 30;
	dev_ds3231_setdatetime(I2C_RTC_PORT, &tset);
	*/

	datetime_t dt;
	int ret = dev_ds3231_getdatetime(I2C_RTC_PORT, &dt);
	printf("[%d] %d-%d-%d %d:%d:%d dotw:%d\n", ret, dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec, dt.dotw);

    printf("Set alarm\n");
	rtc_init();
    rtc_set_datetime(&dt);

    // Alarm 10 seconds later
    datetime_t t_alarm = dt;
    t_alarm.sec = t_alarm.sec + 10;
    if (t_alarm.sec > 59) {
		t_alarm.min += 1;
		t_alarm.sec = 60 - t_alarm.sec;
	}
	gpio_put(LED_PIN, 0);
	printf("Go to sleep until: %d-%d-%d %d:%d:%d dotw:%d\n", t_alarm.year, t_alarm.month, t_alarm.day, t_alarm.hour, t_alarm.min, t_alarm.sec, t_alarm.dotw);
	sleep_goto_sleep_until(&t_alarm, &sleep_callback);

    printf("After sleep\n");
    recover_from_sleep(scb_orig, clock0_orig, clock1_orig);
	gpio_put(LED_PIN, 1);

	// RTC (dev/sys lib)
    //int8_t buffer[30];
    //int8_t buffer_time[30];
    //int8_t buffer_date[30];
    // init buffer
    //memset(buffer_time, 0, sizeof(buffer_time));
    //memset(buffer_date, 0, sizeof(buffer_date));
    //printf("sys_init\n");
    //sys_init();
    // init i2c1 with pins and 100kHz
    //printf("sys_i2c_init\n");
    //sys_i2c_init(RTC_PORT, RTC_SDA_PIN, RTC_SCL_PIN, 100000, true);
    //datetime_t dt;

    //printf("dev_ds3231_getdatetime\n");
    // get date time from DS3231 
    //if (!dev_ds3231_getdatetime(RTC_PORT, &dt))
    //    printf("Error on read DS3231");
    //printf("sys_getrtc_format\n");
	//sys_getrtc_format((int8_t*)"%H:%M:%S", buffer, sizeof(buffer));
	//printf((char*)buffer);
	//sys_getrtc_format((int8_t*)"%d.%m.%Y %a", buffer, sizeof(buffer));
	//printf((char*)buffer);


	// RTC (old lib)
	/*
    printf("DS3231 init\n");
    i2c_inst_t *i2c   = i2c0;
    i2c_init(i2c, 400000);
    gpio_set_function(RTC_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(RTC_SDA_PIN);
    gpio_set_function(RTC_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(RTC_SCL_PIN);
    rtc_init();
	DS3231_get_treg();

    printf("DS3231 init8\n");
    //DS3231_clear_a2f();
    printf("DS3231 init9\n");
    DS3231_init(DS3231_CONTROL_INTCN);

    printf("DS3231 settime\n");
    struct ts tset;
    tset.year = 2023;
    tset.mon = 6;
    tset.mday = 4;
    tset.wday = 1;
    tset.hour = 11;
    tset.min = 55;
    tset.sec = 1;
    DS3231_set(tset);

    printf("DS3231 readtime\n");
    ds3231_readtime();
	sleep_ms(2000);
    ds3231_readtime();
	sleep_ms(2000);
    ds3231_readtime();
	*/

	// WIFI
	uart_puts(UART_ID, "== Wifi init ==\r\n");
	printf("==Wifi init==\r\n");
	wifi_connect(WIFI_SSID, WIFI_PASSWORD);

	run_tcp_client_test(SERVER_IP, atoi(SERVER_PORT));

	run_udp_beacon(SERVER_IP, atoi(SERVER_PORT));

	wifi_disconnect();
	return 0;
}




/*
void ds3231_readtime(void){
    char buff[128];
    struct ts t;
    DS3231_get(&t);
    snprintf(buff, 128, "%d.%02d.%02d %02d:%02d:%02d", t.year,
            t.mon, t.mday, t.hour, t.min, t.sec);

    printf("Time from lib:");
    printf(buff);
    printf("\n");
}


    i2c_inst_t *i2c   = i2c0;
    static const uint8_t DEFAULT_SDA_PIN        = 20;
    static const uint8_t DEFAULT_SCL_PIN        = 21;
    int8_t sda        = DEFAULT_SDA_PIN;
    int8_t scl        = DEFAULT_SCL_PIN;
    i2c_init(i2c, 400000);

    printf("I2C init\n");
    uart_default_tx_wait_blocking();
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_pull_up(sda);
    gpio_set_function(scl, GPIO_FUNC_I2C);
    gpio_pull_up(scl);

    rtc_init();

    sleep_ms(500);
    DS3231_clear_a2f();
    printf("DS3231 init\n");
    uart_default_tx_wait_blocking();
    DS3231_init(DS3231_CONTROL_INTCN);

    printf("DS3231 settime\n");
    struct ts tset;
    tset.year = 2023;
    tset.mon = 6;
    DS3231_set(tset);

    printf("DS3231 readtime\n");
    uart_default_tx_wait_blocking();
    ds3231_readtime();

    int sleep_mins = 1;
    struct ts t;
    DS3231_get(&t);
    // calculate the minute when the next alarm will be triggered
    int wakeup_min = (t.min / sleep_mins + 1) * sleep_mins;
    if (wakeup_min > 59) {
        wakeup_min -= 60;
    }
    printf("wakeup_min:%d\n", wakeup_min);

    uint8_t flags[4] = { 0, 1, 1, 1 };

    // set Alarm2. only the minute is set since we ignore the hour and day component
    DS3231_set_a2(wakeup_min, 0, 0, flags);

    // activate Alarm2 and enable Battery Backed SQW
    DS3231_set_creg(DS3231_CONTROL_INTCN | DS3231_CONTROL_A2IE | DS3231_CONTROL_BBSQW);
    sleep_ms(100);

    gpio_pull_up(22);
    sleep_goto_dormant_until_pin(22, true, false);
*/
