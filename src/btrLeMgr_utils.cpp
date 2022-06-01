/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <cerrno>
#include <unistd.h>
#include <ctime>
#include <fstream>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include "btrLeMgr_utils.h"
#include "btrLeMgr_logger.h"
#include "rfcapi.h"

#define MAX_MACADDR_SIZE            20
#define MAX_RFC_URL_SIZE            100
#define PARAM_SIZE                  512
#define DEFAULT_PREFERENCE_URL      ""    //Will need to update later
#define DEFAULT_BLE_RADIO_STATUS    false

namespace btrLeMgrUtils {

struct res_data
{
    size_t size;
    char* buff;
};

static char gpcSTBMac[MAX_MACADDR_SIZE] = {'\0'};
static char gpcRfcUrl[MAX_RFC_URL_SIZE] = {'\0'};
static char prefUrl[MAX_RFC_URL_SIZE]   = {'\0'};
static bool bTerminateBeaconDetect = false;

bool notifyLeDeviceToCloud( char* apLeDevId )
{
    bool ret = false;
    if(!apLeDevId) {
        BTRLEMGRLOG_ERROR("Failed, due to NULL");
        return (ret);
    }

    char *pMac                  = getSTBMac();
    unsigned int ui32ReqId      = rand();
    const char*  pi8CloudURL    = getRfcUrl();

    if(!pMac) {
        BTRLEMGRLOG_ERROR("Failed to get STB Mac.");
        return (ret);
    }

    if(!pi8CloudURL) {
        BTRLEMGRLOG_ERROR("Failed, RFC LE Notification server URL.");
        return (ret);
    }

    CURL *pCurl = NULL;
    CURLcode res;
    struct curl_slist *headers = NULL;

    curl_global_init(CURL_GLOBAL_ALL);

    pCurl = curl_easy_init();

    if(pCurl) {
        curl_easy_setopt(pCurl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(pCurl, CURLOPT_URL, pi8CloudURL);

        //BTRLEMGRLOG_DEBUG("Server URL Le (%s). \n", pi8CloudURL);

        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers);

        /* Specify the POST data */
        char data[200] = {'\0'};
        memset(data, 0, 200);
        snprintf(data, 199, "{\"device_id\":\"mac:%s\",\"request-id\":\"LE_%d\",\"tile_id\":\"%s\"}",pMac, ui32ReqId, apLeDevId);

        BTRLEMGRLOG_INFO("Json payload is (%s). \n", data);
        curl_easy_setopt(pCurl, CURLOPT_POSTFIELDS, data);
        res = curl_easy_perform(pCurl);

        if(res != CURLE_OK) {
            BTRLEMGRLOG_ERROR("Failed  in curl_easy_perform() with error: %s\n", curl_easy_strerror(res));
        }
        else {
            BTRLEMGRLOG_INFO("Successfully Send Cloud Notification for Le Device (%s). \n", apLeDevId);
            ret = true;
        }

        curl_easy_cleanup(pCurl);
    }
    curl_global_cleanup();

    return (ret);
}

char* getSTBMac()
{
    if(gpcSTBMac[0] != '\0') {
        return (gpcSTBMac);
    } else
        return (NULL);
}

void setSTBMac(char* apStbMac)
{
    if(apStbMac) {
        strncpy(gpcSTBMac, apStbMac, MAX_MACADDR_SIZE-1); //CID :63754: Buffer not null terminated
        BTRLEMGRLOG_DEBUG("Set STB Mac as [%s].\n\n", gpcSTBMac);
    }
}

size_t write_response(void *ptr, size_t size, size_t nmemb, struct res_data *resp)
{
    size_t index = resp->size;
    size_t n = (size * nmemb);
    char* buff;

    resp->size += (size * nmemb);

    BTRLEMGRLOG_DEBUG("data at %p size=%llu nmemb=%llu \n\n", ptr, (unsigned long long)size, (unsigned long long)nmemb);
    buff = (char*)realloc(resp->buff,resp->size + 1);

    if(buff)
      resp->buff = buff;
    else {
      if(resp->buff)
        free(resp->buff);
      BTRLEMGRLOG_ERROR("Failed to allocate memory.\n");
      return 0;
    }

    memcpy((resp->buff + index), ptr, n);
    resp->buff[resp->size] = '\0';

    return size * nmemb;
}

int getRandomTimeInSec(time_t time)
{
  int rand_int, low=-1, high=1;
  int timeToSleep;

  std:: srand(std::time(0));
  rand_int = low + int((high-low)*(rand()/RAND_MAX));
  rand_int = (rand_int < 0)? -1 : 1;
  timeToSleep = time + int(time*rand_int*0.25);

  return (timeToSleep*60);
}

void
terminateBeaconDetection (
    void
) {
    bTerminateBeaconDetect = true;
}

int updateLimitBeaconDetection()
{
    int ret = 0;

    bool isRebooted = false;
    bool isBLERadioEnabled = DEFAULT_BLE_RADIO_STATUS;

    RFC_ParamData_t param = {0};
    WDMP_STATUS status = getRFCParameter((char*)"BTRMGR", "Device.DeviceInfo.UpTime", &param);

    if (status == WDMP_SUCCESS) {
        BTRLEMGRLOG_DEBUG("name = %s, type = %d, value = %s\n", param.name, param.type, param.value);
        isRebooted = (atoi(param.value) < 600 ) ? true : false;  //Considering if uptime is 600 sec or 10 min its rebooted.
    }

    param = {0};
    status = getRFCParameter((char*)"BTRMGR", "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.BLERadio", &param);

    if (status == WDMP_SUCCESS) {
        BTRLEMGRLOG_DEBUG("name = %s, type = %d, value = %s\n", param.name, param.type, param.value);
        isBLERadioEnabled = ((!strncasecmp(param.value, "true", 4)) || (!strncasecmp(param.value, "1", 1)))? true : false;
    }

    BTRLEMGRLOG_INFO("isRebooted = %d, isBLERadioEnabled = %d .\n",isRebooted,isBLERadioEnabled);
    if(isRebooted || isBLERadioEnabled)
    {
        BTRLEMGRLOG_INFO("Device is rebooted or BLERadio is set to true.\n");
        BTRLEMGRLOG_INFO("Checking Customer preference for Beacon Detection Limit\n");

        time_t cur_t;
        time_t start_t = time(0);

        int t1 = 0, t2 = 1;
        int timeToSleep = 0, rand_t = 0;

        while(timeToSleep < 10080) {

            cur_t = time(0);
            if((cur_t - start_t) >= rand_t) {

                if(WDMP_SUCCESS == setCustomerPreference())
                    break;

                timeToSleep = t1 + t2;
                t1 = t2;
                t2 = timeToSleep;
            }
            rand_t = getRandomTimeInSec(timeToSleep);
            BTRLEMGRLOG_INFO("Sleep for rand_t = %d - cur_t = %ld start_t = %ld timeToSleep = %d\n", rand_t, cur_t, start_t, timeToSleep);

            while (rand_t--) {
                if (bTerminateBeaconDetect == true) {
                    BTRLEMGRLOG_INFO("Exiting Beacon Detection\n");
                    break;
                }

                sleep(1);
            }

            if (bTerminateBeaconDetect == true) {
                BTRLEMGRLOG_INFO("Exiting Beacon Detection\n");
                break;
            }
        }
    }

    return ret;
}

int setCustomerPreference()
{
    CURLcode res;
    int ret = -1;
    long response_code;
    char mac_header[PARAM_SIZE];

    struct res_data resp;
    struct curl_slist *headers = NULL;
 
    char *pMac                  = getSTBMac();
    const char*  pURL        = getRfcPrefUrl();

    if(!pMac) {
        BTRLEMGRLOG_ERROR("Failed to get STB Mac.\n");
        return (ret);
    }

    if(pURL == NULL ) {
        BTRLEMGRLOG_ERROR("Url is NUll\n");
        //Need to uncomment it once Default URL is present 
        //strncpy(pURL, DEFAULT_PREFERENCE_URL, strlen(DEFAULT_PREFERENCE_URL)+1);
        //if(pURL == NULL )
          return (ret);
    }

    CURL *pCurl = curl_easy_init();
    if(pCurl)
    {

      resp.size = 0;
      resp.buff = (char*)malloc(PARAM_SIZE);
      if(NULL == resp.buff) {
        BTRLEMGRLOG_ERROR("Failed to allocate memory\n");
        return ret;
      }

      resp.buff[0] = '\0';

      sprintf(mac_header,"mac: %s", pMac);
 
      //Have to update this later, once end-point is availabe.
      //headers = curl_slist_append(headers, "Content-Type: application/json");
      headers = curl_slist_append(headers, mac_header);
      //headers = curl_slist_append(headers, "Authorization: Basic ");

      curl_easy_setopt(pCurl, CURLOPT_URL, pURL);
      curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, write_response);
      curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &resp);

