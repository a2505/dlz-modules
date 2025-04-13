#include <arpa/inet.h> // 用于IP地址转换
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlz_dbi.h>
#include <dlz_list.h>
#include <dlz_minimal.h>
#include <dlz_pthread.h>

#include "http_client.h"

/*
 * Define DLZ_DLOPEN_VERSION to different values to use older versions
 * of the interface
 */
#ifndef DLZ_DLOPEN_VERSION
#define DLZ_DLOPEN_VERSION 3
#define DLZ_DLOPEN_AGE	   0
#endif /* ifndef DLZ_DLOPEN_VERSION */

void
get_ecs_subnet_ip(restapi_instance_t *instance, dns_clientinfo_t *clientinfo,
		  char **subnet_ip);
void
get_client_ip(restapi_instance_t *instance, dns_clientinfo_t *client,
	      dns_clientinfomethods_t *methods, char **ipstr);
static void
b9_add_helper(restapi_instance_t *db, const char *helper_name, void *ptr);

isc_result_t
dlz_create(const char *dlzname, unsigned int argc, char *argv[], void **dbdata,
	   ...) {
	restapi_instance_t *instance = NULL;
	/* allocate memory for MySQL instance */
	instance = calloc(1, sizeof(restapi_instance_t));
	if (instance == NULL) {
		return (ISC_R_NOMEMORY);
	}
	memset(instance, 0, sizeof(restapi_instance_t));
	va_list ap;
	const char *helper_name;
	/* Fill in the helper functions */
	va_start(ap, dbdata);
	while ((helper_name = va_arg(ap, const char *)) != NULL) {
		b9_add_helper(instance, helper_name, va_arg(ap, void *));
	}
	va_end(ap);
	curl_global_init(CURL_GLOBAL_DEFAULT);
	*dbdata = instance;

	for (size_t i = 0; i < argc; i++) {
		instance->log(ISC_LOG_DEBUG(4), "Arg %d: %s", i + 1, argv[i]);
	}
	// 获取base api
	instance->base_api = get_parameter_value(argv[1], "base_api=");

	instance->log(ISC_LOG_DEBUG(4),"----------------");
	instance->log(ISC_LOG_DEBUG(4),"1:%s 2:%s",argv[1],instance->base_api);

	char *zone = calloc(sizeof(char),
			    strlen(instance->base_api) + strlen(argv[2]) + 1);

	strcpy(zone, instance->base_api);
	strcat(zone, argv[2]);
	instance->zone_api = zone;			
	instance->log(ISC_LOG_DEBUG(4),"zone:%s",instance->zone_api);


	char *lookup = calloc(sizeof(char),
			      strlen(instance->base_api) + strlen(argv[3]) + 1);
	strcpy(lookup, instance->base_api);
	strcat(lookup, argv[3]);
	instance->lookup_api = lookup;
	instance->log(ISC_LOG_DEBUG(4),"lookup_api:%s",instance->lookup_api);

	char *authority = calloc(sizeof(char),strlen(instance->base_api) + strlen(argv[4])+1);
	strcpy(authority, instance->base_api);
	strcat(authority, argv[4]);
	instance->authority = authority;
	instance->log(ISC_LOG_DEBUG(4),"authority_api:%s",instance->authority);

	return (ISC_R_SUCCESS);
}

int
dlz_version(unsigned int *flags) {
	*flags |= (DNS_SDLZFLAG_RELATIVEOWNER | DNS_SDLZFLAG_RELATIVERDATA |
		   DNS_SDLZFLAG_THREADSAFE);
	return (DLZ_DLOPEN_VERSION);
}

