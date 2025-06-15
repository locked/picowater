#include "mcp9808.h"

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#define MCP9808_TA_REGISTER_ADDR 0x05


void floatToString(char *str, float f, char size) {
    char pos;  // position in string
    char len;  // length of decimal part of result
    char* curr;  // temp holder for next digit
    int value;  // decimal digit(s) to convert
    pos = 0;  // initialize pos, just to be sure
    value = (int)f;  // truncate the floating point number
    itoa(value, str, 10);  // this is kinda dangerous depending on the length of str
    // now str array has the digits before the decimal

    if (f < 0 ) {  // handle negative numbers
        f *= -1;
        value *= -1;
    }

    len = strlen(str);  // find out how big the integer part was
    pos = len;  // position the pointer to the end of the integer part
    str[pos++] = '.';  // add decimal point to string

    while (pos < (size + len + 1)) {  // process remaining digits
        f = f - (float)value;  // hack off the whole part of the number
        f *= 10;  // move next digit over
        value = (int)f;  // get next digit
        itoa(value, curr, 10); // convert digit to string
        str[pos++] = *curr; // add digit to result string and increment pointer
    }
}


float mcp9808_get_temperature() {
    //printf("mcp9808: starting on-board temperature measurement\n");
	sleep_ms(1000);
    int result;
    uint8_t addr = MCP9808_TA_REGISTER_ADDR;

    result = i2c_write_blocking(i2c1, MCP9808_I2C_ADDR, &addr, 1, false);
    if (result != 1) {
        printf("mcp9808: failed to write Ta register address\n");
        return result;
    }

    uint8_t measure_data[2];
    result = i2c_read_blocking(i2c1, MCP9808_I2C_ADDR, measure_data, sizeof(measure_data), false);
    if (result != sizeof(measure_data)) {
        printf("mcp9808: failed to read measure data\n");
        return result;
    }

    measure_data[0] &= 0x1F;

	float temp;
    if ((measure_data[0] & 0x10) == 0x10) {
        // negative temperature, clear sign and convert
        temp = 256.0 - (measure_data[0] * 16.0) + (measure_data[1] / 16.0);
    } else {
        // positive temperature, just convert
        temp = (measure_data[0] * 16.0) + (measure_data[1] / 16.0);
    }

	//char temp_str[10];
    //floatToString(temp_str, temp, 2);
	//printf("mcp9808: %s\n", temp_str);
	printf("mcp9808 temp: %.1f\n", temp);

	return temp;
}