      res = curl_easy_perform(pCurl);
      if(res != CURLE_OK) {
        BTRLEMGRLOG_ERROR("Curl Called Failed, with error:%s\n",curl_easy_strerror(res));
        ret = -1;
      }
      else
      {
        curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &response_code);
        BTRLEMGRLOG_INFO("Curl Call Completed, Response = %ld\n",response_code);
 
        if(response_code==200)
        {
          if(!strncasecmp(resp.buff, "true", 4))
            ret = setRFCParameter((char*)"btrLeMgr_utils", "Device.DeviceInfo.X_RDKCENTRAL-COM_xBlueTooth.LimitBeaconDetection", "true", WDMP_BOOLEAN);
          else
            ret = setRFCParameter((char*)"btrLeMgr_utils", "Device.DeviceInfo.X_RDKCENTRAL-COM_xBlueTooth.LimitBeaconDetection", "false", WDMP_BOOLEAN);
        }
      }
      curl_easy_cleanup(pCurl);
    }
    return ret;
}

char* getRfcPrefUrl()
{
    RFC_ParamData_t param = {0};
    WDMP_STATUS status = getRFCParameter((char*)"BTRMGR", "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Preference.URL", &param);

    if (status == WDMP_SUCCESS) {
        BTRLEMGRLOG_INFO("name = %s, type = %d, value = %s\n", param.name, param.type, param.value);
        strncpy(prefUrl, param.value, MAX_RFC_URL_SIZE-1); //CID :163644 Buffer not null terminate
    }

    if(prefUrl[0] != '\0') {
        return (prefUrl);
    } else {
        return (NULL);
    }
}

