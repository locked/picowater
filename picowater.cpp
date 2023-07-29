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

#include "rtc_ds3231.h"


#define I2C_RTC_PORT i2c0

// uart
#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_RX_PIN 17
#define UART_TX_PIN 16

#define LED_PIN 0

#define BATTERY_ADC_PIN 26

#define HUMIDITY_POWER_PIN 3
#define HUMIDITY_ADC_PIN 27

#define DISTANCE_TRIG_PIN 1
#define DISTANCE_ECHO_PIN 2

#define RTC_POWER_PIN 10
#define RTC_SQW_PIN 11
#define RTC_SDA_PIN 12
#define RTC_SCL_PIN 13

#define MOTOR_PIN 4


void _i2c_init(i2c_inst_t* i2c, uint32_t sda, uint32_t scl, uint32_t baudrate)
{
    uint8_t idx = (i2c == i2c0) ? 0 : 1;
    
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


void recover_from_sleep(uint scb_orig, uint clock0_orig, uint clock1_orig) {
    // Re-enable ring Oscillator control
    rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);

    // Reset procs back to default
    scb_hw->scr = scb_orig;
    clocks_hw->sleep_en0 = clock0_orig;
    clocks_hw->sleep_en1 = clock1_orig;

    // Reset clocks
    clocks_init();
    //stdio_init_all();

    return;
}


void setup_adc() {
	uart_puts(UART_ID, "== ADC read ==\r\n");
    uart_default_tx_wait_blocking();
	printf("==ADC read==\n");
	adc_init();
    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(BATTERY_ADC_PIN);
    adc_gpio_init(HUMIDITY_ADC_PIN);
}


void setup_rtc() {
	char buf[50] = "";
	uart_puts(UART_ID, "== RTC_poweron ==\r\n");
    uart_default_tx_wait_blocking();
	gpio_init(RTC_POWER_PIN);
	gpio_set_dir(RTC_POWER_PIN, GPIO_OUT);
	gpio_put(RTC_POWER_PIN, 1);

	uart_puts(UART_ID, "== RTC_init ==\r\n");
    uart_default_tx_wait_blocking();
    printf("RTC_init\n");
    _i2c_init(I2C_RTC_PORT, RTC_SDA_PIN, RTC_SCL_PIN, 100000);

	// Fetch date from external RTC
	datetime_t dt;
	int ret = dev_ds3231_getdatetime(I2C_RTC_PORT, &dt);
	gpio_put(RTC_POWER_PIN, 0);
	sprintf(buf, "Get date from external RTC: [%d] %d-%02d-%02d %02d:%02d:%02d dotw:%d\r\n", ret, dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec, dt.dotw);
	uart_puts(UART_ID, buf);
	uart_default_tx_wait_blocking();
	printf(buf);

	if (dt.year < 2000) {
		// To set time
		dt.year = 2023;
		dt.month = 7;
		dt.day = 28;
		dt.dotw = 3;
		dt.hour = 19;
		dt.min = 41;
		dt.sec = 0;
		printf("Set date to external RTC\n");
		dev_ds3231_setdatetime(I2C_RTC_PORT, &dt);
	}

	if (dt.year > 2000) {
		uart_puts(UART_ID, "Set date to internal RTC\r\n");
		uart_default_tx_wait_blocking();
		printf("Set date to internal RTC\n");
		rtc_init();
		rtc_set_datetime(&dt);
		sleep_us(64);	// datetime is not updated immediately when rtc_get_datetime() is called.
	}
}


void setup_humidity() {
	gpio_init(HUMIDITY_POWER_PIN);
	gpio_set_dir(HUMIDITY_POWER_PIN, GPIO_OUT);
}


void setup_distance() {
	gpio_init(DISTANCE_ECHO_PIN);
	gpio_init(DISTANCE_TRIG_PIN);
	gpio_set_dir(DISTANCE_TRIG_PIN, GPIO_OUT);
	gpio_set_dir(DISTANCE_ECHO_PIN, GPIO_IN);
}

