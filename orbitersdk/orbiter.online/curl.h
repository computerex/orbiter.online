#include <string>

struct MemoryStruct {
	char *memory;
	size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

std::string curl_get(std::string url);

std::string curl_post(std::string url, std::string payload);

void curl_clean();