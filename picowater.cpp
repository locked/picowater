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

//#include "rtc_ds3231.h"

#include "ssd1306.h"
#include "acme_5_outlines_font.h"
#include "bubblesstandard_font.h"
#include "crackers_font.h"
#include "BMSPA_font.h"

#include "sensor/PCF8563.h"

#include "picowater.h"



void _i2c_init(i2c_inst_t* i2c, uint32_t sda, uint32_t scl, uint32_t baudrate) {
	uint8_t idx = (i2c == i2c0) ? 0 : 1;

	i2c_init(i2c, baudrate);

	// init pins
	gpio_set_function(sda, GPIO_FUNC_I2C);
	gpio_set_function(scl, GPIO_FUNC_I2C);

	gpio_pull_up(sda);
	gpio_pull_up(scl);
}



void setup_screen(ssd1306_t *disp) {
	i2c_init(i2c1, 400000);
	gpio_set_function(SCREEN_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(SCREEN_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(SCREEN_SDA_PIN);
	gpio_pull_up(SCREEN_SCL_PIN);
	disp->external_vcc = false;
	ssd1306_init(disp, 128, 64, 0x3C, i2c1);
	ssd1306_clear(disp);
}


void setup_adc() {
	uart_puts(UART_ID, "== Setup ADC ==\r\n");
	uart_default_tx_wait_blocking();
	printf("== Setup ADC ==\n");
	adc_init();
	// Make sure GPIO is high-impedance, no pullups etc
	adc_gpio_init(BATTERY_ADC_PIN);
	adc_gpio_init(HUMIDITY_ADC_PIN);
}


Rtc_Pcf8563 rtc;
void setup_rtc_pcf() {
	uart_puts(UART_ID, "== RTC PCF8563 init ==\r\n");
	uart_default_tx_wait_blocking();
	printf("RTC PCF8563 init\n");

	//_i2c_init(I2C_RTC_PORT, RTC_SDA_PIN, RTC_SCL_PIN, 100000);
	i2c_init(I2C_RTC_PORT, 100 * 1000);
	gpio_set_function(RTC_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(RTC_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(RTC_SDA_PIN);
	gpio_pull_up(RTC_SCL_PIN);

	rtc.begin(I2C_RTC_PORT);
	//rtc.initClock();
	//rtc.clearStatus();

	rtc.getDateTime();
	if (rtc.getCentury() != 0 || (rtc.getYear() < 20 || rtc.getYear() > 30)) {
		printf("SET RTC century:%d year:%d\n", rtc.getCentury(), rtc.getYear());
		rtc.setDateTime(1, 2, 3, false, 24, 4, 5, 6);
	}
}

void set_dt_from_rtc_pcf(datetime_t *dt) {
	rtc.getDateTime();
	sleep_us(64);

	char buf[100] = "";
	sprintf(buf, "Time: %s %s year:%d UTC\n", rtc.formatDate(RTCC_DATE_WORLD), rtc.formatTime(), rtc.getYear());
	printf(buf);

	dt->year = rtc.getYear();
	dt->month = rtc.getMonth();
	dt->day = rtc.getDay();
	dt->hour = rtc.getHour();
	dt->min = rtc.getMinute();
	dt->sec = rtc.getSecond();
	/*
	uart_puts(UART_ID, "Set date to internal RTC\r\n");
	uart_default_tx_wait_blocking();
	printf("Set date to internal RTC\n");
	rtc_init();
	rtc_set_datetime(dt);
	sleep_us(64);	// datetime is not updated immediately when rtc_get_datetime() is called.
	*/

	//rtc.setTimer(1, TMR_1MIN, true);
	//rtc.enableTimer();
}

void rtc_pcf_sleep() {
	printf("Will set ALARM\n");
	uart_default_tx_wait_blocking();
	sleep_ms(100);

	rtc.getDateTime();
	byte min = rtc.getMinute() + 1;
	byte hour = rtc.getHour();
	byte day = rtc.getDay();
	byte weekday = rtc.getWeekday();

	printf("Set ALARM to day:%d hour:%d min:%d\n", day, hour, min);
	uart_default_tx_wait_blocking();
	sleep_ms(100);

	rtc.setAlarm(min, hour, day, weekday);
	rtc.enableAlarm();

	// shutdown PI through SN74HC74N
	printf("SHUTDOWN\n");
	uart_default_tx_wait_blocking();
	sleep_ms(100);
	gpio_put(RTC_WORK_DONE_PIN, 1);
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
	while (gpio_get(DISTANCE_ECHO_PIN) == 0) {
		if (loop_count++ > 3000000) {
			break;
		}
	}
	//tight_loop_contents();
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


void add_water(datetime_t *dt) {
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
	sprintf(buf, "POUET ADC Battery:[%fV] Humidity:[%fV]\r\n", battery_result, humidity_result);
	uart_puts(UART_ID, buf);
	uart_default_tx_wait_blocking();
	printf(buf);

	// Temperature
	float temperature = 0;
	/*float temperature = rtc_ds3231_get_temp(I2C_RTC_PORT);
	sprintf(buf, "TEMP:[%f]\r\n", temperature);
	uart_puts(UART_ID, buf);
	uart_default_tx_wait_blocking();
	printf(buf);*/

	// Water level
	int dist = measure_distance();
	sprintf(buf, "DIST:[%u]\r\n", dist);
	uart_puts(UART_ID, buf);
	uart_default_tx_wait_blocking();
	printf(buf);

	// Decide to activate pump or not
	int activate_pump_ms = 0;
	// Every hour:
	//int factor = 1;
	//if (dt->hour >= 8 && dt->hour <= 21) {
	// Morning and evening:
	int factor = 2;
	if (dt->hour == 8 || dt->hour == 20) {
		sprintf(buf, "In time for pump:[%d]\r\n", dt->hour);
		uart_puts(UART_ID, buf);
		uart_default_tx_wait_blocking();
		printf(buf);
		if (dist >= 1 && dist <= 18) {
			sprintf(buf, "Enough water for pump:[%d]\r\n", dist);
			uart_puts(UART_ID, buf);
			uart_default_tx_wait_blocking();
			printf(buf);
			if (humidity_result >= 2.5) {
				// Very dry
				activate_pump_ms = 10000 * factor;
			} else if (humidity_result >= 2.25) {
				// A bit dry
				activate_pump_ms = 7500 * factor;
			} else if (humidity_result >= 1.75) {
				// Not very dry
				activate_pump_ms = 5000 * factor;
			}
		}
	}


	// Pump activation
	if (activate_pump_ms > 0) {
		sprintf(buf, "Activate pump for [%dms]\r\n", activate_pump_ms);
		uart_puts(UART_ID, buf);
		uart_default_tx_wait_blocking();
		printf(buf);
		gpio_put(PUMP_PIN, 1);
		sleep_ms(activate_pump_ms);
		gpio_put(PUMP_PIN, 0);
	}


	// WIFI
	bool enable_wifi = true;
	if (enable_wifi) {
		uart_puts(UART_ID, "== Wifi connection ==\r\n");
		uart_default_tx_wait_blocking();
		printf("== Wifi connection ==\r\n");
		wifi_connect(WIFI_SSID, WIFI_PASSWORD);
		sleep_ms(100);

		char sendbuf[100] = "";
		sprintf(sendbuf, "[%d-%02d-%02d %02d:%02d:%02d] Bat:[%.2fV] Hum:[%.2fV] Temp:[%.2f] Water:[%d] Pump:[%d]",
			dt->year, dt->month, dt->day, dt->hour, dt->min, dt->sec, battery_result, humidity_result, temperature, dist, activate_pump_ms);
		int rt = send_udp(SERVER_IP, atoi(SERVER_PORT), sendbuf);
		sleep_ms(200);
		int rt2 = send_udp(SERVER_IP, atoi(SERVER_PORT), sendbuf);
		sprintf(buf, "rt:[%d] rt2:[%d] sent:[%s]\r\n", rt, rt2, sendbuf);
		uart_puts(UART_ID, buf);
		uart_default_tx_wait_blocking();

		sleep_ms(10);
		wifi_disconnect();
		sleep_ms(10);
	}
}


void blink(int count, int delay) {
	int i = 0;
	gpio_put(LED_PIN, 0);
	while (i < count) {
		gpio_put(LED_PIN, 1);
		sleep_ms(delay);
		gpio_put(LED_PIN, 0);
		i++;
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

	gpio_init(RTC_WORK_DONE_PIN);
	gpio_set_dir(RTC_WORK_DONE_PIN, GPIO_OUT);
	gpio_put(RTC_WORK_DONE_PIN, 0);

	gpio_init(PUMP_PIN);
	gpio_set_dir(PUMP_PIN, GPIO_OUT);
	gpio_put(PUMP_PIN, 0);

	sleep_ms(1000);

	uart_puts(UART_ID, "==START==\r\n");
	uart_default_tx_wait_blocking();
	stdio_init_all();
	printf("==START==\n");

	gpio_pull_up(RTC_SQW_PIN);

	//ssd1306_t disp;
	//setup_screen(&disp);
	//ssd1306_draw_line(&disp, 0, 0, 127, 0);
	//sprintf(buf, "start");
	//ssd1306_draw_string_with_font(&disp, 1, 1, 2, acme_font, buf);
	//ssd1306_show(&disp);

	// Setup humidity sensor
	setup_humidity();

	// Init water level
	setup_distance();

	// ADC
	setup_adc();

	// RTC
	setup_rtc_pcf();
	blink(3, 50);
	sleep_ms(100);

	gpio_put(LED_PIN, 0);

	datetime_t dt;
	char datetime_buf[256];
	char *datetime_str = &datetime_buf[0];
	uart_puts(UART_ID, "Start loop\r\n");
	uart_default_tx_wait_blocking();
	while (1) {
		blink(2, 50);
		sleep_ms(100);

		//ssd1306_poweron(&disp);

		set_dt_from_rtc_pcf(&dt);

		blink(2, 300);
		sleep_ms(3000);

		add_water(&dt);

		rtc_pcf_sleep();

		// sleepUntil();	// Old method
	}
	return 0;
}
