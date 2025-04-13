#include "http_client.h"

static size_t
cb(char *data, size_t size, size_t nmemb, void *clientp) {
	size_t realsize = size * nmemb;
	struct memory *mem = (struct memory *)clientp;

	char *ptr = realloc(mem->response, mem->size + realsize + 1);
	if (!ptr)
		return 0; /* out of memory */

	mem->response = ptr;
	memcpy(&(mem->response[mem->size]), data, realsize);
	mem->size += realsize;
	mem->response[mem->size] = 0;

	return realsize;
}

int
find_zone(restapi_instance_t *instance, const char *name) {
	// 创建根对象
	cJSON *root = cJSON_CreateObject();
	// 添加键值对
	cJSON_AddStringToObject(root, "zone", name);
	// 生成 JSON 字符串
	char *json_str = cJSON_Print(root);
	cJSON_Delete(root);
	CURL *curl;
	curl = curl_easy_init();
	if (curl) {
		CURLcode res;
		struct curl_slist *headers = NULL;
		struct memory chunk = { 0 };
		curl_easy_setopt(curl, CURLOPT_URL,instance->zone_api);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
		// 设置http发送的内容类型为JSON
		// 构建HTTP报文头
		headers = curl_slist_append(
			headers, "Content-Type:application/json;charset=UTF-8");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		/* if we do not provide POSTFIELDSIZE, libcurl calls strlen() by
		 * itself */
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
				 (long)strlen(json_str));

		/* Perform the request, res gets the return code */
		res = curl_easy_perform(curl);
		/* Check for errors */
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));
		/* always cleanup */
		curl_easy_cleanup(curl);
		cJSON *root = cJSON_Parse(chunk.response);
		// 提取字段
		cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "count");
		return result->valueint > 0 ? ISC_R_SUCCESS : ISC_R_NOTFOUND;
	}
	return ISC_R_FAILURE;
}

int
restapi_lookup(restapi_instance_t *instance, const char *zone,
	       const char *record, lookup_result_array **rs) {
	// 创建根对象
	cJSON *root = cJSON_CreateObject();
	// 添加键值对
	cJSON_AddStringToObject(root, "zone", zone);
	cJSON_AddStringToObject(root, "record", record);
	// 生成 JSON 字符串
	char *json_str = cJSON_Print(root);
	cJSON_Delete(root);
	CURL *curl;
	curl = curl_easy_init();
	lookup_result_array *arr = malloc(sizeof(lookup_result_array));
	arr->count = 0;
	*rs = arr;
	if (curl) {
		CURLcode res;
		struct curl_slist *headers = NULL;
		struct memory chunk = { 0 };
		curl_easy_setopt(curl, CURLOPT_URL,instance->lookup_api);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
		printf("\r\nRequest JSON: %s\n", json_str);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
		// 设置http发送的内容类型为JSON
		// 构建HTTP报文头
		headers = curl_slist_append(
			headers, "Content-Type:application/json;charset=UTF-8");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		/* if we do not provide POSTFIELDSIZE, libcurl calls strlen() by
		 * itself */
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
				 (long)strlen(json_str));

		/* Perform the request, res gets the return code */
		res = curl_easy_perform(curl);
		/* Check for errors */
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));
		/* always cleanup */
		curl_easy_cleanup(curl);
		printf("\r\nRespone JSON: %s\n", chunk.response);
		cJSON *root = cJSON_Parse(chunk.response);
		// 提取字段
		cJSON *count = cJSON_GetObjectItemCaseSensitive(root, "count");
		cJSON *record = cJSON_GetObjectItemCaseSensitive(root,
								 "record");
		// 遍历 "record" 数组并分配结果内存
		int i = 0;
		arr->count = count->valueint;
		if (arr->count <= 0) {
			return ISC_R_FAILURE; // 无效记录数
		}
		arr->results = calloc(sizeof(lookup_result), arr->count);
		if (arr->results == NULL) {
			return ISC_R_NOMEMORY; // 内存分配失败
		}
		cJSON *item = NULL;
		cJSON_ArrayForEach(item, record) {
			cJSON *ttl = cJSON_GetObjectItemCaseSensitive(item,
								      "ttl");
			cJSON *type = cJSON_GetObjectItemCaseSensitive(item,
								       "type");
			cJSON *data = cJSON_GetObjectItemCaseSensitive(item,
								       "data");
			cJSON *mx_priority = cJSON_GetObjectItemCaseSensitive(
				item, "mx_priority");
			int mx_is_null = cJSON_IsNull(mx_priority);
			if (!mx_is_null) {
				char *tmpString;
				// 计算转换后的字符串长度（不含结尾空字符）
				int len = snprintf(NULL, 0, "%d",
						   mx_priority->valueint);
				unsigned int num = strlen(data->valuestring) +
						   len;
				tmpString = calloc(sizeof(char), len + 2);
				char *mx_str = calloc(sizeof(char), len + 1);
				snprintf(mx_str, len + 1, "%d",
					 mx_priority->valueint);
				strcpy(tmpString, mx_str);
				strcat(tmpString, " ");
				strcat(tmpString, data->valuestring);
				arr->results[i].data = tmpString;
			} else {
				arr->results[i].data = data->valuestring;
			}
			arr->results[i].ttl = ttl->valueint;
			arr->results[i].type = type->valuestring;
			i++;
		}
		return count->valueint > 0 ? ISC_R_SUCCESS : ISC_R_NOTFOUND;
	}
	return ISC_R_FAILURE;
}

int
find_authority(restapi_instance_t *instance,dns_sdlzlookup_t *lookup, const char *zone){
	// 创建根对象
	cJSON *root = cJSON_CreateObject();
	// 添加键值对
	cJSON_AddStringToObject(root, "zone", zone);
	// 生成 JSON 字符串
	char *json_str = cJSON_Print(root);
	cJSON_Delete(root);
	CURL *curl;
	curl = curl_easy_init();
	if (curl) {
		CURLcode res;
		struct curl_slist *headers = NULL;
		struct memory chunk = { 0 };
		curl_easy_setopt(curl, CURLOPT_URL,instance->authority);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
		printf("\r\nRequest JSON: %s\n", json_str);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
		// 设置http发送的内容类型为JSON
		// 构建HTTP报文头
		headers = curl_slist_append(
			headers, "Content-Type:application/json;charset=UTF-8");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		/* if we do not provide POSTFIELDSIZE, libcurl calls strlen() by
		 * itself */
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
				 (long)strlen(json_str));

		/* Perform the request, res gets the return code */
		res = curl_easy_perform(curl);
		/* Check for errors */
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));
		/* always cleanup */
		curl_easy_cleanup(curl);
		printf("\r\nRespone JSON: %s\n", chunk.response);
		cJSON *root = cJSON_Parse(chunk.response);
		// 提取字段
		cJSON *count = cJSON_GetObjectItemCaseSensitive(root, "count");
		cJSON *record = cJSON_GetObjectItemCaseSensitive(root,
								 "record");
		cJSON *item = NULL;
		cJSON_ArrayForEach(item, record) {
			cJSON *ttl = cJSON_GetObjectItemCaseSensitive(item,
								      "ttl");
			cJSON *type = cJSON_GetObjectItemCaseSensitive(item,
								       "type");
			cJSON *data = cJSON_GetObjectItemCaseSensitive(item,
								       "data");

			instance->putrr(lookup,type->valuestring,ttl->valueint,data->valuestring);
		}
		return count->valueint > 0 ? ISC_R_SUCCESS : ISC_R_NOTFOUND;
	}
	return ISC_R_FAILURE;
}