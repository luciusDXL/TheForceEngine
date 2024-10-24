#include <TFE_FileSystem/filestream.h>
#include <TFE_System/system.h>
#include "curl/curl.h"
#include <string>

using namespace std;

namespace Download
{
	
	// Store curl results in a string
	size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
		size_t newLength = size * nmemb;
		s->append((char*)contents, newLength);
		return newLength;
	}

	string curlWeb(const char* webPath)
	{
		CURL* req = curl_easy_init();
		CURLcode res;
		string responseString;

		string responseCharPtr = "";

		if (req)
		{
			curl_easy_setopt(req, CURLOPT_URL, webPath);
			curl_easy_setopt(req, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(req, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(req, CURLOPT_WRITEDATA, &responseString);

			res = curl_easy_perform(req);

			if (res != CURLE_OK)
			{
				fprintf(stderr, "curl_easy_operation() failed : %s\n", curl_easy_strerror(res));
			}
			else {
				responseCharPtr = responseString.c_str();
			}
			curl_easy_cleanup(req);
		}
		return responseCharPtr;
	}

	size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream)
	{
		size_t written = fwrite(ptr, size, nmemb, stream);
		return written;
	}

	bool download(const char* webPath, const char* downloadPath)
	{
		TFE_System::logWrite(LOG_MSG, "Download", "Download started for %s", webPath);
		bool result = false;

		FILE* outFile = fopen(downloadPath, "wb");
		if (!outFile)
		{
			return result;
		}

		

		CURL* req = curl_easy_init();
		CURLcode res;
		if (req)
		{
			curl_easy_setopt(req, CURLOPT_URL, webPath);
			curl_easy_setopt(req, CURLOPT_URL, webPath);
			curl_easy_setopt(req, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(req, CURLOPT_WRITEFUNCTION, write_data);
			curl_easy_setopt(req, CURLOPT_WRITEDATA, outFile);

			res = curl_easy_perform(req);

			if (res != CURLE_OK)
			{
				TFE_System::logWrite(LOG_ERROR, "Download", "Failed downloading due to %s", curl_easy_strerror(res));
			}
			else
			{
				result = true;
			}
		}
		curl_easy_cleanup(req);
		
		fclose(outFile);

		if (!result) remove(downloadPath);

		TFE_System::logWrite(LOG_MSG, "Download", "Finished Download for %s", webPath);
		return result;
	}
}