char* getRfcUrl()
{
    if(gpcRfcUrl[0] != '\0') {
        return (gpcRfcUrl);
    } else
        return (NULL);
}

void setRfcUrl(char* apRfcUrl)
{
    if(apRfcUrl) {
        memset(gpcRfcUrl, 0, MAX_RFC_URL_SIZE);
        strncpy(gpcRfcUrl, apRfcUrl, MAX_RFC_URL_SIZE-1); //CID :163796 Buffer not null terminated 
        BTRLEMGRLOG_DEBUG("Set RFC Reporting URL [%s].\n\n", gpcRfcUrl);
    }
}

bool isFile_exists (const std::string& file_name) {
    struct stat buffer;
    int isPresent = stat (file_name.c_str(), &buffer);
    if(-1 == isPresent) {
        BTRLEMGRLOG_ERROR("File (%s) Not present.\n", file_name.c_str());
        return false;
    }
    return true;
}

long int get_unix_timestamp()
{
    time_t ut = std::time(0);
    long int uts = static_cast<long int> (ut);
    return uts;
}

long int  generate_nonce()
{
    srand(time(NULL));
    return (rand());
}

#ifdef BTR_LOCAL_OAUTH_SUPPORT
namespace oauth1 {

static bool decryptSecKey(void)
{
    bool ret = true;
    std::string cmd;
    if(isFile_exists(cpg_file)) {
        cmd = cpg_file + " jx ";
        cmd +=key_file;
        cmd +=" /tmp/seckey.txt";
        BTRLEMGRLOG_INFO("Executing command (%s).\n", cmd.c_str());
        system(cmd.c_str());
    }
    else {
        ret = false;
    }
    return ret;
}

static bool readSecandKey(std::string keysec[])
{
    std::string line;
    int index = 0;
    std::string tmp_file = "/tmp/seckey.txt";
    if(!isFile_exists(tmp_file)) {
        return false;
    }
    std::ifstream tmpfile(tmp_file);

    if (tmpfile.is_open())
    {
        while ( (getline (tmpfile,line)) && (index < 2) )
        {
            keysec[index] = line;
            index++;
        }
        tmpfile.close();
        std::remove(tmp_file.c_str());
    }
    else {
        BTRLEMGRLOG_ERROR("Failed to open (%s).\n", tmp_file.c_str());
        return false;
    }
    return true;
}

static bool convert_ascii_encoding(const char *in_str, std::string &out_en_str)
{
    bool ret = false;
    CURL *curl = NULL;

    curl = curl_easy_init();
    if(curl) {
        char *output = curl_easy_escape(curl, in_str, strlen(in_str));
        if(output) {
            out_en_str = output;
            curl_free(output);
            ret = true;
        }

        curl_easy_cleanup(curl);
    }
    return ret;
}


static void construct_BaseString(
    const char *in_url,
    std::string in_oaSigMethod,
    std::string in_ocKey,
    std::string in_ocKeySec,
    long int in_timestamp,
    std::string in_nonce,
    std::string &out_base_str)
{

    std::string en_url;

    BTRLEMGRLOG_DEBUG("URL (Normal) => [%s]\n\n", in_url);

    convert_ascii_encoding(in_url, en_url);
    BTRLEMGRLOG_DEBUG("URL (Encoded) => [%s]\n\n", en_url.c_str());

    std::string oauthConskey = "oauth_consumer_key="+in_ocKey;
    std::string oauthSigMethods = "oauth_signature_method="+in_oaSigMethod;
    std::string oauthTimeStamp = "oauth_timestamp="+std::to_string(in_timestamp);
    std::string oauthNonce = "oauth_nonce="+ in_nonce;
    std::string oauthVersion = "oauth_version=1.0";

    std::string params = oauthConskey + "&";
    params += oauthNonce + "&";
    params += oauthSigMethods + "&";
    params += oauthTimeStamp + "&";
    params += oauthVersion;

    BTRLEMGRLOG_DEBUG("Params (Normal) : [%s]\n\n", params.c_str());

    std::string en_params;

    convert_ascii_encoding(params.c_str(), en_params);

    BTRLEMGRLOG_DEBUG("Params (Encoded) : [%s]\n\n", en_params.c_str());

    std::string http_method = "POST";
    std::string base_string = http_method + "&";
    base_string += en_url + "&";
    base_string += en_params;

    out_base_str = base_string;
    BTRLEMGRLOG_DEBUG("\nEncoded base string => [%s]\n\n", base_string.c_str());
}



static void calculateSignature(
    const char *in_url,
    std::string in_ocKey,
    std::string in_ocKeySec,
    long int timestamp,
    std::string in_nonce,
    std::string& out_signature)
{
    std::string baseStr;
    std::string oaSigMethod = "HMAC-SHA1";
    construct_BaseString(in_url, oaSigMethod, in_ocKey, in_ocKeySec, timestamp, in_nonce, baseStr);
    BTRLEMGRLOG_DEBUG("\n\nThe Base string : [%s]\n\n", baseStr.c_str());

    const char *data = baseStr.c_str();
    std::string secret = in_ocKeySec + "&";
    const char *keySecret = secret.c_str();

    unsigned char result[64];
    unsigned  int len;
    //result = (unsigned char*)malloc(sizeof(char) * len);

    HMAC_CTX ctx;
    HMAC_CTX_init(&ctx);

    HMAC_Init_ex(&ctx, keySecret, strlen(keySecret), EVP_sha1(), NULL);
    HMAC_Update(&ctx, (unsigned char*)data, strlen(data));
    HMAC_Final(&ctx, result, &len);
    HMAC_CTX_cleanup(&ctx);

    /*
    BTRLEMGRLOG_DEBUG("HMAC digest: ");

    for (int i = 0; i< strlen((const char *)result); i++)
        printf("%02x", (unsigned char)result[i]);
    BTRLEMGRLOG_DEBUG("\n");
    */
    char *digest_b64 = encodeBase64((const unsigned char *)result, len);

    BTRLEMGRLOG_DEBUG("Base64: *%s*\n\n", digest_b64);
    out_signature = digest_b64;
    free(digest_b64);
}

bool generateOauthHeaderDetails(
    const char *in_url,
    std::string& out_oauth_consumer,
    std::string& out_oauth_nonce,
    std::string& out_oauth_sig_method,
    std::string& out_oauth_timestamp,
    std::string& out_oauth_version,
    std::string& out_oauth_signature
)
{
    bool ret = false;
    std::string keysec[2];
    std::string baseString;
    std::string signature;
    long int nonce;
    long int timeStamp;

    if(!in_url)
        return false;

    ret = decryptSecKey();

    if(ret) {
        ret = readSecandKey(keysec);
    }

    if(false == ret)
        return ret;

    nonce = generate_nonce();
    timeStamp = get_unix_timestamp();

    out_oauth_consumer = keysec[0];
    out_oauth_nonce = std::to_string(nonce);
    out_oauth_timestamp = std::to_string(timeStamp);
    out_oauth_version = "1.0";
    out_oauth_sig_method = "HMAC-SHA1";

    BTRLEMGRLOG_INFO("out_oauth_consumer is : [%s] \n", out_oauth_consumer.c_str());
    BTRLEMGRLOG_INFO("out_oauth_nonce is : [%s] \n", out_oauth_nonce.c_str());
    BTRLEMGRLOG_INFO("out_oauth_sig_method is : [%s] \n", out_oauth_sig_method.c_str());
    BTRLEMGRLOG_INFO("out_oauth_timestamp is : [%s] \n", out_oauth_timestamp.c_str());
    BTRLEMGRLOG_INFO("out_oauth_version is : [%s] \n", out_oauth_version.c_str());

    calculateSignature(in_url, keysec[0], keysec[1], timeStamp, out_oauth_nonce, signature);
    if(!signature.empty()) {
        std::string en_sig;
        convert_ascii_encoding(signature.c_str(), en_sig);
        out_oauth_signature = en_sig;
        BTRLEMGRLOG_INFO("oauth_signature_value is : [%s] \n", out_oauth_signature.c_str());
        ret = true;
    }
    else
        ret = false;

    return ret;
}

void getOAuthHeaderValues(const char *in_basestring,  const char *in_Oauthkey, std::string& out_value)
{
    if(in_basestring != NULL)
    {
        if(strcmp(in_Oauthkey,"oauth_signature=") == 0)
        {
            char *key_string = (char *)strstr(in_basestring,in_Oauthkey);
            char *value = key_string + strlen(in_Oauthkey);
            int out_val_len = strlen(value);
            char tmp[256] = {'\0'};
            snprintf(tmp, out_val_len, "%s", value);
            out_value = tmp;
        } else
        {
            char *key_string = (char *)strstr(in_basestring,in_Oauthkey);
            char *next_string =  strstr(key_string,"&oauth");
            char key_value[1024]= {'\0'};
            strncpy(key_value,key_string,(strlen(key_string) -  strlen(next_string)));
            char *value = key_value + strlen(in_Oauthkey);
            out_value = value;
        }
    }
}


#if 0 /* Only for test*/
void postTileDataToTileAdoptar(string in_ocKey, string in_ocKeySec, long int timestamp, string in_nonce, string const& signature)
{

    /* Specify the POST data */
    string payload = "{\"device_id\":\"mac:E8:82:5B:68:A2:68\",\"request_id\":\"LE_1681692777\",\"tile_id\":\"a48388a7eeb7de9b\"}";

    string oaSigMethod = "HMAC-SHA1";
    CURL *pCurl = NULL;
    CURLcode res;

    struct curl_slist *headers = NULL;
    curl_global_init(CURL_GLOBAL_ALL);

    pCurl = curl_easy_init();

    if(pCurl)
    {
        curl_easy_setopt(pCurl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(pCurl, CURLOPT_URL, in_url);

        headers = curl_slist_append(headers, "Content-Type: application/json");

        string oAuth_header = "Authorization: OAuth ";

        oAuth_header += "oauth_consumer_key=";
        oAuth_header +=  "\""+in_ocKey+"\",";
        oAuth_header +=  "oauth_nonce=\""+ in_nonce +"\",";
        oAuth_header +=  "oauth_signature_method=\""+ oaSigMethod +"\",";
        oAuth_header +=  "oauth_timestamp=\""+std::to_string(timestamp) +"\",";
        oAuth_header += " oauth_version=\"1.0\",";

        string en_sig;
        convert_ascii_encoding(signature.c_str(), en_sig);
        oAuth_header +=  "oauth_signature=\"" + en_sig +"\"";
        printf("oAuth_Signature : [%s] \n\n", en_sig.c_str());
        printf("oAuth_header : [%s] \n\n", oAuth_header.c_str());

        headers = curl_slist_append(headers, oAuth_header.c_str());

        curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers);

        printf("Json payload is (%s). \n", payload.c_str());

        curl_easy_setopt(pCurl, CURLOPT_POSTFIELDS, payload.c_str());
        res = curl_easy_perform(pCurl);
        if(res != CURLE_OK) {
            printf("Failed  in curl_easy_perform() with error: %s\n", curl_easy_strerror(res));
        }
        else {
            printf("Successful in curl_easy_perform(). \n");
        }
        curl_easy_cleanup(pCurl);
    }
    curl_global_cleanup();
}

int main(void)
{
    string  signature;
    string ocKey;    // Give input for key
    string ocKeySec; // Give input for secret

    long int timestamp = get_unix_timestamp();
    string nonce = "nonce";

    calculateSignature(ocKey, ocKeySec, timestamp, nonce, signature);
    printf("Signature is (%s). \n", signature.c_str());
    postTileDataToTileAdoptar(ocKey, ocKeySec, timestamp, nonce, signature);

}
#endif
}
#endif

