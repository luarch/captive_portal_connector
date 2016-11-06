#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <curl/curl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <stdexcept>
using namespace std;

string TESTUSERNAME;
string TESTPASSWORD;
int retrytime=900;
int checktime=15;
#define LOGIN_SERVLET "https://wlan.ct10000.com/authServlet"
#define LOGOUT_SERVLET "https://wlan.ct10000.com/logoutServlet"

size_t dummy_write_data(void *buffer, size_t size, size_t nmemb, void *userp) {
    return size*nmemb;
}

size_t string_write_data(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool checkPortal() {
    /*
     * Connect https://wlan.ct10000.com/ to see if there's an error.
     */
    CURL *curl = curl_easy_init();
    long response_code;
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://wlan.ct10000.com/errorpage/showNatFail_ctsh.jsp");

        /* complete within 2 seconds */
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dummy_write_data);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            return false;
        } else {
            res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if((res == CURLE_OK) && ((response_code / 100) != 2)) {
                curl_easy_cleanup(curl);
                return false;
            } else {
                curl_easy_cleanup(curl);
                return true;
            }
        }
    } else {
        throw runtime_error("Error when establishing connection to TP");
    }
}

/**
 * @param [out] char* location - login url
 */
bool checkConnectivity(char *login_url) {
    /*
     * Connect http://connect.rom.miui.com/ to see if we will get a redirect location.
     */
    CURL *curl;
    CURLcode res;
    long response_code;
    char* location;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://connect.rom.miui.com");
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dummy_write_data);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK)
            return false;
        else {
            res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if((res == CURLE_OK) && ((response_code / 100) != 3)) {
                /* a redirect implies a 3xx response code */
                curl_easy_cleanup(curl);
                return true;
            } else {
                res = curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &location);

                if((res == CURLE_OK) && location) {
                    strcpy(login_url, location);
                    curl_easy_cleanup(curl);
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

bool connectPortal(string username, string password, char* login_url, char* logout_url) {
    CURL *curl;
    CURLcode res;
    string response_text;
    string paramStrEnc;
    string paramStr;
    /*
     * Get login_url, to find paramStr
     */
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, login_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_text);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK) {
            return false;
        } else {
            /* find paramStr, put it into variable paramStr */
            string paramStrPrefix="paramStr=";
            int paramStrOffest = response_text.find(paramStrPrefix);
            int paramStrEndIndex = response_text.find("\"", paramStrOffest);
            if(paramStrOffest == -1 || paramStrEndIndex == -1) {
                // Can't find paramStr
                curl_easy_cleanup(curl);
                return false;
            }
            paramStrOffest += paramStrPrefix.length();
            int paramStrLen = paramStrEndIndex - paramStrOffest;
            paramStr = response_text.substr(paramStrOffest, paramStrLen);

            /* escape paramStr to paramStrEnc */
            char* out = curl_easy_escape(curl, paramStr.c_str(), 0);
            paramStrEnc.append(out);
            curl_free(out);
        }

        curl_easy_cleanup(curl);
    } else {
        throw runtime_error("Error when establishing connection to find paramStr");
    }

    /*
     * Post data to LOGIN_SERVLET
     */
    response_text.erase();
    long response_code;
    char payload[2048];
    char* location;
    sprintf(payload, "UserName=%s&PassWord=%s&verifycode=&UserType=1&paramStr=%s&paramStrEnc=%s&isChCardUser=false&isWCardUser=false&province=student&isRegisterRealm=false&defaultProv=sh&isCookie=&cookieType=&UserName1=%s&hiddenAccountAddress=sh&accountAddress=sh&PassWord1=%s",
            username.c_str(), password.c_str(), paramStr.c_str(), paramStrEnc.c_str(), username.c_str(), password.c_str());

    /* get a curl handle */
    curl = curl_easy_init();
    if(curl) {
        /* First set the URL that is about to receive our POST. This URL can
           just as well be a https:// URL if that is what should receive the
           data. */
        curl_easy_setopt(curl, CURLOPT_URL, LOGIN_SERVLET);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        /* Now specify the POST data */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_text);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            return false;
        } else {
            res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if((res == CURLE_OK) && ((response_code / 100) != 3)) {
                /* a redirect implies a 3xx response code */
                curl_easy_cleanup(curl);
                return false;
            }
            else {
                res = curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &location);

                if((res == CURLE_OK) && location) {
                    // Check out if there's a login error
                    char* fail_str = strstr(location, "fail");
                    if(fail_str) {
                        // Login failed
                        curl_easy_cleanup(curl);
                        return false;
                    } else {
                        // Login succeeded.
                        strcpy(logout_url, location);
                        curl_easy_cleanup(curl);
                        return true;
                    }
                } else {
                    curl_easy_cleanup(curl);
                    return false;
                }
            }
        }

        curl_easy_cleanup(curl);
    } else {
        throw runtime_error("Error when establishing connection in login");
    }
    return false;
}

