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

// uart
#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_RX_PIN 17
#define UART_TX_PIN 16

#define LED_PIN 25

#define ADC_PIN 26

#define RTC_POWER_PIN 10
#define RTC_SQW_PIN 11
#define RTC_SDA_PIN 12
#define RTC_SCL_PIN 13

#define MOTOR_PIN 1

#define I2C_RTC_PORT i2c0
#define I2C_DS3231_ADDR 0x68
#define I2C_TIMEOUT_CHAR 500

#define DS3231_ALARM2_ADDR      0x0B
#define DS3231_CONTROL_ADDR     0x0E
#define DS3231_STATUS_ADDR      0x0F
#define DS3231_TEMPERATURE_ADDR 0x11

// control register bits
#define DS3231_CONTROL_A1IE     0x1		/* Alarm 2 Interrupt Enable */
#define DS3231_CONTROL_A2IE     0x2		/* Alarm 2 Interrupt Enable */
#define DS3231_CONTROL_INTCN    0x4		/* Interrupt Control */
#define DS3231_CONTROL_RS1	    0x8		/* square-wave rate select 2 */
#define DS3231_CONTROL_RS2    	0x10	/* square-wave rate select 2 */
#define DS3231_CONTROL_CONV    	0x20	/* Convert Temperature */
#define DS3231_CONTROL_BBSQW    0x40	/* Battery-Backed Square-Wave Enable */
#define DS3231_CONTROL_EOSC	    0x80	/* not Enable Oscillator, 0 equal on */

// status register bits
#define DS3231_STATUS_A1F      0x01		/* Alarm 1 Flag */
#define DS3231_STATUS_A2F      0x02		/* Alarm 2 Flag */
#define DS3231_STATUS_BUSY     0x04		/* device is busy executing TCXO */
#define DS3231_STATUS_EN32KHZ  0x08		/* Enable 32KHz Output  */
#define DS3231_STATUS_OSF      0x80		/* Oscillator Stop Flag */

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


void i2c_write_bytes(i2c_inst_t* i2c, uint8_t addr, uint8_t *buf, int len) {
    uint8_t buffer[len + 1];
    buffer[0] = addr;
    for(int x = 0; x < len; x++) {
      buffer[x + 1] = buf[x];
    }
    i2c_write_timeout_us(i2c, I2C_DS3231_ADDR, buffer, len + 1, false, len * I2C_TIMEOUT_CHAR);
};

void DS3231_set_sreg(i2c_inst_t* i2c, const uint8_t val)
{
    uint8_t buffer[1] = { val };
    i2c_write_bytes(i2c, DS3231_STATUS_ADDR, buffer, 1);
}

uint8_t DS3231_get_sreg(i2c_inst_t* i2c)
{
    uint8_t buffer[1];
    i2c_read_timeout_us(i2c, DS3231_STATUS_ADDR, buffer, 1, false, I2C_TIMEOUT_CHAR);
    return buffer[0];
}

void dev_ds3231_setalarm(i2c_inst_t* i2c, const uint8_t mi, const uint8_t h, const uint8_t d)
{
	// Clear A2F
    uint8_t reg_val;
    reg_val = DS3231_get_sreg(i2c) & ~DS3231_STATUS_A2F;
    DS3231_set_sreg(i2c, reg_val);


    uint8_t buffer[1] = { DS3231_CONTROL_INTCN };
    i2c_write_bytes(i2c, DS3231_CONTROL_ADDR, buffer, 1);
	uart_puts(UART_ID, "write to DS3231_CONTROL_ADDR (1) done\r\n");
	uart_default_tx_wait_blocking();

	// flags are: A2M2 (minutes), A2M3 (hour), A2M4 (day) 0 to enable, 1 to disable, DY/DT (dayofweek == 1/dayofmonth == 0) - 
	uint8_t flags[4] = { 0, 1, 1, 1 };

    // set Alarm2. only the minute is set since we ignore the hour and day component
    uint8_t t[3] = { mi, h, d };
    uint8_t send_t[3];
    uint8_t i;

    for (i = 0; i <= 2; i++) {
        if (i == 2) {
            send_t[i] = numbcd(t[2]) | (flags[2] << 7) | (flags[3] << 6);
        } else {
            send_t[i] = numbcd(t[i]) | (flags[i] << 7);
		}
    }
    i2c_write_bytes(i2c, DS3231_ALARM2_ADDR, send_t, 3);
	uart_puts(UART_ID, "write to DS3231_ALARM2_ADDR done\r\n");
	uart_default_tx_wait_blocking();

	// set control register to activate Alarm2 and enable Battery Backed SQW
    buffer[0] = DS3231_CONTROL_INTCN | DS3231_CONTROL_A2IE; // | DS3231_CONTROL_BBSQW;
    i2c_write_bytes(i2c, DS3231_CONTROL_ADDR, buffer, 1);
	uart_puts(UART_ID, "write to DS3231_CONTROL_ADDR (2) done\r\n");
	uart_default_tx_wait_blocking();
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
	uart_puts(UART_ID, "RTC woke us up\r\n");
	uart_default_tx_wait_blocking();
}


