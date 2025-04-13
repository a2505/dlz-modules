#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

#include <dlz_minimal.h>

#define ZONE_EXSITS	1
#define ZONE_NOT_EXSITS -1

typedef struct {
	char *base_api;
	char *zone_api;
	char *lookup_api;
	char *authority;
	/* Helper functions from the dlz_dlopen driver */
	log_t *log;
	dns_sdlz_putrr_t *putrr;
	dns_sdlz_putnamedrr_t *putnamedrr;
	dns_dlz_writeablezone_t *writeable_zone;
} restapi_instance_t;

struct memory {
	char *response;
	size_t size;
};

typedef struct {
	char *type;
	int16_t ttl;
	char *data;
} lookup_result;

typedef struct {
	lookup_result *results; // 指向 lookup_result_t 数组的指针
	int count;		// 数组中元素的数量
} lookup_result_array;

int
find_zone(restapi_instance_t *instance, const char *name);

int
restapi_lookup(restapi_instance_t *instance, const char *zone,
	       const char *record, lookup_result_array **rs);

int
find_authority(restapi_instance_t *instance, dns_sdlzlookup_t *lookup,
	       const char *zone);