bool disconnectPortal(char* logout_url) {
    /*
     * Get data to http://172.30.0.94/F.htm
     */
    CURL *curl = curl_easy_init();
    char payload[2048];

    /* find paramStr from logout_url */
    string paramStrEnc;
    string paramStr;
    string paramStrPrefix="paramStr=";
    string response_text;
    response_text.append(logout_url);
    int paramStrOffest = response_text.find(paramStrPrefix);
    if(paramStrOffest == -1) {
        // Can't find paramStr
        return false;
    }
    paramStrOffest += paramStrPrefix.length();
    paramStr = response_text.substr(paramStrOffest, response_text.length()-paramStrOffest);

    /* escape paramStr to paramStrEnc */
    char* out = curl_easy_escape(curl, paramStr.c_str(), 0);
    paramStrEnc.append(out);
    curl_free(out);

    sprintf(payload, "pagetype=5&ip=127.0.0.1&realIp=127.0.0.1&bOffline=&paramStr=%s&paramStrEnc=%s",
            paramStr.c_str(), paramStrEnc.c_str());


    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, LOGOUT_SERVLET);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dummy_write_data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            return false;
        } else {
            return true;
        }
    } else {
        throw runtime_error("Error when establishing connection in logout");
    }
    return false;
}

bool Initialization()
{
    FILE *fp;
    if(fp=fopen("conf.txt","r")){
        int enough=4;
        cout<<"Load Configuration File Success"<<endl;
        char bufferread[1024]={'\0'};
        while(!feof(fp)){
            bzero(bufferread,1024);
            fgets(bufferread,1024,fp);
            if(bufferread==NULL) {
                fclose(fp);
            }
            else {
                string read=bufferread;
                read=read.substr(0,read.find("#",0));
                if(read==""){
                    if(enough) 
                        return false;
                    else return true;
                } 
                string property,value;
                property=read.substr(0,read.find("=",0));
                value=read.substr(read.find("=",0)+1,read.length()-read.find("=",0)-1);
                //cout<<property<<"="<<value<<endl;
                if(property=="username"){
                    enough--;
                    TESTUSERNAME=value.substr(0,value.length()-1);
                }
                else if(property=="password"){
                    enough--;
                    TESTPASSWORD=value.substr(0,value.length()-1);;
                }
                else if(property=="retrytime"){
                    enough--;
                    retrytime=atoi(value.data());
                }
                else if(property=="checktime"){
                    enough--;
                    checktime=atoi(value.data());
                }
            } 
        }
        cout<<"Configuration file read successfully"<<endl;
        fclose(fp);
        return true;
    }
    return false;
}

int main() {
    signal(SIGCHLD,SIG_IGN);
    int cmd = 0;
    char loginUrl[2048]={0}, logoutUrl[2048]={0};
    int Detection=0;
    if(!Initialization()){
        cout<<"Configuration file read failed"<<endl;
        return 1;
    }
    while(true){
         cin>>cmd;
         switch(cmd){
            case 0:
                if(Detection)
                    kill(Detection,SIGSTOP);
                cout<<"Exit"<<endl;
                return 1;
            case 1:
                if(checkPortal()){
                    if(!checkConnectivity(loginUrl))
                        cout<<"ConnectPortal "<<((connectPortal(TESTUSERNAME,TESTPASSWORD,loginUrl,logoutUrl)==true)?"Success":"Fail")<<endl;
                    else cout<<"Already Connect"<<endl;
                    if(!Detection){
                        Detection=fork();
                        if(!Detection){
                            do{
                                if(checkPortal()){
                                    if(!checkConnectivity(loginUrl))
                                        cout<<"ReConnectPortal "<<((connectPortal(TESTUSERNAME,TESTPASSWORD,loginUrl,logoutUrl)==true)?"Success":"Fail")<<endl;
                                    sleep(retrytime);
                                }
                                else
                                    sleep(checktime);
                            }while(getppid()!=1);
                            exit(0);
                        }
                    }
                }else
                    cout<<"Please Connect tongji-student-portal First"<<endl;
                break;
            case 2:
                if(checkConnectivity(loginUrl)){
                    if(Detection){
                        kill(Detection,SIGSTOP);
                        Detection=0;
                    }
                    cout<<"DisconnectPortal "<<((disconnectPortal(logoutUrl)==true)?"Success":"Fail")<<endl;
                }
                else 
                    cout<<"Already Disconnect"<<endl;
                break;
            case 3:
                if(!checkConnectivity(loginUrl))
                        cout<<"ConnectPortal "<<((connectPortal(TESTUSERNAME,TESTPASSWORD,loginUrl,logoutUrl)==true)?"Success":"Fail")<<endl;
            default:
                break;
         }
    }
    return 0;
}
