#include "pcf8563.h"

#include "pico/stdlib.h"


uint8_t constrain(uint8_t x, uint8_t a, uint8_t b){
  if (x < a) {
    return a;
  }
  if (x > b) {
    return b;
  }

  return x;
}


/*
 * Wire abstraction
 */

i2c_inst_t *pcf8563_i2c;

void pcf8563_set_i2c(i2c_inst_t *i2c) {
	pcf8563_i2c = i2c;
}


/* Private internal functions, but useful to look at if you need a similar func. */
uint8_t pcf8563_decToBcd(uint8_t val) {
    return ( (val/10*16) + (val%10) );
}

uint8_t pcf8563_bcdToDec(uint8_t val) {
    return ( (val/16*10) + (val%16) );
}

void pcf8563_zeroClock() {
    uint8_t txBuffer[256];
    uint32_t usedTxBuffer;
    txBuffer[usedTxBuffer++] = (uint8_t)0x0;        // start address

    txBuffer[usedTxBuffer++] = (uint8_t)0x0;     //control/status1
    txBuffer[usedTxBuffer++] = (uint8_t)0x0;     //control/status2
    txBuffer[usedTxBuffer++] = (uint8_t)0x00;    //set seconds to 0 & VL to 0
    txBuffer[usedTxBuffer++] = (uint8_t)0x00;    //set minutes to 0
    txBuffer[usedTxBuffer++] = (uint8_t)0x00;    //set hour to 0
    txBuffer[usedTxBuffer++] = (uint8_t)0x01;    //set day to 1
    txBuffer[usedTxBuffer++] = (uint8_t)0x00;    //set weekday to 0
    txBuffer[usedTxBuffer++] = (uint8_t)0x81;    //set month to 1, century to 1900
    txBuffer[usedTxBuffer++] = (uint8_t)0x00;    //set year to 0
    txBuffer[usedTxBuffer++] = (uint8_t)0x80;    //minute alarm value reset to 00
    txBuffer[usedTxBuffer++] = (uint8_t)0x80;    //hour alarm value reset to 00
    txBuffer[usedTxBuffer++] = (uint8_t)0x80;    //day alarm value reset to 00
    txBuffer[usedTxBuffer++] = (uint8_t)0x80;    //weekday alarm value reset to 00
    txBuffer[usedTxBuffer++] = (uint8_t)SQW_32KHZ; //set SQW to default, see: setSquareWave
    txBuffer[usedTxBuffer++] = (uint8_t)0x0;     //timer off

    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_zeroClock write error\n");
    }
}

void pcf8563_clearStatus() {
    uint8_t txBuffer[256];
    uint32_t usedTxBuffer;
    txBuffer[usedTxBuffer++] = (uint8_t)0x0;
    txBuffer[usedTxBuffer++] = (uint8_t)0x0;                 //control/status1
    txBuffer[usedTxBuffer++] = (uint8_t)0x0;                 //control/status2
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_clearStatus write error\n");
    }
}

/*
* Read status byte
*/
uint8_t pcf8563_readStatus2() {
    time_struct t = pcf8563_getDateTime();
    return t.status2;
}

void pcf8563_clearVoltLow(void) {
    time_struct t = pcf8563_getDateTime();
    // Only clearing is possible on device (I tried)
    pcf8563_setDateTime(t.day, t.weekday, t.month,
                t.century, t.year, t.hour,
                t.min, t.sec);
}


