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

#ifndef _BTR_LE_MGR_UTILS_H_
#define _BTR_LE_MGR_UTILS_H_

#include <string>

/*
 * Configuration parameters,
 * Later can be moved to xml/json file
 */
const std::string cpg_file = "/usr/bin/configparamgen";
const std::string key_file = "/etc/nauzhxhue.isz";
const std::string codebig_hostname = "https://tile-adapter-prod.codebig2.net";
const std::string tilealert_uri = "/api/v2/bte/device/tilealert";
const std::string tilebatchupdate_uri = "/api/v2/bte/device/tilebatchupdate";
const std::string tileRingStatus_uri = "/api/v2/bte/device/tilestatus";

namespace  btrLeMgrUtils {
bool notifyLeDeviceToCloud(char* in_apDevice_Id);
char* getSTBMac();
void setSTBMac(char* mac);
char* getRfcUrl();
void setRfcUrl(char *url);
char* getRfcPrefUrl();
void terminateBeaconDetection(void);
int updateLimitBeaconDetection();
int setCustomerPreference();
bool isFile_exists (const std::string& file_name);
long int  generate_nonce();
long int get_unix_timestamp();

namespace oauth1 {
bool generateOauthHeaderDetails(
    const char *in_url,
    std::string& out_oauth_consumer,
    std::string& out_oauth_nonce,
    std::string& out_oauth_sig_method,
    std::string& out_oauth_timestamp,
    std::string& out_oauth_version,
    std::string& out_oauth_signature);
}

bool generateUrlOauthBaseString(const char *in_uri_method, std::string& out_baseString);
void getAuthorizationOathHeaderValue(const char *in_basestring,  const char *in_Oauthkey, std::string& out_value);
char *encodeBase64(const unsigned char *input, int length);
bool decodeBase64(const char *in_b64msg,unsigned char **out_decoded,size_t *decodedlength);
void hexStringToByteArray(const char *in_hexstring, unsigned char out_rand_t_byte_array[], int* out_byte_arr_len);
bool checkRFC_ServiceAvailability ( std::string in_rfc_param, std::string& out_param_value);
}
#endif /* _BTR_LE_MGR_UTILS_H_ */