bool generateUrlOauthBaseString(const char *in_uri_method, std::string& out_baseString )
{
    bool ret = true;
    std::string cmd;

    if(!in_uri_method)
    {
        BTRLEMGRLOG_ERROR("Failed due to NULL input.\n");
        return false;
    }

    if(isFile_exists(cpg_file)) {
        cmd =cpg_file +" 21 ";
        cmd += in_uri_method;
    }
    else {
        ret = false;
    }

    if(!cmd.empty())
    {
        char buff[1024] = {'\0'};
        std::string output = "";
        FILE *pipe=NULL;

        memset(buff, 0, sizeof(buff));
        pipe = popen(cmd.c_str(),"r");
        if(pipe != NULL)
        {
            while(fgets(buff, 1024, pipe) != NULL)
            {
                out_baseString += buff;
            }
            pclose(pipe);
        }
        else {
            BTRLEMGRLOG_ERROR("[%s]Empty pipe stream.\n", __FUNCTION__);
            ret = false;
        }
    }
    else {
        BTRLEMGRLOG_ERROR("[%s]Failed due to unsupported cpg option.\n", __FUNCTION__);
        ret = false;
    }
    return ret;
}

void getAuthorizationOathHeaderValue(const char *in_basestring,  const char *in_Oauthkey, std::string& out_value)
{
    if(in_basestring != NULL)
    {
        if(strcmp(in_Oauthkey,"oauth_signature=") == 0)
        {
            char *key_string = (char *)strstr(in_basestring,in_Oauthkey);
            char *value = key_string + strlen(in_Oauthkey);
            int out_val_len = strlen(value);
            char tmp[256] = {'\0'};
            snprintf(tmp, out_val_len, "%s", value);
            out_value = tmp;
        } else
        {
            char *key_string = (char *)strstr(in_basestring,in_Oauthkey);
            char *next_string =  strstr(key_string,"&oauth");
            char key_value[1024]= {'\0'};
            strncpy(key_value,key_string,(strlen(key_string) -  strlen(next_string)));
            char *value = key_value + strlen(in_Oauthkey);
            out_value = value;
        }
    }
}