/*
* Atomicly read all device registers in one operation
*/
time_struct pcf8563_getDateTime(void) {
    time_struct t;

    /* Start at beginning, read entire memory in one go */
    uint8_t txBuffer[256];
    uint32_t usedTxBuffer;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_STAT1_ADDR;
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_getDateTime write error\r\n");
		return t;
    }

    /* As per data sheet, have to read everything all in one operation */
    uint8_t readBuffer[16] = {0};
	int ret = i2c_read_blocking_until(pcf8563_i2c, RTCC_ADDR, readBuffer, 16, false, make_timeout_time_ms(1000));
	if (ret == PICO_ERROR_GENERIC) {
		printf("pcf8563_getDateTime read error\r\n");
		return t;
	}
    //for (uint8_t i=0; i < 16; i++)
    //    readBuffer[i] = _i2c.read();

    // status bytes
    t.status1 = readBuffer[0];
    t.status2 = readBuffer[1];

    // time bytes
    //0x7f = 0b01111111
    t.volt_low = readBuffer[2] & RTCC_VLSEC_MASK;  //VL_Seconds
    t.sec = pcf8563_bcdToDec(readBuffer[2] & ~RTCC_VLSEC_MASK);
    t.min = pcf8563_bcdToDec(readBuffer[3] & 0x7f);
    //0x3f = 0b00111111
    t.hour = pcf8563_bcdToDec(readBuffer[4] & 0x3f);
    if(pcf8563_bcdToDec(readBuffer[4] & 0x3f) > 12) {
        t.hour12 = pcf8563_bcdToDec(readBuffer[4] & 0x3f) - 12;
        t.am = false;
    } else {
        t.hour12 = t.hour;
        t.am = true;
    }
    
    // date bytes
    //0x3f = 0b00111111
    t.day = pcf8563_bcdToDec(readBuffer[5] & 0x3f);
    //0x07 = 0b00000111
    t.weekday = pcf8563_bcdToDec(readBuffer[6] & 0x07);
    //get raw month data uint8_t and set month and century with it.
    t.month = readBuffer[7];
    if (t.month & RTCC_CENTURY_MASK)
        t.century = true;
    else
        t.century = false;
    //0x1f = 0b00011111
    t.month = t.month & 0x1f;
    t.month = pcf8563_bcdToDec(t.month);
    t.year = pcf8563_bcdToDec(readBuffer[8]);

    // alarm bytes
    t.alarm_min = readBuffer[9];
    if(0b10000000 & t.alarm_min)
        t.alarm_min = RTCC_NO_ALARM;
    else
        t.alarm_min = pcf8563_bcdToDec(t.alarm_min & 0b01111111);
    t.alarm_hour = readBuffer[10];
    if(0b10000000 & t.alarm_hour)
        t.alarm_hour = RTCC_NO_ALARM;
    else
        t.alarm_hour = pcf8563_bcdToDec(t.alarm_hour & 0b00111111);
    t.alarm_day = readBuffer[11];
    if(0b10000000 & t.alarm_day)
        t.alarm_day = RTCC_NO_ALARM;
    else
        t.alarm_day = pcf8563_bcdToDec(t.alarm_day  & 0b00111111);
    t.alarm_weekday = readBuffer[12];
    if(0b10000000 & t.alarm_weekday)
        t.alarm_weekday = RTCC_NO_ALARM;
    else
        t.alarm_weekday = pcf8563_bcdToDec(t.alarm_weekday  & 0b00000111);

    // CLKOUT_control 0x03 = 0b00000011
    t.squareWave = readBuffer[13] & 0x03;

    // timer bytes
    t.timer_control = readBuffer[14] & 0x03;
    t.timer_value = readBuffer[15];  // current value != set value when running

	return t;
}


void pcf8563_setDateTime(uint8_t day, uint8_t weekday, uint8_t month,
                              bool century, uint8_t year, uint8_t hour,
                              uint8_t minute, uint8_t sec)
{
    /* year val is 00 to 99, xx
        with the highest bit of month = century
        0=20xx
        1=19xx
        */
    month = pcf8563_decToBcd(month);
    if (century)
        month |= RTCC_CENTURY_MASK;
    else
        month &= ~RTCC_CENTURY_MASK;

    /* As per data sheet, have to set everything all in one operation */
    uint8_t txBuffer[256];
    uint32_t usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = RTCC_SEC_ADDR;       // send addr low byte, req'd
    txBuffer[usedTxBuffer++] = pcf8563_decToBcd(sec) &~RTCC_VLSEC_MASK; //set sec, clear VL bit
    txBuffer[usedTxBuffer++] = pcf8563_decToBcd(minute);    //set minutes
    txBuffer[usedTxBuffer++] = pcf8563_decToBcd(hour);        //set hour
    txBuffer[usedTxBuffer++] = pcf8563_decToBcd(day);            //set day
    txBuffer[usedTxBuffer++] = pcf8563_decToBcd(weekday);    //set weekday
    txBuffer[usedTxBuffer++] = month;                 //set month, century to 1
    txBuffer[usedTxBuffer++] = pcf8563_decToBcd(year);        //set year to 99
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_setDateTime write error\n");
    }
}

