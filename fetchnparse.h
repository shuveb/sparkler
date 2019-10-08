#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "json.h"

#define TWEET_URL       "https://sparkler-service.herokuapp.com/tweet"
#define WEATHER_URL     "https://sparkler-service.herokuapp.com/weather"
#define AIR_QUALITY_URL "https://sparkler-service.herokuapp.com/air_quality"

struct MemoryStruct {
    char *memory;
    size_t size;
};

char *fetch_latest_tweet();
char *fetch_weather(char *city);
char *fetch_air_quality(char *country, char *city);