isc_result_t
dlz_lookup(const char *zone, const char *name, void *dbdata,
	   dns_sdlzlookup_t *lookup, dns_clientinfomethods_t *methods,
	   dns_clientinfo_t *clientinfo) {
	restapi_instance_t *instance = (restapi_instance_t *)dbdata;
	instance->log(ISC_LOG_DEBUG(3), "Zone: %s, Name: %s", zone, name);
	char *client_ipaddr;
	char *subnet_ip;
	//get_ecs_subnet_ip(instance, clientinfo, &subnet_ip);
	// get_client_ip(instance, clientinfo, methods, &client_ipaddr);
	// instance->log(ISC_LOG_DEBUG(3), "Client IP: %s, Subnet IP: %s",
	// 	      client_ipaddr, subnet_ip);
	lookup_result_array *arr;
	isc_result_t result = restapi_lookup(instance,zone, name, &arr);
	if (result == ISC_R_SUCCESS) {
		printf("\r\nfind count %d\n", arr->count);
		for (int i = 0; i < arr->count; i++) {
			//instance->log(ISC_LOG_DEBUG(3), "\n i = %d", i);
			instance->putrr(lookup, arr->results[i].type,
					arr->results[i].ttl,
					arr->results[i].data);
		}
		return (ISC_R_SUCCESS);
	}
	return (ISC_R_FAILURE);
}

isc_result_t
dlz_findzonedb(void *dbdata, const char *name, dns_clientinfomethods_t *methods,
	       dns_clientinfo_t *clientinfo) {
	UNUSED(dbdata);
	UNUSED(name);
	UNUSED(methods);
	UNUSED(clientinfo);
	return find_zone((restapi_instance_t *)dbdata,name);
}

isc_result_t
dlz_authority(const char *zone, void *dbdata, dns_sdlzlookup_t *lookup){
	return find_authority((restapi_instance_t *)dbdata,lookup,zone);
}

void
get_ecs_subnet_ip(restapi_instance_t *instance, dns_clientinfo_t *clientinfo,
		  char **subnet_ip) {
	// IPv4 demo of inet_ntop() and inet_pton()
	struct sockaddr_in sa;
	char str[INET_ADDRSTRLEN];
	// now get it back and print it
	inet_ntop(AF_INET, &(clientinfo->ecs.addr.type.in), str,
		  INET_ADDRSTRLEN);
	instance->log(ISC_LOG_DEBUG(4), "Subnet's IP:%s", str);
	*subnet_ip = str;
}

void
get_client_ip(restapi_instance_t *instance, dns_clientinfo_t *client,
	      dns_clientinfomethods_t *methods, char **ipstr) {
	isc_sockaddr_t *addr = NULL;
	if (methods->sourceip(client, &addr) == ISC_R_SUCCESS) {
		if (addr->type.sa.sa_family == AF_INET) { // IPv4 地址
			struct sockaddr_in *sockaddr_ipv4 =
				(struct sockaddr_in *)&addr->type.sa;
			char ip_str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ip_str,
				  sizeof(ip_str));
			*ipstr = ip_str;
			instance->log(ISC_LOG_DEBUG(4), "IPv4 Address: %s\n",
				      ip_str);
		} else if (addr->type.sa.sa_family == AF_INET6) { // IPv6 地址
			struct sockaddr_in6 *sockaddr_ipv6 =
				(struct sockaddr_in6 *)&addr->type.sa;
			char ip_str[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &sockaddr_ipv6->sin6_addr, ip_str,
				  sizeof(ip_str));
			*ipstr = ip_str;
			instance->log(ISC_LOG_DEBUG(4), "IPv6 Address: %s\n",
				      ip_str);
		} else {
			instance->log(ISC_LOG_DEBUG(4),
				      "Unknown Address Family: %d",
				      addr->type.sa.sa_family);
		}
	}
}

/*
 * Register a helper function from the bind9 dlz_dlopen driver
 */
static void
b9_add_helper(restapi_instance_t *db, const char *helper_name, void *ptr) {
	if (strcmp(helper_name, "log") == 0) {
		db->log = (log_t *)ptr;
	}
	if (strcmp(helper_name, "putrr") == 0) {
		db->putrr = (dns_sdlz_putrr_t *)ptr;
	}
	if (strcmp(helper_name, "putnamedrr") == 0) {
		db->putnamedrr = (dns_sdlz_putnamedrr_t *)ptr;
	}
	if (strcmp(helper_name, "writeable_zone") == 0) {
		db->writeable_zone = (dns_dlz_writeablezone_t *)ptr;
	}
}

void
dlz_destroy(void *dbdata) {
	/* use libcurl, then before exiting... */
	curl_global_cleanup();
}