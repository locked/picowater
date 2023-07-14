#ifndef _RTC_DS3231_TCP_H
#define _RTC_DS3231_TCP_H

#ifdef __cplusplus
extern "C"{
#endif 

bool dev_ds3231_setdatetime(i2c_inst_t* i2c, datetime_t *dt);

int dev_ds3231_getdatetime(i2c_inst_t* i2c, datetime_t *dt);

void dev_ds3231_setalarm(i2c_inst_t* i2c, const uint8_t mi, const uint8_t h, const uint8_t d);

float rtc_ds3231_get_temp(i2c_inst_t* i2c);

#ifdef __cplusplus
}
#endif

#endif // _RTC_DS3231_TCP_H