/*
* Returns true if AIE is on
*
*/
bool pcf8563_alarmEnabled() {
    time_struct t = pcf8563_getDateTime();
    return t.status2 & RTCC_ALARM_AIE;
}

/*
* Returns true if AF is on
*
*/
bool pcf8563_alarmActive() {
    time_struct t = pcf8563_getDateTime();
    return t.status2 & RTCC_ALARM_AF;
}

/* enable alarm interrupt
 * whenever the clock matches these values an int will
 * be sent out pin 3 of the Pcf8563 chip
 */
void pcf8563_enableAlarm() {
    time_struct t = pcf8563_getDateTime();  // operate on current values
    //set status2 AF val to zero
    t.status2 &= ~RTCC_ALARM_AF;
    //set TF to 1 masks it from changing, as per data-sheet
    t.status2 |= RTCC_TIMER_TF;
    //enable the interrupt
    t.status2 |= RTCC_ALARM_AIE;

    //enable the interrupt
    uint8_t txBuffer[256];
    uint32_t usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_STAT2_ADDR;
    txBuffer[usedTxBuffer++] = (uint8_t)t.status2;
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_enableAlarm write error\n");
    }
}

/* set the alarm values
 * whenever the clock matches these values an int will
 * be sent out pin 3 of the Pcf8563 chip
 */
void pcf8563_setAlarm(uint8_t min, uint8_t hour, uint8_t day, uint8_t weekday) {
    time_struct t = pcf8563_getDateTime();  // operate on current values
    if (min <99) {
        min = constrain(min, 0, 59);
        min = pcf8563_decToBcd(min);
        min &= ~RTCC_ALARM;
    } else {
        min = 0x0; min |= RTCC_ALARM;
    }

    if (hour <99) {
        hour = constrain(hour, 0, 23);
        hour = pcf8563_decToBcd(hour);
        hour &= ~RTCC_ALARM;
    } else {
        hour = 0x0; hour |= RTCC_ALARM;
    }

    if (day <99) {
        day = constrain(day, 1, 31);
        day = pcf8563_decToBcd(day); day &= ~RTCC_ALARM;
    } else {
        day = 0x0; day |= RTCC_ALARM;
    }

    if (weekday <99) {
        weekday = constrain(weekday, 0, 6);
        weekday = pcf8563_decToBcd(weekday);
        weekday &= ~RTCC_ALARM;
    } else {
        weekday = 0x0; weekday |= RTCC_ALARM;
    }

    t.alarm_hour = hour;
    t.alarm_min = min;
    t.alarm_weekday = weekday;
    t.alarm_day = day;

    // First set alarm values, then enable
    uint8_t txBuffer[256];
    uint32_t usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_ALRM_MIN_ADDR;
    txBuffer[usedTxBuffer++] = (uint8_t)t.alarm_min;
    txBuffer[usedTxBuffer++] = (uint8_t)t.alarm_hour;
    txBuffer[usedTxBuffer++] = (uint8_t)t.alarm_day;
    txBuffer[usedTxBuffer++] = (uint8_t)t.alarm_weekday;
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_setAlarm write error\n");
    }

    pcf8563_enableAlarm();
}

