#include <string.h>
#include <time.h>

#include "pico/stdlib.h"

#include "hardware/i2c.h"

#define I2C_DS3231_ADDR 0x68
#define I2C_TIMEOUT_CHAR 500

#define DS3231_ALARM2_ADDR      0x0B
#define DS3231_CONTROL_ADDR     0x0E
#define DS3231_STATUS_ADDR      0x0F
#define DS3231_TEMP_MSB_ADDR    0x11
#define DS3231_TEMP_LSB_ADDR    0x12

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


#define UART_ID uart0


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

void dev_ds3231_set_sreg(i2c_inst_t* i2c, const uint8_t val)
{
    uint8_t buffer[1] = { val };
    i2c_write_bytes(i2c, DS3231_STATUS_ADDR, buffer, 1);
}

uint8_t dev_ds3231_get_sreg(i2c_inst_t* i2c)
{
    uint8_t buffer[1];
    i2c_read_timeout_us(i2c, DS3231_STATUS_ADDR, buffer, 1, false, I2C_TIMEOUT_CHAR);
    return buffer[0];
}

void dev_ds3231_setalarm(i2c_inst_t* i2c, const uint8_t mi, const uint8_t h, const uint8_t d)
{
	// Clear A2F
    uint8_t reg_val;
    reg_val = dev_ds3231_get_sreg(i2c) & ~DS3231_STATUS_A2F;
    dev_ds3231_set_sreg(i2c, reg_val);


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

	// set control register to activate Alarm2 and enable Battery Backed SQW
    buffer[0] = DS3231_CONTROL_INTCN | DS3231_CONTROL_A2IE; // | DS3231_CONTROL_BBSQW;
    i2c_write_bytes(i2c, DS3231_CONTROL_ADDR, buffer, 1);
}


float rtc_ds3231_get_temp(i2c_inst_t* i2c) {
    uint8_t temp[2];
    uint8_t addr = DS3231_TEMP_MSB_ADDR;
    i2c_write_blocking(i2c, I2C_DS3231_ADDR, &addr, 1, true);
    i2c_read_blocking(i2c, I2C_DS3231_ADDR, temp, 2, false);

	uint8_t lsb = temp[1] >> 6;

	float rv = 0.25 * lsb + temp[0];

    return rv;
}