//Calculates the length of a decoded string
static int calcDecodeLength(const char* in_b64String)
{
    int len = strlen(in_b64String),
        padding = 0;

    if (in_b64String[len-1] == '=' && in_b64String[len-2] == '=') //last two chars are =
        padding = 2;
    else if (in_b64String[len-1] == '=') //last char is =
        padding = 1;

    return (len*3)/4 - padding;
}

char *encodeBase64(const unsigned char *input, int length)
{
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);

    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    BTRLEMGRLOG_TRACE("Encoded Data (%s), length is (%llu)\n",bptr->data, (unsigned long long)bptr->length);
    char *buff = (char *)malloc(bptr->length+1);
    memcpy(buff, bptr->data, bptr->length);
    buff[bptr->length] = 0;
//    BIO_set_close(b64, BIO_NOCLOSE);
    BIO_free_all(b64);
    return buff;
}


bool  decodeBase64(const char *in_b64msg,unsigned char **out_decoded,size_t *decodedlength)
{
    BIO *bio, *b64;
    bool ret = false;

    BTRLEMGRLOG_TRACE("Entering...\n");
    size_t decodeLen = calcDecodeLength(in_b64msg);

    BTRLEMGRLOG_DEBUG("Calculated Decode Length is [%llu]\n",(unsigned long long)decodeLen);

    *out_decoded = (unsigned char*)malloc(decodeLen + 1);

    if( *out_decoded != NULL )
    {
        (*out_decoded)[decodeLen] = '\0';
        bio = BIO_new_mem_buf(in_b64msg, -1);
        b64 = BIO_new(BIO_f_base64());
        bio = BIO_push(b64, bio);

        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); //Do not use newlines to flush buffer
        *decodedlength = BIO_read(bio, *out_decoded, strlen(in_b64msg));

        BTRLEMGRLOG_DEBUG("Read Decoded Length is [%llu].\n",(unsigned long long)*decodedlength);

        if(*decodedlength == decodeLen) //length should equal decodeLen, else something went horribly wrong
        {
            BTRLEMGRLOG_DEBUG("The Length should equal to decoded length.\n");
            ret = true;
        }
        else {
            BTRLEMGRLOG_WARN("The calculated and read decoded lengths should be equal, else something went horribly wrong.\n");
        }
        BIO_free_all(bio);
    }
    else
    {
        BTRLEMGRLOG_ERROR("Malloc Failed: Can't allocate memory.\n");
    }
    BTRLEMGRLOG_TRACE("Exiting...\n");

    return ret;
}

