#include "cJSON.h"
#include "version.h"
#include "wttrrequest.h"
#include <curl/curl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CURLWEATHER_APP_NAME "curlweather"

void print_version(const char *appname) {
  printf("%s %d.%d.%d\n", appname, PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR,
         PROJECT_VERSION_PATCH);
  char *curlversionstring = curl_version();
  printf("Curl: %s\n", curlversionstring);
  printf("cJSON: %d.%d.%d\n", CJSON_VERSION_MAJOR, CJSON_VERSION_MINOR,
         CJSON_VERSION_PATCH);
}

void display_forecast(wttr_today_forecast_struct fetched_forecast) {
  wttr_area_struct area = fetched_forecast.area;
  if (area.area != NULL || area.country != NULL || area.region != NULL) {
    printf("Weather at ");
    char *parts[] = {area.area, area.region, area.country};
    bool unescaped = false;
    for (size_t i = 0; i < 3; i++) {
      if (parts[i] != NULL) {
        if (unescaped) {
          printf(", ");
        }
        printf("%s", parts[i]);
        unescaped = true;
      }
    }
  } else {
    printf("Weather");
  }
  if (fetched_forecast.date != NULL) {
    printf(" on %s", fetched_forecast.date);
  }
  printf(": ");
  char *parts[] = {fetched_forecast.weather.temperature_celsius,
                   fetched_forecast.weather.description_text,
                   fetched_forecast.weather.wind_description};
  bool unescaped = false;
  for (size_t i = 0; i < 3; i++) {
    if (parts[i] != NULL) {
      if (unescaped) {
        printf(", ");
      }
      if (i == 2) {
        printf("wind: ");
      };
      printf("%s", parts[i]);
      unescaped = true;
    }
  }
  printf("\n");
}

int fetch_weather(char *location_code) {
  int ret_val = wttrrequest_get_by_location(location_code, display_forecast);
  if (ret_val != 0) {
    switch (ret_val) {
    case CODE_WTTR_IN_BAD_LOCATION:
      printf("Bad location: \"%s\"\n", location_code);
      break;
    case CODE_WTTR_IN_REQUEST_FAILED:
      printf("Error during network request\n");
      break;
    case CODE_WTTR_IN_RESPONSE_READ_ERROR:
      printf("Cannot parse weather information\n");
      break;
    }
    return ret_val;
  }
  return 0;
}

void print_usage(const char *appname) {
  printf("Usage: %s <location>\n", appname);
}

int main(int argc, char **argv) {
  int ret_val = 0;
  CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
  if (res != CURLE_OK) {
    printf("Cannot initialize curl, errcode %d\n", res);
  }
  if (argc < 2 || (argc == 2 && ((strcmp("--help", argv[1]) == 0) ||
                                 strcmp("-h", argv[1]) == 0))) {
    print_usage(argc == 1 ? argv[0] : CURLWEATHER_APP_NAME);
    ret_val = argc < 2 ? -1 : 0;
  } else if ((strcmp("--version", argv[1]) == 0) ||
             strcmp("-v", argv[1]) == 0) {
    print_version(argv[0]);
  } else if (argc > 2) {
    printf("Too many arguments\n");
    print_usage(argv[0]);
    ret_val = -1;
  } else {
    ret_val = fetch_weather(argv[1]);
  }
  curl_global_cleanup();
  return ret_val;
}