void pcf8563_clearAlarm() {
    time_struct t = pcf8563_getDateTime();
    //set status2 AF val to zero to reset alarm
    t.status2 &= ~RTCC_ALARM_AF;
    //set TF to 1 masks it from changing, as per data-sheet
    t.status2 |= RTCC_TIMER_TF;
    //turn off the interrupt
    t.status2 &= ~RTCC_ALARM_AIE;

    uint8_t txBuffer[256];
    uint32_t usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_STAT2_ADDR;
    txBuffer[usedTxBuffer++] = (uint8_t)t.status2;
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_clearAlarm write error\n");
    }
}

/**
* Reset the alarm leaving interrupt unchanged
*/
void pcf8563_resetAlarm() {
    time_struct t = pcf8563_getDateTime();
    //set status2 AF val to zero to reset alarm
    t.status2 &= ~RTCC_ALARM_AF;
    //set TF to 1 masks it from changing, as per data-sheet
    t.status2 |= RTCC_TIMER_TF;

    uint8_t txBuffer[256];
    uint32_t usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_STAT2_ADDR;
    txBuffer[usedTxBuffer++] = (uint8_t)t.status2;
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_resetAlarm write error\n");
    }
}

// true if timer interrupt and control is enabled
bool pcf8563_timerEnabled() {
    time_struct t = pcf8563_getDateTime();
    if (t.status2 & RTCC_TIMER_TIE)
        if (t.timer_control & RTCC_TIMER_TE)
            return true;
    return false;
}


// true if timer is active
bool pcf8563_timerActive() {
    time_struct t = pcf8563_getDateTime();
    return t.status2 & RTCC_TIMER_TF;
}


// enable timer and interrupt
void pcf8563_enableTimer(void) {
    time_struct t = pcf8563_getDateTime();
    //set TE to 1
    t.timer_control |= RTCC_TIMER_TE;
    //set status2 TF val to zero
    t.status2 &= ~RTCC_TIMER_TF;
    //set AF to 1 masks it from changing, as per data-sheet
    t.status2 |= RTCC_ALARM_AF;
    //enable the interrupt
    t.status2 |= RTCC_TIMER_TIE;
    
    t.status2 &= ~RTCC_ALARM_AIE;

    // Enable interrupt first, then enable timer
    uint8_t txBuffer[256];
    uint32_t usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_STAT2_ADDR;
    txBuffer[usedTxBuffer++] = (uint8_t)t.status2;
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_enableTimer write error\n");
    }

    usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_TIMER1_ADDR;
    txBuffer[usedTxBuffer++] = (uint8_t)t.timer_control;  // Timer starts ticking now!
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_enableTimer write error [2]\n");
    }
}


// set count-down value and frequency
void pcf8563_setTimer(uint8_t value, uint8_t frequency, bool is_pulsed) {
    time_struct t = pcf8563_getDateTime();
    if (is_pulsed)
        t.status2 |= TMR_PULSE;
    else
        t.status2 &= ~TMR_PULSE;
    t.timer_value = value;
    
    // TE set to 1 in pcf8563_enableTimer(), leave 0 for now
    t.timer_control |= (frequency & RTCC_TIMER_TD10); // use only last 2 bits

    uint8_t txBuffer[256];
    uint32_t usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_TIMER2_ADDR;
    txBuffer[usedTxBuffer++] = (uint8_t)t.timer_value;
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_setTimer write error\n");
    }

    usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_TIMER1_ADDR;
    txBuffer[usedTxBuffer++] = (uint8_t)t.timer_control;
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_setTimer write error [2]\n");
    }

    pcf8563_enableTimer();
}


// clear timer flag and interrupt
void pcf8563_clearTimer(void) {
    time_struct t = pcf8563_getDateTime();
    //set status2 TF val to zero
    t.status2 &= ~RTCC_TIMER_TF;
    //set AF to 1 masks it from changing, as per data-sheet
    t.status2 |= RTCC_ALARM_AF;
    //turn off the interrupt
    t.status2 &= ~RTCC_TIMER_TIE;
    //turn off the timer
    t.timer_control = 0;

    // Stop timer first
    uint8_t txBuffer[256];
    uint32_t usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_TIMER1_ADDR;
    txBuffer[usedTxBuffer++] = (uint8_t)t.timer_control;
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_clearTimer write error\n");
    }

    // clear flag and interrupt
    usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_STAT2_ADDR;
    txBuffer[usedTxBuffer++] = (uint8_t)t.status2;
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_clearTimer write error [2]\n");
    }
}