void recover_from_sleep(uint scb_orig, uint clock0_orig, uint clock1_orig){

    //Re-enable ring Oscillator control
    rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);
	//uart_puts(UART_ID, "Re-enabled ring Oscillator control\r\n");
	//uart_default_tx_wait_blocking();

    //reset procs back to default
    scb_hw->scr = scb_orig;
    clocks_hw->sleep_en0 = clock0_orig;
    clocks_hw->sleep_en1 = clock1_orig;
	//uart_puts(UART_ID, "reseted procs back to default\r\n");
	//uart_default_tx_wait_blocking();

    //reset clocks
    clocks_init();
    //stdio_init_all();
	//uart_puts(UART_ID, "reseted clocks\r\n");
	//uart_default_tx_wait_blocking();

    return;
}


int main() {
	char buf[50] = "";

	uart_init(UART_ID, BAUD_RATE);
	gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
	gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uint scb_orig = scb_hw->scr;
    uint clock0_orig = clocks_hw->sleep_en0;
    uint clock1_orig = clocks_hw->sleep_en1;


	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);
	gpio_put(LED_PIN, 1);

	gpio_init(MOTOR_PIN);
	gpio_set_dir(MOTOR_PIN, GPIO_OUT);
	gpio_put(MOTOR_PIN, 1);	// enable low

	uart_puts(UART_ID, "==START==\r\n");
    uart_default_tx_wait_blocking();

	gpio_pull_up(RTC_SQW_PIN);

	stdio_init_all();
	sleep_ms(500);
	gpio_put(LED_PIN, 0);
	printf("==START==\n");


	// ADC
	uart_puts(UART_ID, "== ADC read ==\r\n");
    uart_default_tx_wait_blocking();
	printf("==ADC read==\n");
	//gpio_set_dir_all_bits(0);
	//gpio_set_function(26, GPIO_FUNC_SIO);
	adc_init();
    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(ADC_PIN);
    // Select ADC input 0 (GPIO26)
    adc_select_input(0);


	// RTC (own lib)
	uart_puts(UART_ID, "== RTC_poweron ==\r\n");
    uart_default_tx_wait_blocking();
	gpio_init(RTC_POWER_PIN);
	gpio_set_dir(RTC_POWER_PIN, GPIO_OUT);
	gpio_put(RTC_POWER_PIN, 1);

	uart_puts(UART_ID, "== RTC_init ==\r\n");
    uart_default_tx_wait_blocking();
    printf("RTC_init\n");
    _i2c_init(I2C_RTC_PORT, RTC_SDA_PIN, RTC_SCL_PIN, 100000);

	/*
	// To set time
	datetime_t tset;
    tset.year = 2023;
    tset.month = 7;
    tset.day = 12;
    tset.dotw = 3;
    tset.hour = 19;
    tset.min = 41;
    tset.sec = 0;
    printf("Set date to external RTC\n");
	dev_ds3231_setdatetime(I2C_RTC_PORT, &tset);
	*/


	// Fetch date from external RTC
	datetime_t dt;
	int ret = dev_ds3231_getdatetime(I2C_RTC_PORT, &dt);
	gpio_put(RTC_POWER_PIN, 0);
	sprintf(buf, "Get date from external RTC: [%d] %d-%d-%d %d:%d:%d dotw:%d\r\n", ret, dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec, dt.dotw);
	uart_puts(UART_ID, buf);
	uart_default_tx_wait_blocking();
	printf(buf);

	uart_puts(UART_ID, "Set date to internal RTC\r\n");
	uart_default_tx_wait_blocking();
	printf("Set date to internal RTC\n");
	rtc_init();
	rtc_set_datetime(&dt);
	sleep_us(64);	// datetime is not updated immediately when rtc_get_datetime() is called.

	bool need_water = false;
	int loop_counter = 0;
	char datetime_buf[256];
    char *datetime_str = &datetime_buf[0];
	while (1) {
		rtc_get_datetime(&dt);
		datetime_to_str(datetime_str, sizeof(datetime_buf), &dt);
        sprintf(buf, "%s      \r\n", datetime_str);
		uart_puts(UART_ID, buf);
		uart_default_tx_wait_blocking();
		printf(buf);

		gpio_put(LED_PIN, 1);


		need_water = false;
		if (//dt.hour == 22 &&
			(dt.min == 0 || dt.min == 10 || dt.min == 20 || dt.min == 30 || dt.min == 40 || dt.min == 50)) {
			need_water = true;
		}
		need_water = true;
		if (need_water) {
			// Read ADC
			uint32_t result = adc_read();
			const float conversion_factor = 3.3f / (1 << 12);
			sprintf(buf, "\n0x%03x -> %f V\r\n", result, result * conversion_factor);
			uart_puts(UART_ID, buf);
			uart_default_tx_wait_blocking();
			printf(buf);


			// Activate motor if in the right time slot
			gpio_put(MOTOR_PIN, 0);
			sleep_ms(4000);
			gpio_put(MOTOR_PIN, 1);


			// WIFI
			bool enable_wifi = true;
			if (enable_wifi) {
				uart_puts(UART_ID, "== Wifi connection ==\r\n");
				uart_default_tx_wait_blocking();
				printf("== Wifi connection ==\r\n");
				wifi_connect(WIFI_SSID, WIFI_PASSWORD);

				send_udp(SERVER_IP, atoi(SERVER_PORT), loop_counter);

				loop_counter++;

				wifi_disconnect();
			}
		}

		gpio_put(LED_PIN, 0);


		// Alarm 10 seconds later
		datetime_t t_alarm = dt;
		// TODO: fix this
		t_alarm.sec = t_alarm.sec + 90;
		while (t_alarm.sec > 59) {
			t_alarm.min += 1;
			t_alarm.sec -= 60;
		}
		if (t_alarm.min > 59) {
			t_alarm.hour += 1;
			t_alarm.min -= 60;
		}
		if (t_alarm.hour > 23) {
			t_alarm.day += 1;
			t_alarm.hour = t_alarm.hour - 24;
		}
		if (t_alarm.day > 30) {
			t_alarm.month += 1;
			t_alarm.day = t_alarm.day - 30;
		}
		sprintf(buf, "Go to sleep until: %d-%d-%d %d:%d:%d dotw:%d\r\n", t_alarm.year, t_alarm.month, t_alarm.day, t_alarm.hour, t_alarm.min, t_alarm.sec, t_alarm.dotw);
		uart_puts(UART_ID, buf);
		uart_default_tx_wait_blocking();
		printf(buf);
		//sleep_goto_sleep_until(&t_alarm, &sleep_callback);
		uart_puts(UART_ID, "Set alarm\r\n");
		uart_default_tx_wait_blocking();
		dev_ds3231_setalarm(I2C_RTC_PORT, t_alarm.min, t_alarm.hour, t_alarm.day);
		uart_puts(UART_ID, "Alarm set (2)\r\n");
		uart_default_tx_wait_blocking();
		sleep_ms(10);
		sleep_goto_dormant_until_pin(RTC_SQW_PIN, true, false);

		//uart_puts(UART_ID, "After sleep\r\n");
		//uart_default_tx_wait_blocking();
		//printf("After sleep\n");
		recover_from_sleep(scb_orig, clock0_orig, clock1_orig);

		uart_puts(UART_ID, "Recovered from sleep\r\n");
		uart_default_tx_wait_blocking();
	}
	return 0;
}
