#include <string.h>
#include <time.h>

#include "pico/unique_id.h"
#include "hardware/watchdog.h"

#include "network.h"
#include "wifi.h"
#include "tinyhttp/http.h"
#include "tiny-json.h"
#include "pcf8563/pcf8563.h"


extern int pump_force_ms;
extern bool wakeup_everymin;
extern int dist;
extern float batlvl;
extern float batlvl2;
extern float systemp;


int get_response_from_server(char *response) {
	char board_id[20];
	pico_get_unique_board_id_string(board_id, 20);

	time_struct dt = pcf8563_getDateTime();

	char query[200];
	//sprintf(query, "GET /water.php?id=%s&dist=%d&batlvl=%f&date=%d-%d-%dT%d:%d HTTP/1.0\r\nContent-Length: 0\r\n\r\n", board_id, dist, batlvl, dt.year, dt.month, dt.day, dt.hour, dt.min);
	sprintf(query, "GET /water.php?id=%s&dist=%d&batlvl=%.2f&batlvl2=%.2f&systemp=%.1f&date=%d-%d-%dT%d:%d HTTP/1.0\r\nContent-Length: 0\r\n\r\n", board_id, dist, batlvl, batlvl2, systemp, dt.year, dt.month, dt.day, dt.hour, dt.min);
	int ret = send_tcp(SERVER_IP, 80, query, strlen(query), response);
	//printf("RESPONSE FROM SERVER ret:[%d] Server:[%s] => [%s]\r\n", ret, SERVER_IP, response);
	if (ret == 0 && strlen(response) < 10) {
		ret = 1;
	}

    return ret;
}

// HTTP lib
// Response data/funcs
struct HttpResponse {
	char body[4096];
    int code;
};
static void* response_realloc(void* opaque, void* ptr, int size) {
    return realloc(ptr, size);
}
static void response_body(void* opaque, const char* data, int size) {
    struct HttpResponse* response = (struct HttpResponse*)opaque;
    //printf("[NET] ADD [%d] BYTES TO BODY:[%s]\r\n", size, data);
    strncpy(response->body, data, size+1);
}
static void response_header(void* opaque, const char* ckey, int nkey, const char* cvalue, int nvalue) {
}
static void response_code(void* opaque, int code) {
    struct HttpResponse* response = (struct HttpResponse*)opaque;
    //printf("[NET] SET CODE [%d]\r\n", code);
    response->code = code;
}
static const struct http_funcs responseFuncs = {
    response_realloc,
    response_body,
    response_header,
    response_code,
};

int parse_http_response(char *http_response, struct HttpResponse *response) {
    //printf("[NET] PARSE HTTP:[%s]\r\n", http_response);

    //struct HttpResponse* response = (struct HttpResponse*)opaque;
    response->code = 0;

    struct http_roundtripper rt;
    http_init(&rt, responseFuncs, response);
    //printf("[NET] PARSE HTTP: INIT OK\r\n");
    int read;
    int ndata = strlen(http_response);
    http_data(&rt, http_response, ndata, &read);
    //printf("[NET] PARSE HTTP: http_data OK\r\n");

    http_free(&rt);

    if (http_iserror(&rt)) {
        printf("Error parsing data\r\n");
        return -1;
    }

    if (strlen(response->body) > 0) {
        //printf("CODE:[%d] BODY:[%s]\r\n", response->code, response->body);
        return 0;
    }

    return -1;
}

int parse_json_response(char *response) {
    //printf("[NET] PARSE JSON:[%s]\r\n", response);
	enum { MAX_FIELDS = 100 };
	json_t pool[MAX_FIELDS];

	json_t const* root_elem = json_create(response, pool, MAX_FIELDS);
    if (root_elem == NULL) {
        printf("[NET] Error parsing json: root_elem not found\r\n");
        return -1;
    }
    json_t const* status_elem = json_getProperty(root_elem, "status");
    if (status_elem == NULL) {
        printf("[NET] Error parsing json: status_elem not found\r\n");
        return -1;
    }
	int status = json_getInteger(status_elem);

	// Date
	//printf("Parsing date from [%s]...\r\n", response);
	json_t const* date_elem = json_getProperty(root_elem, "date");
    if (date_elem == NULL) {
        printf("[NET] Error parsing json: date_elem not found\r\n");
        return -1;
    }
	json_t const* year_elem = json_getProperty(date_elem, "year");
    if (year_elem == NULL) {
        printf("[NET] Error parsing json: year_elem not found in date\r\n");
        return -1;
    }
	int year_full = json_getInteger(year_elem);
	int year;
	bool century;
	if (year_full >= 2000) {
        year = year_full - 2000;
        century = false;
	} else {
        year = year_full - 1900;
        century = true;
	}
	json_t const* day_elem = json_getProperty(date_elem, "day");
	datetime_t dt;
	dt.year = year_full;
	dt.month = json_getInteger(json_getProperty(date_elem, "month"));
	dt.day = json_getInteger(day_elem);
	dt.dotw = json_getInteger(json_getProperty(date_elem, "weekday"));
	dt.hour = json_getInteger(json_getProperty(date_elem, "hour"));
	dt.min = json_getInteger(json_getProperty(date_elem, "minute"));
	dt.sec = json_getInteger(json_getProperty(date_elem, "second"));
    //printf("SET in INTERNAL + EXTERNAL RTC year:[%d]\r\n", dt.year);
	pcf8563_setDateTime(dt.day, dt.dotw, dt.month, century, year, dt.hour, dt.min, dt.sec);

	// Pump
	json_t const* pump_elem = json_getProperty(root_elem, "pump");
	pump_force_ms = json_getInteger(pump_elem);

	// Wakeup every minute
	json_t const* wakeup_everymin_elem = json_getProperty(root_elem, "wakeup_everymin");
	int wakeup_everymin_value = json_getInteger(wakeup_everymin_elem);
	if (wakeup_everymin_value == 1) {
		wakeup_everymin = true;
	} else {
		wakeup_everymin = false;
	}

	printf("STATUS FROM SERVER:[%d] year:[%d] day:[%d] weekday:[%d] month:[%d] hour:[%d]\r\n", status, dt.year, dt.day, dt.dotw, dt.month, dt.hour);
}


int network_update() {
    printf("Connecting to wifi [%s][%s]...\r\n", WIFI_SSID, WIFI_PASSWORD);
	int ret = wifi_connect(WIFI_SSID, WIFI_PASSWORD);
    if (ret != 0) {
		watchdog_update();
		printf("Failed, wait a bit and retry connecting to wifi [%s][%s]...\r\n", WIFI_SSID, WIFI_PASSWORD);
		wifi_disconnect();
		sleep_ms(1500);
		watchdog_update();
		ret = wifi_connect(WIFI_SSID, WIFI_PASSWORD);
	}
	watchdog_update();
    if (ret == 0) {
		char http_buffer[4096] = "";
		if (get_response_from_server(http_buffer) == 0) {
			struct HttpResponse response;
			if (parse_http_response(http_buffer, &response) == 0) {
				parse_json_response(response.body);
			}
		}
	}
	printf("Disconnect...\r\n");
    wifi_disconnect();
    printf("Disconnected from wifi, ret:[%d]\r\n", ret);
    return ret;
}
