#ifndef WTTRREQUEST_H
#define WTTRREQUEST_H

#include <stdint.h>

#define CODE_WTTR_IN_BAD_LOCATION -1
#define CODE_WTTR_IN_REQUEST_FAILED -2
#define CODE_WTTR_IN_RESPONSE_READ_ERROR -3
#define CODE_WTTR_IN_CURL_ERROR -4

typedef struct {
  char *description_text;
  char *wind_description;
  char *temperature_celsius;
} wttr_info_struct;

typedef struct {
  char *country;
  char *region;
  char *area;
} wttr_area_struct;

typedef struct {
  char *date;
  wttr_area_struct area;
  wttr_info_struct weather;
} wttr_today_forecast_struct;

typedef void (*forecast_process_fn)(
    wttr_today_forecast_struct fetched_forecast);

int wttrrequest_get_by_location(char *location_code,
                                forecast_process_fn forecast_processor);

#endif
