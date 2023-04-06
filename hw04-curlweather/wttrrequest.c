#include "wttrrequest.h"
#include "3rdparty/cJSON/cJSON.h"
#include "cJSON.h"
#include <ctype.h>
#include <curl/curl.h>
#include <limits.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define URL_TEMPALTE "http://wttr.in/%s?M&format=j2&lang=ru"
#define HTTP_PARAM_ANCHOR_SYMBOL '#'
char errbuf[CURL_ERROR_SIZE];

typedef struct {
  char *buf;
  size_t size;
} readed_data_struct;

void print_curl_error(CURLcode code) {
  size_t len = strlen(errbuf);
  fprintf(stderr, "\nlibcurl: (%d) ", code);
  if (len)
    fprintf(stderr, "%s%s", errbuf, ((errbuf[len - 1] != '\n') ? "\n" : ""));
  else
    fprintf(stderr, "%s\n", curl_easy_strerror(code));
}

char *trim(char *s) {
  int l = strlen(s);
  while (isspace(s[l - 1]))
    --l;
  while (*s && isspace(*s))
    ++s, --l;
  return strndup(s, l);
}

char *generate_req_to_location(CURL *curl, char *location_code) {
  char *escaped_location = NULL;
  char *result = NULL;
  char *processed_location = trim(location_code);
  if (processed_location == NULL) {
    return NULL;
  }
  if (strlen(processed_location) == 0) {
    goto release_resources;
  }
  if (strchr(location_code, HTTP_PARAM_ANCHOR_SYMBOL) != NULL) {
    goto release_resources;
  }
  escaped_location = curl_easy_escape(curl, processed_location, 0);
  if (escaped_location == NULL) {
    goto release_resources;
  }
  char *buf = malloc(strlen(URL_TEMPALTE) + strlen(escaped_location) -
                     sizeof(char) * 2 + 1);
  if (buf == NULL) {
    goto release_resources;
  }
  int cres = sprintf(buf, URL_TEMPALTE, escaped_location);
  if (cres < 0) {
    free(buf);
    goto release_resources;
  }
  result = buf;
release_resources:
  curl_free(escaped_location);
  free(processed_location);
  return result;
}