uint64_t measure_distance() {
	absolute_time_t pulse_start, pulse_end;
	uint32_t loop_count = 0;
	uint32_t loop_count2 = 0;
	gpio_put(DISTANCE_TRIG_PIN, 0);
	sleep_ms(100);
	gpio_put(DISTANCE_TRIG_PIN, 1);
	sleep_us(10);
	gpio_put(DISTANCE_TRIG_PIN, 0);
	while (gpio_get(DISTANCE_ECHO_PIN) == 0) tight_loop_contents();
    pulse_start = get_absolute_time();
	while (gpio_get(DISTANCE_ECHO_PIN) == 1) {
		sleep_us(1);
		loop_count2++;
		if (loop_count2 > 30000) {
			break;
		}
	}
	pulse_end = get_absolute_time();
	uint64_t pulse_duration = absolute_time_diff_us(pulse_start, pulse_end);
	uint64_t distance = pulse_duration / 29 / 2;
	char buf[100] = "";
	sprintf(buf, "pulse_start:[%u] pulse_end:[%u] pulse_duration:[%u] distance:[%u] loop_count:[%u/%u]\r\n", pulse_start, pulse_end, pulse_duration, distance, loop_count, loop_count2);
	uart_puts(UART_ID, buf);
	uart_default_tx_wait_blocking();
	printf(buf);
	return distance;
}


