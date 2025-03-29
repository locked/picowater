#ifndef PCF8563_H
#define PCF8563_H

#include "hardware/i2c.h"
#include <stdint.h>
#include <stdio.h>


/* the read and write values for pcf8563 rtcc */
/* these are adjusted for arduino */
#define RTCC_R      0xa3
#define RTCC_W      0xa2
#define RTCC_ADDR   RTCC_R>>1

#define RTCC_SEC        1
#define RTCC_MIN        2
#define RTCC_HR         3
#define RTCC_DAY        4
#define RTCC_WEEKDAY    5
#define RTCC_MONTH      6
#define RTCC_YEAR       7
#define RTCC_CENTURY    8

/* register addresses in the rtc */
#define RTCC_STAT1_ADDR     0x0
#define RTCC_STAT2_ADDR     0x01
#define RTCC_SEC_ADDR       0x02
#define RTCC_MIN_ADDR       0x03
#define RTCC_HR_ADDR        0x04
#define RTCC_DAY_ADDR       0x05
#define RTCC_WEEKDAY_ADDR   0x06
#define RTCC_MONTH_ADDR     0x07
#define RTCC_YEAR_ADDR      0x08
#define RTCC_ALRM_MIN_ADDR  0x09
#define RTCC_SQW_ADDR       0x0D
#define RTCC_TIMER1_ADDR    0x0E
#define RTCC_TIMER2_ADDR    0x0F

/* setting the alarm flag to 0 enables the alarm.
 * set it to 1 to disable the alarm for that value.
 */
#define RTCC_ALARM          0x80
#define RTCC_ALARM_AIE      0x02
#define RTCC_ALARM_AF       0x08
/* optional val for no alarm setting */
#define RTCC_NO_ALARM       99

#define RTCC_TIMER_TIE      0x01  // Timer Interrupt Enable

#define RTCC_TIMER_TF       0x04  // Timer Flag, read/write active state
                                  // When clearing, be sure to set RTCC_TIMER_AF
                                  // to 1 (see note above).
#define RTCC_TIMER_TI_TP    0x10  // 0: INT is active when TF is active
                                  //    (subject to the status of TIE)
                                  // 1: INT pulses active
                                  //    (subject to the status of TIE);
                                  // Note: TF stays active until cleared
                                  // no matter what RTCC_TIMER_TI_TP is.
#define RTCC_TIMER_TD10     0x03  // Timer source clock, TMR_1MIN saves power
#define RTCC_TIMER_TE       0x80  // Timer 1:enable/0:disable

/* Timer source-clock frequency constants */
#define TMR_4096HZ      0b00000000
#define TMR_64Hz        0b00000001
#define TMR_1Hz         0b00000010
#define TMR_1MIN        0b00000011

#define TMR_PULSE       0b00010000

#define RTCC_CENTURY_MASK   0x80
#define RTCC_VLSEC_MASK     0x80

/* date format flags */
#define RTCC_DATE_WORLD     0x01
#define RTCC_DATE_ASIA      0x02
#define RTCC_DATE_US        0x04
#define RTCC_DATE_CZ        0x05
/* time format flags */
#define RTCC_TIME_HMS       0x01
#define RTCC_TIME_HM        0x02
#define RTCC_TIME_HMS_12    0x03
#define RTCC_TIME_HM_12     0x04

/* square wave contants */
#define SQW_DISABLE     0b00000000
#define SQW_32KHZ       0b10000000
#define SQW_1024HZ      0b10000001
#define SQW_32HZ        0b10000010
#define SQW_1HZ         0b10000011



/* time variables */
typedef struct {
	uint8_t hour;
	uint8_t hour12;
	bool am;
	uint8_t min;
	bool volt_low;		// indicates if the values are valid
	uint8_t sec;
	uint8_t day;
	uint8_t weekday;
	uint8_t month;
	uint8_t year;

	/* alarm */
	uint8_t alarm_hour;
	uint8_t alarm_min;
	uint8_t alarm_weekday;
	uint8_t alarm_day;

	/* CLKOUT */
	uint8_t squareWave;

	/* timer */
	uint8_t timer_control;
	uint8_t timer_value;

	/* support */
	uint8_t status1;
	uint8_t status2;
	bool century;
} time_struct;


#ifdef __cplusplus
extern "C"{
#endif 

time_struct pcf8563_getDateTime();		// get date and time vals to local vars

void pcf8563_set_i2c(i2c_inst_t *i2c);

void pcf8563_zeroClock();				// Zero date/time, alarm / timer, default clkout
void pcf8563_clearStatus();				// set both status bytes to zero
uint8_t pcf8563_readStatus2();
void pcf8563_clearVoltLow(void);		// Only clearing is possible

void pcf8563_setDateTime(uint8_t day, uint8_t weekday, uint8_t month, bool century, uint8_t year,
				 uint8_t hour, uint8_t minute, uint8_t sec);
bool pcf8563_alarmEnabled();			// true if alarm interrupt is enabled
bool pcf8563_alarmActive();				// true if alarm is active (going off)
void pcf8563_enableAlarm();				// activate alarm flag and interrupt
void pcf8563_setAlarm(uint8_t min, uint8_t hour, uint8_t day, uint8_t weekday);	// set alarm vals, 99=ignore
void pcf8563_clearAlarm();				// clear alarm flag and interrupt
void pcf8563_resetAlarm();				// clear alarm flag but leave interrupt unchanged

bool pcf8563_timerEnabled();			// true if timer and interrupt is enabled
bool pcf8563_timerActive();				// true if timer is active (going off)
void pcf8563_enableTimer(void);			// activate timer flag and interrupt
void pcf8563_setTimer(uint8_t value, uint8_t frequency, bool is_pulsed);  // set value & frequency
void pcf8563_clearTimer(void);			// clear timer flag, and interrupt, leave value unchanged
void pcf8563_resetTimer(void);			// same as clearTimer() but leave interrupt unchanged */

void pcf8563_setSquareWave(uint8_t frequency);
void pcf8563_clearSquareWave();

// Sets date/time to static fixed values, disable all alarms
// use zeroClock() above to guarantee lowest possible values instead.
void pcf8563_initClock();

#ifdef __cplusplus
}
#endif

#endif
