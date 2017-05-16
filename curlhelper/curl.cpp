#include <cstdlib>
#include <stdio.h>
#include <cstring>
#include <curl\curl.h>
#include "curl.h"

CRITICAL_SECTION cs2;

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;

	mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
	if (mem->memory == NULL) {
		/* out of memory! */
		printf("not enough memory (realloc returned NULL)\n");
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

struct WriteThis {
	const char *readptr;
	long sizeleft;
};

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	struct WriteThis *pooh = (struct WriteThis *)userp;

	if (size*nmemb < 1)
		return 0;

	if (pooh->sizeleft) {
		*(char *)ptr = pooh->readptr[0]; /* copy one single byte */
		pooh->readptr++;                 /* advance pointer */
		pooh->sizeleft--;                /* less data left */
		return 1;                        /* we return 1 byte at a time! */
	}

	return 0;                          /* no more data left to deliver */
}
bool init = false;
CURL *curl_handle;
CURLcode res;
std::string curl_post(std::string url, std::string payload)
{
	if (!init){
		curl_global_init(CURL_GLOBAL_ALL);
		InitializeCriticalSection(&cs2);
		init = true;
	}
	EnterCriticalSection(&cs2);
	curl_handle = curl_easy_init();
	struct WriteThis pooh;
	struct curl_slist *headers = NULL;                      /* http headers to send with request */
	char buffer[512];
	pooh.readptr = payload.c_str();
	pooh.sizeleft = (long)strlen(pooh.readptr);
	struct MemoryStruct chunk;

	chunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */
	chunk.size = 0;    /* no data at this point */

	curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "cookie.txt");
	curl_easy_setopt(curl_handle, CURLOPT_COOKIEJAR, "cookie.txt");

	/* specify URL to get */
	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());

	/* Now specify we want to POST data */
	curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);

	/* we want to use our own read function */
	curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, read_callback);

	/* pointer to pass to our read function */
	curl_easy_setopt(curl_handle, CURLOPT_READDATA, &pooh);

	/* send all data to this function  */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

	/* some servers don't like requests that are made without a user-agent
	field, so we provide one */
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	/* Set the expected POST size. If you want to POST large amounts of data,
	consider CURLOPT_POSTFIELDSIZE_LARGE */
	curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, pooh.sizeleft);

	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);

	/* get it! */
	res = curl_easy_perform(curl_handle);

	/* check for errors */
	if (res != CURLE_OK) {
		
		sprintf_s(buffer, "curl_easy_perform() failed: %s\n",
			curl_easy_strerror(res));
	}
	else {
		/*
		* Now, our chunk.memory points to a memory block that is chunk.size
		* bytes big and contains the remote file.
		*
		* Do something nice with it!
		*/

		printf("%lu bytes retrieved\n", (long)chunk.size);
	}
	curl_easy_cleanup(curl_handle);
	LeaveCriticalSection(&cs2);
	return std::string(chunk.memory);
}
std::string curl_get(std::string url)
{
	if (!init){
		curl_global_init(CURL_GLOBAL_ALL);
		InitializeCriticalSection(&cs2);
		init = true;
	}
	EnterCriticalSection(&cs2);
	curl_handle = curl_easy_init();
	struct MemoryStruct chunk;

	chunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */
	chunk.size = 0;    /* no data at this point */

	curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "cookie.txt");
	curl_easy_setopt(curl_handle, CURLOPT_COOKIEJAR, "cookie.txt");

	/* specify URL to get */
	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());

	/* send all data to this function  */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

	/* some servers don't like requests that are made without a user-agent
	field, so we provide one */
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	/* get it! */
	res = curl_easy_perform(curl_handle);

	/* check for errors */
	if (res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
			curl_easy_strerror(res));
	}
	else {
		/*
		* Now, our chunk.memory points to a memory block that is chunk.size
		* bytes big and contains the remote file.
		*
		* Do something nice with it!
		*/

		printf("%lu bytes retrieved\n", (long)chunk.size);
	}
	curl_easy_cleanup(curl_handle);
	LeaveCriticalSection(&cs2);
	return std::string(chunk.memory);
}

void curl_clean()
{
	curl_global_cleanup();
}