void add_water(datetime_t dt, uint loop_counter) {
	char buf[100] = "";
	// Read Battery and Humidity ADCs
    adc_select_input(0); // Select ADC input 0 (GPIO26)
	const float conversion_factor = 3.3f / (1 << 12);
	float battery_result = adc_read() * conversion_factor;
    adc_select_input(1); // Select ADC input 1 (GPIO27)
    gpio_put(HUMIDITY_POWER_PIN, 1);
	sleep_ms(10);
	float humidity_result = adc_read() * conversion_factor;
    gpio_put(HUMIDITY_POWER_PIN, 0);
	sprintf(buf, "ADC Battery:[%fV] Humidity:[%fV]\r\n", battery_result, humidity_result);
	uart_puts(UART_ID, buf);
	uart_default_tx_wait_blocking();
	printf(buf);

	// Temperature
	float temperature = rtc_ds3231_get_temp(I2C_RTC_PORT);
	sprintf(buf, "TEMP:[%f]\r\n", temperature);
	uart_puts(UART_ID, buf);
	uart_default_tx_wait_blocking();
	printf(buf);

	// Water level
	int dist = measure_distance();
	sprintf(buf, "DIST:[%u]\r\n", dist);
	uart_puts(UART_ID, buf);
	uart_default_tx_wait_blocking();
	printf(buf);

	// Decide to activate pump or not
	int activate_pump_ms = 0;
	if (dt.hour >= 8 && dt.hour <= 21) {
		sprintf(buf, "In time for pump:[%d]\r\n", dt.hour);
		uart_puts(UART_ID, buf);
		uart_default_tx_wait_blocking();
		printf(buf);
		if (dist < 14) {
			sprintf(buf, "Enough water for pump:[%d]\r\n", dist);
			uart_puts(UART_ID, buf);
			uart_default_tx_wait_blocking();
			printf(buf);
			if (humidity_result >= 3.0) {
				// Very dry
				activate_pump_ms = 10000;
			} else if (humidity_result >= 2.5) {
				// A bit dry
				activate_pump_ms = 7500;
			} else if (humidity_result >= 2.0) {
				// Not very dry
				activate_pump_ms = 5000;
			}
		}
	}

	if (activate_pump_ms > 0) {
		sprintf(buf, "Activate pump for [%dms]\r\n", activate_pump_ms);
		uart_puts(UART_ID, buf);
		uart_default_tx_wait_blocking();
		printf(buf);
		// Activate motor
		gpio_put(MOTOR_PIN, 0);
		sleep_ms(activate_pump_ms);
		gpio_put(MOTOR_PIN, 1);
	}


	// WIFI
	bool enable_wifi = true;
	if (enable_wifi) {
		uart_puts(UART_ID, "== Wifi connection ==\r\n");
		uart_default_tx_wait_blocking();
		printf("== Wifi connection ==\r\n");
		wifi_connect(WIFI_SSID, WIFI_PASSWORD);
		sleep_ms(100);

		datetime_t dt;
		rtc_get_datetime(&dt);
		char sendbuf[100] = "";
		sprintf(sendbuf, "[%d-%02d-%02d %02d:%02d:%02d] C:[%d] Bat:[%.2fV] Hum:[%.2fV] Temp:[%.2f] Water:[%d] Pump:[%d]",
			dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec, loop_counter, battery_result, humidity_result, temperature, dist, activate_pump_ms);
		int rt = send_udp(SERVER_IP, atoi(SERVER_PORT), sendbuf);
		sleep_ms(200);
		int rt2 = send_udp(SERVER_IP, atoi(SERVER_PORT), sendbuf);
		sprintf(buf, "rt:[%d] rt2:[%d] sent:[%s]\r\n", rt, rt2, sendbuf);
		uart_puts(UART_ID, buf);
		uart_default_tx_wait_blocking();

		sleep_ms(10);
		wifi_disconnect();
		sleep_ms(10);
		uart_puts(UART_ID, "Disconnected\r\n");
		uart_default_tx_wait_blocking();
	}
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
	sleep_ms(2000);
	gpio_put(LED_PIN, 0);
	printf("==START==\n");

	// Setup humidity sensor
	setup_humidity();

	// Init water level
	setup_distance();

	// ADC
	setup_adc();

	// RTC (own lib)
	setup_rtc();

	datetime_t dt;
	uint loop_counter = 0;
	char datetime_buf[256];
    char *datetime_str = &datetime_buf[0];
	uart_puts(UART_ID, "Start loop\r\n");
	uart_default_tx_wait_blocking();
	while (1) {
		gpio_put(LED_PIN, 1);

		rtc_get_datetime(&dt);
		datetime_to_str(datetime_str, sizeof(datetime_buf), &dt);
        sprintf(buf, "%s      \r\n", datetime_str);
		uart_puts(UART_ID, buf);
		uart_default_tx_wait_blocking();
		printf(buf);

		add_water(dt, loop_counter);
		loop_counter++;

		// Alarm xx seconds later
		datetime_t t_alarm;
		rtc_get_datetime(&t_alarm);
		bool wakeup_everymin = false;
		if (wakeup_everymin) {
			t_alarm.min += 1;
			if (t_alarm.min > 59) {
				t_alarm.hour += 1;
				t_alarm.min -= 60;
			}
		} else {
			t_alarm.hour += 1;
		}
		if (t_alarm.hour > 23) {
			t_alarm.day += 1;
			t_alarm.hour = t_alarm.hour - 24;
		}
		if (t_alarm.day > 30) {
			t_alarm.month += 1;
			t_alarm.day = t_alarm.day - 30;
		}
		sleep_ms(10);
		sprintf(buf, "Sleep until: %d-%02d-%02d %02d:%02d:%02d\r\n", t_alarm.year, t_alarm.month, t_alarm.day, t_alarm.hour, t_alarm.min, t_alarm.sec);
		uart_puts(UART_ID, buf);
		uart_default_tx_wait_blocking();

		//sleep_goto_sleep_until(&t_alarm, &sleep_callback);
		uart_puts(UART_ID, "Set alarm\r\n");
		uart_default_tx_wait_blocking();
		dev_ds3231_setalarm(I2C_RTC_PORT, t_alarm.min, t_alarm.hour, t_alarm.day);

		uart_puts(UART_ID, "Alarm set (2)\r\n");
		uart_default_tx_wait_blocking();

		gpio_put(LED_PIN, 0);
		sleep_ms(10);

		// Sleep until edge on SQW pin
		sleep_goto_dormant_until_pin(RTC_SQW_PIN, true, false);

		recover_from_sleep(scb_orig, clock0_orig, clock1_orig);

		uart_puts(UART_ID, "Recovered from sleep\r\n");
		uart_default_tx_wait_blocking();
	}
	return 0;
}