static size_t process_curl_data(void *contents, size_t size, size_t nmemb,
                                void *userp) {
  size_t realsize = size * nmemb;
  readed_data_struct *readed = (readed_data_struct *)userp;

  readed->buf = realloc(readed->buf, readed->size + realsize + 1);
  if (readed->buf == NULL) {
    fprintf(stderr, "Cannot allocate memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(readed->buf[readed->size]), contents, realsize);
  readed->size += realsize;
  readed->buf[readed->size] = 0;

  return realsize;
}

unsigned int kmphtoms(uint8_t kmph) { return (uint8_t)((double)kmph * 5 / 18); }

int parse_response_json(char *json_string,
                        forecast_process_fn forecast_processor) {
#ifdef DEBUG_OUTPUT
  printf("%s", json_string);
#endif
  int ret_val = 0;
  cJSON *json_doc = NULL;
  json_doc = cJSON_Parse(json_string);
  if (json_doc == NULL) {
    fprintf(stderr, "Error parsing JSON");
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "Error before: %s\n", error_ptr);
    }
    ret_val = CODE_WTTR_IN_RESPONSE_READ_ERROR;
    goto release_resources;
  }
  cJSON *j_nearest_area_arr =
      cJSON_GetObjectItemCaseSensitive(json_doc, "nearest_area");
  wttr_area_struct wttr_area = {NULL, NULL, NULL};
  if (cJSON_GetArraySize(j_nearest_area_arr) != 0) {
    cJSON *j_nearest_area = cJSON_GetArrayItem(j_nearest_area_arr, 0);
    cJSON *j_country_arr =
        cJSON_GetObjectItemCaseSensitive(j_nearest_area, "country");
    cJSON *j_country = cJSON_GetArrayItem(j_country_arr, 0);
    cJSON *j_country_name =
        cJSON_GetObjectItemCaseSensitive(j_country, "value");
    if (cJSON_IsString(j_country_name) &&
        (j_country_name->valuestring != NULL)) {
      wttr_area.country = j_country_name->valuestring;
    }
    cJSON *j_region_arr =
        cJSON_GetObjectItemCaseSensitive(j_nearest_area, "region");
    cJSON *j_region = cJSON_GetArrayItem(j_region_arr, 0);
    cJSON *j_region_name = cJSON_GetObjectItemCaseSensitive(j_region, "value");
    if (cJSON_IsString(j_region_name) && (j_region_name->valuestring != NULL)) {
      wttr_area.region = j_region_name->valuestring;
    }
    cJSON *j_area_arr =
        cJSON_GetObjectItemCaseSensitive(j_nearest_area, "areaName");
    cJSON *j_area = cJSON_GetArrayItem(j_area_arr, 0);
    cJSON *j_area_name = cJSON_GetObjectItemCaseSensitive(j_area, "value");
    if (cJSON_IsString(j_area_name) && (j_area_name->valuestring != NULL)) {
      wttr_area.area = j_area_name->valuestring;
    }
  }
  wttr_info_struct wttr_today_forecast = {NULL, NULL, NULL};

  cJSON *j_current_condition_arr =
      cJSON_GetObjectItemCaseSensitive(json_doc, "current_condition");
  cJSON *j_current_condition = cJSON_GetArrayItem(j_current_condition_arr, 0);
  cJSON *j_current_condition_ru_arr =
      cJSON_GetObjectItemCaseSensitive(j_current_condition, "lang_ru");
  cJSON *j_current_condition_ru =
      cJSON_GetArrayItem(j_current_condition_ru_arr, 0);
  cJSON *j_current_condition_ru_value =
      cJSON_GetObjectItemCaseSensitive(j_current_condition_ru, "value");

  if (cJSON_IsString(j_current_condition_ru_value) &&
      (j_current_condition_ru_value->valuestring != NULL)) {
    wttr_today_forecast.description_text =
        j_current_condition_ru_value->valuestring;
  }
  char *date = NULL;
  cJSON *j_weather_arr = cJSON_GetObjectItemCaseSensitive(json_doc, "weather");
  cJSON *j_weather = cJSON_GetArrayItem(j_weather_arr, 0);
  char *temp_parts[] = {NULL, NULL, NULL};
  cJSON *j_temp_avg = cJSON_GetObjectItemCaseSensitive(j_weather, "avgtempC");
  if (cJSON_IsString(j_temp_avg) && (j_temp_avg->valuestring != NULL)) {
    temp_parts[0] = j_temp_avg->valuestring;
  }
  cJSON *j_temp_min = cJSON_GetObjectItemCaseSensitive(j_weather, "mintempC");
  if (cJSON_IsString(j_temp_min) && (j_temp_min->valuestring != NULL)) {
    temp_parts[1] = j_temp_min->valuestring;
  }
  cJSON *j_temp_max = cJSON_GetObjectItemCaseSensitive(j_weather, "maxtempC");
  if (cJSON_IsString(j_temp_max) && (j_temp_max->valuestring != NULL)) {
    temp_parts[2] = j_temp_max->valuestring;
  }
  char temp_desc_buffer[100];
  if (temp_parts[0] == NULL && temp_parts[1] == NULL && temp_parts[2] == NULL) {
    wttr_today_forecast.temperature_celsius = NULL;
  } else {
    int cx = 0;
    if (temp_parts[0] != NULL) {
      cx += snprintf(temp_desc_buffer, 100 - cx, "%sC", temp_parts[0]);
    }
    if (temp_parts[1] != NULL && temp_parts[2] != NULL && cx < 100) {
      cx += snprintf(temp_desc_buffer + cx, 100 - cx, "(from %sC to %sC)",
                     temp_parts[1], temp_parts[2]);
    }
    wttr_today_forecast.temperature_celsius = temp_desc_buffer;
  }
  int windspeed_kmph = -1;
  char *wind_direction = NULL;
  cJSON *j_windspeed_kmph =
      cJSON_GetObjectItemCaseSensitive(j_current_condition, "windspeedKmph");
  if (cJSON_IsString(j_windspeed_kmph) &&
      j_windspeed_kmph->valuestring != NULL) {
    char *speed_string = j_windspeed_kmph->valuestring;
    char *end = NULL;
    unsigned long l_windspeed_kmph = strtoul(speed_string, &end, 10);
    if (!(end == speed_string || l_windspeed_kmph == ULONG_MAX ||
          l_windspeed_kmph > INT_MAX)) {
      windspeed_kmph = (int)l_windspeed_kmph;
    }
  }
  cJSON *j_wind_direction =
      cJSON_GetObjectItemCaseSensitive(j_current_condition, "winddir16Point");
  if (cJSON_IsString(j_wind_direction)) {
    wind_direction = j_wind_direction->valuestring;
  }
  char wind_desc_buffer[100];
  if (wind_direction != NULL || windspeed_kmph != -1) {
    int cx = 0;
    if (windspeed_kmph != -1) {
      cx += snprintf(wind_desc_buffer, 100 - cx, "%d m/s",
                     kmphtoms(windspeed_kmph));
    }
    if (wind_direction != NULL && windspeed_kmph != -1) {
      cx += snprintf(wind_desc_buffer + cx, 100 - cx, " ");
    }
    if (wind_direction != NULL) {
      sprintf(wind_desc_buffer + cx, "%s", wind_direction);
    }
    wttr_today_forecast.wind_description = wind_desc_buffer;
  }
  wttr_today_forecast_struct result = {date, wttr_area, wttr_today_forecast};
  forecast_processor(result);
release_resources:
  if (json_doc != NULL) {
    cJSON_Delete(json_doc);
  }
  return ret_val;
}

int wttrrequest_get_by_location(char *location_code,
                                forecast_process_fn forecast_processor) {
  CURL *curl = NULL;
  char *url = NULL;
  int ret_val = 0;
  curl = curl_easy_init();
  if (!curl) {
    ret_val = CODE_WTTR_IN_CURL_ERROR;
    goto release_resources;
  }
  url = generate_req_to_location(curl, location_code);
  if (url == NULL) {
    ret_val = CODE_WTTR_IN_BAD_LOCATION;
    goto release_resources;
  }
  CURLcode res;
  readed_data_struct readed_curl_data = {NULL, 0};
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, process_curl_data);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&readed_curl_data);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    ret_val = CODE_WTTR_IN_REQUEST_FAILED;
    print_curl_error(res);
    goto release_resources;
  }
  char *raw_http_response = readed_curl_data.buf;
  ret_val = parse_response_json(raw_http_response, forecast_processor);
release_resources:
  free(readed_curl_data.buf);
  curl_easy_cleanup(curl);
  free(url);
  return ret_val;
}