// clear timer flag but leave interrupt unchanged */
void pcf8563_resetTimer(void) {
    time_struct t = pcf8563_getDateTime();
    //set status2 TF val to zero to reset timer
    t.status2 &= ~RTCC_TIMER_TF;
    //set AF to 1 masks it from changing, as per data-sheet
    t.status2 |= RTCC_ALARM_AF;

    uint8_t txBuffer[256];
    uint32_t usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_STAT2_ADDR;
    txBuffer[usedTxBuffer++] = (uint8_t)t.status2;
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_resetTimer write error\n");
    }
}

/**
* Set the square wave pin output
*/
void pcf8563_setSquareWave(uint8_t frequency) {
    uint8_t txBuffer[256];
    uint32_t usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)RTCC_SQW_ADDR;
    txBuffer[usedTxBuffer++] = (uint8_t)frequency;
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_setSquareWave write error\n");
    }
}

void pcf8563_clearSquareWave() {
    pcf8563_setSquareWave(SQW_DISABLE);
}

/*
const char *pcf8563_formatTime(uint8_t style) {
	char strOut[9];
    // getTime();
    switch (style) {
        case RTCC_TIME_HMS_12:
            strOut[0] = '0' + (hour12 / 10);
            strOut[1] = '0' + (hour12 % 10);
            strOut[2] = ':';
            strOut[3] = '0' + (minute / 10);
            strOut[4] = '0' + (minute % 10);
            strOut[5] = ':';
            strOut[6] = '0' + (sec / 10);
            strOut[7] = '0' + (sec % 10);
            if(am == true){
                strOut[8] = 'a';
            }
            else{
                strOut[8] = 'p';
            }
            strOut[9] = 'm';
            strOut[10] = '\0';
            break;
        case RTCC_TIME_HM_12:
            strOut[0] = '0' + (hour12 / 10);
            strOut[1] = '0' + (hour12 % 10);
            strOut[2] = ':';
            strOut[3] = '0' + (minute / 10);
            strOut[4] = '0' + (minute % 10);
            if(am == true){
                strOut[5] = 'a';
            }
            else{
                strOut[5] = 'p';
            }
            strOut[6] = 'm';
            strOut[7] = '\0';
            break;
        case RTCC_TIME_HM:
            strOut[0] = '0' + (hour / 10);
            strOut[1] = '0' + (hour % 10);
            strOut[2] = ':';
            strOut[3] = '0' + (minute / 10);
            strOut[4] = '0' + (minute % 10);
            strOut[5] = '\0';
            break;
        case RTCC_TIME_HMS:
        default:
            strOut[0] = '0' + (hour / 10);
            strOut[1] = '0' + (hour % 10);
            strOut[2] = ':';
            strOut[3] = '0' + (minute / 10);
            strOut[4] = '0' + (minute % 10);
            strOut[5] = ':';
            strOut[6] = '0' + (sec / 10);
            strOut[7] = '0' + (sec % 10);
            strOut[8] = '\0';
            break;
    }
    return strOut;
}


const char *pcf8563_formatDate(uint8_t style) {
	char strDate[11];
    pcf8563_getDate();
	switch (style) {
	case RTCC_DATE_CZ:
		//do the czech style, dd.mm.yyyy
		strDate[0] = '0' + (day / 10);
		strDate[1] = '0' + (day % 10);
		strDate[2] = '.';
		strDate[3] = '0' + (month / 10);
		strDate[4] = '0' + (month % 10);
		strDate[5] = '.';
		if (century){
			strDate[6] = '1';
			strDate[7] = '9';
		} else {
			strDate[6] = '2';
			strDate[7] = '0';
		}
		strDate[8] = '0' + (year / 10 );
		strDate[9] = '0' + (year % 10);
		strDate[10] = '\0';
		break;
	case RTCC_DATE_ASIA:
		//do the asian style, yyyy-mm-dd
		if (century ){
			strDate[0] = '1';
			strDate[1] = '9';
		}
		else {
			strDate[0] = '2';
			strDate[1] = '0';
		}
		strDate[2] = '0' + (year / 10 );
		strDate[3] = '0' + (year % 10);
		strDate[4] = '-';
		strDate[5] = '0' + (month / 10);
		strDate[6] = '0' + (month % 10);
		strDate[7] = '-';
		strDate[8] = '0' + (day / 10);
		strDate[9] = '0' + (day % 10);
		strDate[10] = '\0';
		break;
	case RTCC_DATE_US:
	//the pitiful US style, mm/dd/yyyy
		strDate[0] = '0' + (month / 10);
		strDate[1] = '0' + (month % 10);
		strDate[2] = '/';
		strDate[3] = '0' + (day / 10);
		strDate[4] = '0' + (day % 10);
		strDate[5] = '/';
		if (century){
			strDate[6] = '1';
			strDate[7] = '9';
		}
		else {
			strDate[6] = '2';
			strDate[7] = '0';
		}
		strDate[8] = '0' + (year / 10 );
		strDate[9] = '0' + (year % 10);
		strDate[10] = '\0';
		break;
	case RTCC_DATE_WORLD:
	default:
		//do the world style, dd-mm-yyyy
		strDate[0] = '0' + (day / 10);
		strDate[1] = '0' + (day % 10);
		strDate[2] = '-';
		strDate[3] = '0' + (month / 10);
		strDate[4] = '0' + (month % 10);
		strDate[5] = '-';
		if (century){
			strDate[6] = '1';
			strDate[7] = '9';
		} else {
			strDate[6] = '2';
			strDate[7] = '0';
		}
		strDate[8] = '0' + (year / 10 );
		strDate[9] = '0' + (year % 10);
		strDate[10] = '\0';
		break;

    }
    return strDate;
}
*/

