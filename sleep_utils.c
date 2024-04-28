

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


void setup_rtc_ds() {
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

	if (dt.year <= 2000) {
		// To set time
		dt.year = 2023;
		dt.month = 8;
		dt.day = 17;
		dt.dotw = 4;
		dt.hour = 19;
		dt.min = 38;
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



void sleepUntil() {
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

	//ssd1306_poweroff(&disp);

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