void  hexStringToByteArray(const char *hexstring, unsigned char rand_t_byte_array[], int* byte_arr_len)
{
    BTRLEMGRLOG_TRACE("Entering\n");
    const char *pos = hexstring;
    *byte_arr_len = strlen(hexstring)/2;
    while( *pos )
    {
        if( !((pos-hexstring)&1) )
            sscanf(pos,"%02x",(unsigned int*)&rand_t_byte_array[(pos-hexstring)>>1]);
        ++pos;
    }

//     for(int i =0; i < bytearraylength; i ++)
//     {
//         BTRLEMGRLOG_TRACE("rand_t_byte_array[%d] : 0x%02x\n",i, rand_t_byte_array[i]);
//     }
    BTRLEMGRLOG_TRACE("Exiting...\n");
}

bool checkRFC_ServiceAvailability ( std::string rfc_param, std::string& param_value)
{
    RFC_ParamData_t param = {0};
    const char* paramName = (const char* )rfc_param.c_str();
    WDMP_STATUS status = getRFCParameter((char *)"BTRLEMGR", paramName, &param);

    if (status == WDMP_SUCCESS) {
        BTRLEMGRLOG_DEBUG ("Successfully retrived the RFC Parameter information. (Name = %s, type = %d, value = %s)\n", param.name, param.type, param.value);
        param_value = param.value;
        return true;
    }
    else {
        BTRLEMGRLOG_ERROR ("Failed to retrive the RFC Parameter \"%s\" with status as (%s).\n", paramName, getRFCErrorString(status));
        return false;
    }
}

}