void pcf8563_initClock()
{
    uint8_t txBuffer[256];
    uint32_t usedTxBuffer = 0;
    txBuffer[usedTxBuffer++] = (uint8_t)0x0;        // start address

    txBuffer[usedTxBuffer++] = (uint8_t)0x0;     //control/status1
    txBuffer[usedTxBuffer++] = (uint8_t)0x0;     //control/status2
    txBuffer[usedTxBuffer++] = (uint8_t)0x81;     //set seconds & VL
    txBuffer[usedTxBuffer++] = (uint8_t)0x01;    //set minutes
    txBuffer[usedTxBuffer++] = (uint8_t)0x01;    //set hour
    txBuffer[usedTxBuffer++] = (uint8_t)0x01;    //set day
    txBuffer[usedTxBuffer++] = (uint8_t)0x01;    //set weekday
    txBuffer[usedTxBuffer++] = (uint8_t)0x01;     //set month, century to 1
    txBuffer[usedTxBuffer++] = (uint8_t)0x01;    //set year to 99
    txBuffer[usedTxBuffer++] = (uint8_t)0x80;    //minute alarm value reset to 00
    txBuffer[usedTxBuffer++] = (uint8_t)0x80;    //hour alarm value reset to 00
    txBuffer[usedTxBuffer++] = (uint8_t)0x80;    //day alarm value reset to 00
    txBuffer[usedTxBuffer++] = (uint8_t)0x80;    //weekday alarm value reset to 00
    txBuffer[usedTxBuffer++] = (uint8_t)0x0;     //set SQW, see: setSquareWave
    txBuffer[usedTxBuffer++] = (uint8_t)0x0;     //timer off
    if (i2c_write_blocking_until(pcf8563_i2c, RTCC_ADDR, txBuffer, usedTxBuffer, false, make_timeout_time_ms(1000)) == PICO_ERROR_GENERIC){
		printf("pcf8563_initClock write error\n");
    }
}
