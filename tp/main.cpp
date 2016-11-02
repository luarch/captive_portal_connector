#include <iostream>
#include <string>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <curl/curl.h>
using namespace std;

#define TESTUSERNAME "1452266"
#define TESTPASSWORD ""

size_t dummy_write_data(void *buffer, size_t size, size_t nmemb, void *userp) {
    return size*nmemb;
}

size_t string_write_data(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool checkPortal() {
    /*
     * Connect http://172.30.0.94/ to see if there's an error.
     */
    CURL *curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://172.30.0.94");

        /* complete within 2 seconds */
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dummy_write_data);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            //fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    //curl_easy_strerror(res));
            return false;
        } else {
            return true;
        }
    } else {
        throw runtime_error("Error when establishing connection to TP");
    }
}

bool checkConnectivity() {
    /*
     * Connect http://captive.apple.com/ to see if we will get a redirect location.
     */
    CURL *curl;
    CURLcode res;
    char *location;
    long response_code;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://captive.apple.com");
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dummy_write_data);

        /* captive.apple.com is redirected, figure out the redirection! */

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK)
            return false;
        else {
            res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if((res == CURLE_OK) && ((response_code / 100) != 3)) {
                /* a redirect implies a 3xx response code */
                return true;
            }
            else {
                res = curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &location);

                if((res == CURLE_OK) && location) {
                    //printf("Redirected to: %s\n", location);
                    return false;
                }
            }
        }

        curl_easy_cleanup(curl);
    } else {
        throw runtime_error("Error when establishing connection to captive.apple.com");
    }
    return false;
}

bool connectPortal(string username, string password) {
    /*
     * Post data to http://172.30.0.94/
     */
    CURL *curl;
    CURLcode res;
    char payload[1024];
    sprintf(payload, "DDDDD=%s&upass=%s&0MKKey=", username.c_str(), password.c_str());
    string response_text;

    /* get a curl handle */
    curl = curl_easy_init();
    if(curl) {
        /* First set the URL that is about to receive our POST. This URL can
           just as well be a https:// URL if that is what should receive the
           data. */
        curl_easy_setopt(curl, CURLOPT_URL, "http://172.30.0.94/");
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        /* Now specify the POST data */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_text);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            //fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    //curl_easy_strerror(res));
            return false;
        } else {
            if(response_text.find("successfully logged into")==-1) {
                return false;
            } else {
                return true;
            }
        }

        curl_easy_cleanup(curl);
    } else {
        throw runtime_error("Error when establishing connection in login");
    }
    return 0;
}

bool disconnectPortal() {
    /*
     * Get data to http://172.30.0.94/F.htm
     */
    CURL *curl = curl_easy_init();
    string response_text;
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://172.30.0.94/F.htm");
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_text);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            return false;
        } else {
            if(response_text.find("Msg=14")!=-1) {
                return true;
            } else {
                return false;
            }
        }
    } else {
        throw runtime_error("Error when establishing connection in logout");
    }
    return false;
}

int main() {
    int cmd = 0;
    while(true) {
        cin >> cmd;
        switch(cmd) {
            case 0:
                cout << "Exit" << endl;
                exit(0);
                break;
            case 1:
                cout << "CheckPortal " << checkPortal() << endl;
                break;
            case 2:
                cout << "CheckConnectivity " << checkConnectivity() << endl;
                break;
            case 3:
                cout << "ConnectPortal " << connectPortal(TESTUSERNAME, TESTPASSWORD) << endl;
                break;
            case 4:
                cout << "DisconnectPortal " << disconnectPortal() << endl;
                break;
        }
    }
    return 0;
}
