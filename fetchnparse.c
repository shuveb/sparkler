#include "fetchnparse.h"
#include "json.h"

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

CURL *curl_handle;
struct MemoryStruct chunk;

int _fetch_url(char *url)
{
    CURLcode res;

    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "sparkler-agent/1.0");

    /* get it! */
    res = curl_easy_perform(curl_handle);

    /* check for errors */
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        return -1;
    }
    else {
        /*
         * Now, our chunk.memory points to a memory block that is chunk.size
         * bytes big and contains the remote file.
         *
         * Do something nice with it!
         */

        printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
    }
    return 0;
}

void _fetch_cleanup() {
    curl_easy_cleanup(curl_handle);
    free(chunk.memory);
    curl_global_cleanup();
}

json_value *get_value_for_key(json_value *v, char *key) {
    for (unsigned int i = 0; i < v->u.object.length; i++) {
        if (strcmp(key, v->u.object.values[i].name) == 0)
            return v->u.object.values[i].value;
    }

    return NULL;
}

char *fetch_latest_tweet() {
    if (_fetch_url(TWEET_URL) != 0)
        return NULL;

    json_value *v = json_parse(chunk.memory, chunk.size);

    /* Make sure we got a JSON object back */
    if (v->type == json_object) {
        /* Make sure that "status" is "success" */
        json_value *v_status = get_value_for_key(v, "status");
        if (v_status == NULL)
            goto error_exit;
        if (strcmp(v_status->u.string.ptr, "success") != 0)
            goto error_exit;

        /* Get value of the "text" key inside object pointed to by "tweet" */
        json_value *v_tweet = get_value_for_key(v, "tweet");
        if (v_tweet == NULL)
            goto error_exit;
        json_value *v_tweet_text = get_value_for_key(v_tweet, "text");
        if (v_tweet_text == NULL)
            goto error_exit;
        char *tweet_text = malloc(v_tweet_text->u.string.length + 1);
        if (!tweet_text)
            goto error_exit;
        bzero(tweet_text, v_tweet_text->u.string.length + 1);
        strcpy(tweet_text, v_tweet_text->u.string.ptr);
        json_value_free(v);
        _fetch_cleanup();
        return tweet_text;
    }
    /* Gotos are bad, but they're perfect for error handling scenarios */
error_exit:
    _fetch_cleanup();
    return NULL;
}

char *fetch_air_quality(char *country, char *city) {
    char request_url[2048];

    snprintf(request_url, sizeof(request_url), "%s?country=%s&city=%s", AIR_QUALITY_URL, country, city);
    if (_fetch_url(request_url) != 0)
        return NULL;

    char *aq_report = malloc(8192);
    if (!aq_report)
        return NULL;
    bzero(aq_report, 8192);

    json_value *v = json_parse(chunk.memory, chunk.size);
    /* Make sure we got a JSON object back */
    if (v->type == json_object) {
        /* Make sure that "status" is "success" */
        json_value *v_status = get_value_for_key(v, "status");
        if (v_status == NULL)
            goto error_exit;
        if (strcmp(v_status->u.string.ptr, "success") != 0)
            goto error_exit;

        json_value *v_data = get_value_for_key(v, "data");
        if (v_data == NULL)
            goto error_exit;
        for (unsigned int i = 0; i < v_data->u.array.length; i++) {
            char record[1024];
            json_value *v_record = v_data->u.array.values[i];
            char *v_location = v_record->u.object.values[0].name;
            json_value *v_reading = v_record->u.object.values[0].value;
            snprintf(record, sizeof(record), "%s: %s\n", v_location, v_reading->u.string.ptr);
            strncat(aq_report, record, 8192);
        }
        _fetch_cleanup();
        return aq_report;
    }

    /* Gotos are bad, but they're perfect for error handling scenarios */
    error_exit:
    _fetch_cleanup();
    free(aq_report);
    return NULL;
}

char *fetch_weather(char *city) {
    char request_url[2048];

    snprintf(request_url, sizeof(request_url), "%s?city=%s", WEATHER_URL, city);
    if (_fetch_url(request_url) != 0)
        return NULL;

    char *weather_forecast = malloc(8192);
    if (!weather_forecast)
        return NULL;
    bzero(weather_forecast, 8192);

    json_value *v = json_parse(chunk.memory, chunk.size);
    /* Make sure we got a JSON object back */
    if (v->type == json_object) {
        /* Make sure that "status" is "success" */
        json_value *v_status = get_value_for_key(v, "status");
        if (v_status == NULL)
            goto error_exit;
        if (strcmp(v_status->u.string.ptr, "success") != 0)
            goto error_exit;

        json_value *v_data = get_value_for_key(v, "data");
        if (v_data == NULL)
            goto error_exit;
        json_value *v_weather = get_value_for_key(v_data, "consolidated_weather");
        if (v_weather == NULL)
            goto error_exit;

        for (unsigned int i = 0; i < v_weather->u.array.length; i++) {
            char record[1024];
            json_value *v_record = v_weather->u.array.values[i];
            json_value *v_date = get_value_for_key(v_record, "applicable_date");
            json_value *v_weather_state_name = get_value_for_key(v_record, "weather_state_name");
            json_value *v_min_temp = get_value_for_key(v_record, "min_temp");
            json_value *v_max_temp = get_value_for_key(v_record, "max_temp");
            json_value *v_humidity = get_value_for_key(v_record, "humidity");
            snprintf(record, sizeof(record), "Date: %s\n\tWeather: %s\n\tMin. temp: %.02f\n\tMax. temp: %.02f\n\tHumidity: %ld\n",
                    v_date->u.string.ptr,
                    v_weather_state_name->u.string.ptr,
                    v_min_temp->u.dbl,
                    v_max_temp->u.dbl,
                    v_humidity->u.integer
                    );
            strncat(weather_forecast, record, 8192);
        }
        _fetch_cleanup();
        return weather_forecast;
    }

    /* Gotos are bad, but they're perfect for error handling scenarios */
    error_exit:
    _fetch_cleanup();
    free(weather_forecast);
    return NULL;
}
