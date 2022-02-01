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

#include <unistd.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <uuid/uuid.h>

#include "btrTileMgr.h"
#include "btrLeMgr_logger.h"
#include "btrLeMgr_utils.h"

#include "hostIf_tr69ReqHandler.h"
#include "libIBus.h"


#define MAX_LE_TILE_NOTIFY_INTERVAL     15*60  // 15 min
#define MAX_LE_TILE_RECONNECT_INTERVAL  30*60  // 30 min


/* Tile Specific TOA msg feilds */
# define _1_BYTE_TOA_CID                 "00"
# define _4_BYTE_TOA_SESSION_TOKEN       "00000000"
# define _14_BYTE_TOA_RAND_A             "0000000000000000000000000000"
# define _14_BYTE_TOA_MSG_PAYLOAD        "0000000000000000000000000000"
# define _14_BYTE_TOA_SONG_PAYLOAD       "020103"
# define _4_BYTE_TOA_MIC                 "00000000"
# define _1_BYTE_TOA_CMD_OPEN_CHANNEL    "10"
# define _1_BYTE_TOA_CMD_READY           "12"
# define _1_BYTE_TOA_CMD_SONG_PLAY       "05"
# define _1_BYTE_TOA_RSP_OPEN_CHANNEL    "12"
# define _1_BYTE_TOA_RSP_READY           "01"

#define PROD_TILE_RING_STATUS_URL "https://tile-adapter-prod.codebig2.net/api/v2/bte/device/tilestatus"
#define POC_TILE_RING_STATUS_URL "http://tile-api.xs.comcast.net:8081/api/v2/bte/device/tilestatus"
#define TILE_BATCHUPADTE_URL "https://tile-adapter-prod.codebig2.net/api/v2/bte/device/tilebatchupdate"

const std::string notifyUuid    = "9d410019-35d6-f4dd-ba60-e7bd8dc491c0";
const std::string writeCharUuid = "9d410018-35d6-f4dd-ba60-e7bd8dc491c0";

BtrTileMgr* BtrTileMgr::instance = NULL;
bool BtrTileMgr::m_isTileRingFeatureEnabled = false;
static stRingAttributes empty = {0};
#define MAX_URL_SIZE 100
static char gStagingDiscoveryURL[MAX_URL_SIZE] = {'\0'};
static char gStagingRingURL[MAX_URL_SIZE] = {'\0'};
#if 0

// Comment: BtrLeDevProp can be renamed as BtrMgrIfDevProp and be exposed by BtrMgrIf
BTRLEMGR_Result_t BtrTileMgr::getLeProperty(ui_long_long devId, BtrLeDevProp &leProp) {
    return (LEMGR_RESULT_SUCCESS);
}

BTRLEMGR_Result_t BtrTileMgr::getLeDevList() {
    return (LEMGR_RESULT_SUCCESS);
}

#endif

// Comment: Should be part of BtrLeMgrBase which would call derived classes overloaded functions as required
void
BtrTileMgr::receiver_LeEventQ (
    void
) {
    sleep(1);

    BTRLEMGRLOG_TRACE("Entering...\n");

    /* First device discovers, the check below steps in Hash,
     * if (not present), then connect and notify.
     *     If notified, then add in hash list with updated time-stamp.
     *     If not notified then don't add to hash list
     * else if (present), then check with last notified time
     *    if rediscovered time is less then throttle time, then don't connect and notify
     *    else if rediscovered time is more then throttle time, then connect and notify
     * finish
     **/
    while (m_bQuitThread == false) {
        auto lePropObj = m_pBtrMgrIfce->popEvent ();
        ui_long_long devId = lePropObj.m_devId;

        switch (lePropObj.m_eventType) {
        case LEMGR_EVENT_DEVICE_DISCOVERY_STARTED:
            clearDiscoveryList();
            break;
        case LEMGR_EVENT_DEVICE_DISCOVERY_UPDATE:
        {
            if (false == isPresentInDiscoveryList(devId)) { // Is a device we have not added to Discovery List
                stBtLeDevList *pstLeDevList     = new stBtLeDevList;
                pstLeDevList->m_eventType       = lePropObj.m_eventType;
                pstLeDevList->m_devId           = lePropObj.m_devId;
                pstLeDevList->m_macAddr         = lePropObj.m_macAddr;
                pstLeDevList->m_discoveryCount  = lePropObj.m_discoveryCount;
                pstLeDevList->m_name            = lePropObj.m_name;
                pstLeDevList->m_serviceUuid     = lePropObj.m_serviceUuid;
                pstLeDevList->m_signalLevel     = lePropObj.m_signalLevel;
                pstLeDevList->m_isServDataPresent = lePropObj.m_isServDataPresent;

                if(pstLeDevList->m_isServDataPresent)
                    pstLeDevList->m_advSerData      = lePropObj.m_adServiceData;

                addToDiscoveryList(pstLeDevList); // add device to the list of devices
            }


            m_last_discovery_timestamp = std::chrono::system_clock::now();

            if (false == is_LeDevPresentInMap(devId))  {

                /* Check Service Data for new or old Tile, if not, then support the old Tile, otherwise send new PDU for new Tile*/
                bool isSrvData = lePropObj.m_isServDataPresent;

                if(false == isSrvData) {
                    BTRLEMGRLOG_INFO("[%d]This Tile ( devId: %llu), (mac: %s) doesn't have ServiceData.\n",  __LINE__, devId, lePropObj.m_macAddr.c_str());
                    doConnect(devId);
                }
                else {
                    /* For new Tile, no Tile Id retrieval, so first arg Tileid is empty*/
                    std::string hashTileId = lePropObj.m_adServiceData.substr(4); /* Last 8 Hex string */
                    enBtrNotifyState notifyState = notifyTileInfoCloud(
                                                       hashTileId,
                                                       "",
                                                       LE_TILE_NOTIFY_TYPE_DISCOVERY_ALERT,
                                                       LE_NOTIFY_MSG_CLOUD_POST_STATUS_OK,
                                                       lePropObj.m_macAddr.c_str(),
                                                       lePropObj.m_adServiceData.c_str(),
                                                       lePropObj.m_signalLevel);

                    if (LE_NOTIFY_SEND_SUCCESSFULLY == notifyState) {

                        add_LeDevPropToMap(devId, lePropObj.m_macAddr.c_str(), lePropObj.m_name.c_str(), "", lePropObj.m_adServiceData,hashTileId.c_str());
                        updateTimestamp_LeDevMap(devId, notifyState, false);
                    }
                    BTRLEMGRLOG_INFO("[%d]%s Send Cloud Notification for Tile Device [(%llu), (%s)]. \n", __LINE__,
                                     (char *)(notifyState == LE_NOTIFY_SEND_SUCCESSFULLY ? "Successfully" : "Failed to "), devId, lePropObj.m_macAddr.c_str());

                }
            }
            else {
                bool isSrvData = lePropObj.m_isServDataPresent;

                /* Change as per new Tile property, if Service Data present and changing for same mac address, then notify. */

                if(isSrvData) {
                    BTRLEMGRLOG_INFO("[%d]This Tile ( devId: %llu)(m_LeDevMap[devId]->m_devId: %llu), (mac: %s) lePropObj.m_adServiceData = %s, m_LeDevMap[devId]->m_sericeData = %s \n",
                                    __LINE__, devId, m_LeDevMap[devId]->m_devId, lePropObj.m_macAddr.c_str(), lePropObj.m_adServiceData.c_str(), m_LeDevMap[devId]->m_sericeData.c_str());
                    if((devId == m_LeDevMap[devId]->m_devId) && (lePropObj.m_adServiceData.compare(m_LeDevMap[devId]->m_sericeData)))
                    {
                        std::string hashTileId = lePropObj.m_adServiceData.substr(4); /* Last 8 Hex string */
                        enBtrNotifyState notifyState = notifyTileInfoCloud(
                                                           hashTileId,
                                                           "",
                                                           LE_TILE_NOTIFY_TYPE_DISCOVERY_ALERT,
                                                           LE_NOTIFY_MSG_CLOUD_POST_STATUS_OK,
                                                           lePropObj.m_macAddr.c_str(),
                                                           lePropObj.m_adServiceData.c_str(),
                                                           lePropObj.m_signalLevel);
                        updateTimestamp_LeDevMap(devId, notifyState, false);
                        updateTileServiceData_LeDevMap(devId, lePropObj.m_adServiceData);
                        BTRLEMGRLOG_INFO("%s Send Cloud Notification for Tile Device [(%llu), (%s)]. for same MAC and updated/rotating ServiceData \n",
                                         (char *)((notifyState == LE_NOTIFY_SEND_SUCCESSFULLY) ? "Successfully" : "Failed to "), devId, lePropObj.m_macAddr.c_str());
                    }
                }
                else {
                    if (true == check_LeDevThrottleTimeInMap(devId)) {
                        enBtrNotifyState notifyState = LE_NOTIFY_NONE;
                        std::string tileId = retrieve_TileId_from_DevId(devId);

                        if (!tileId.empty())
                            notifyState = notifyTileInfoCloud(tileId, "", LE_TILE_NOTIFY_TYPE_DISCOVERY_ALERT, LE_NOTIFY_MSG_CLOUD_POST_STATUS_OK);

                        // add/update info on this tile in the map of tiles
                        updateTimestamp_LeDevMap(devId, notifyState, false);
                        BTRLEMGRLOG_INFO("%s Send Cloud Notification for Tile Device [(%llu), (%s)]. \n",
                                (char *)((notifyState == LE_NOTIFY_SEND_SUCCESSFULLY) ? "Successfully" : "Failed to "), devId, lePropObj.m_macAddr.c_str());
                        BTRLEMGRLOG_INFO("Tile (device id %llu): %s sending notification to cloud, mac (%s)\n",
                                devId, notifyState == LE_NOTIFY_SEND_SUCCESSFULLY ? "Success" : "Failure", lePropObj.m_macAddr.c_str());
                    }
                    else {
                        std::unique_lock <std::mutex> lock(m_mtxMap);
                        m_LeDevMap[devId]->m_lastSeenTimestamp = time(nullptr);

                    }
                }

            }
        }
        break;
        case LEMGR_EVENT_DEVICE_DISCOVERY_COMPLETE:
            break;
        case LEMGR_EVENT_DEVICE_CONNECTION_COMPLETE:
            if (true == isPresentInDiscoveryList(devId)) { // is a device we requested to connect to, so safe to react
                m_last_discovery_timestamp = std::chrono::system_clock::now();
                std::unique_lock <std::mutex> lock(m_request_mutex);
                BTRLEMGRLOG_DEBUG("Ring state of Tile, for dev_id: (%llu) and isRingRequested (%d). \n", m_stRingAttributes.devId, m_stRingAttributes.isRingRequested);

                if(m_isTileRingFeatureEnabled && (m_stRingAttributes.devId == devId && m_stRingAttributes.isRingRequested == true)) {
                    m_stRingAttributes.isConnected = true;
                    time_t current_ts = time(nullptr);
                    if((current_ts - m_stRingAttributes.ring_trigger_ts) <= 30) {
                        BTRLEMGRLOG_INFO("Now ready to write MEP_TOA_OPEN_CHANNEL \n");
                        /* Write to Open */
                        if(m_stRingAttributes.oper_code.compare("MEP_TOA_OPEN_CHANNEL") == 0) {
                            BTRLEMGRLOG_INFO("Ring state of Tile, for dev_id: (%llu) and isRingRequested (%d). \n", m_stRingAttributes.devId, m_stRingAttributes.isRingRequested);
                            lock.unlock();
                            BTRLEMGR_Result_t result = write_TOA_CMD_OPEN_CHANNEL(devId);
                            lock.lock();
                            if((devId == m_stRingAttributes.devId) && (LE_TILE_RING_REQUEST_TO_CONNECT == m_stRingAttributes.ring_state)) {
                                if(LEMGR_RESULT_SUCCESS == result) {
                                    m_stRingAttributes.ring_state = LE_TILE_RING_WRITE_MEP_TOA_OPENNED;
                                }
                                else {
                                    BTRLEMGRLOG_ERROR("Failed to MEP_TOA_OPEN_CHANNEL for Ring Request.\n");
                                    m_stRingAttributes.ring_state = LE_TILE_RING_REQUEST_FAILED;
                                }
                            }
                            else {
                                BTRLEMGRLOG_WARN("Ring connection notification ignored as the request is no longer current.\n");
                                //TODO: Should we disconnect from this device if devId is different now?
                            }
                        }
                    }
                    else {
                        BTRLEMGRLOG_INFO("Failed to Ring with in 30 sec.\n");
                        m_stRingAttributes =empty;
                    }
                }
                else {
                    lock.unlock();
                    /* Check for Non Service data Tile, if so, then, do fetch Tile id on connect*/
                    if(false == m_LeDevList[devId]->m_isServDataPresent) {
                        sleep(3); /* Added delay to wait for GATT profile to populate*/
                        std::string tileId;
                        BTRLEMGR_Result_t op_result = getTileIdCore (devId, tileId); // main operation

                        // always disconnect after LE operations are done, irrespective of whether getting the tile id was a success
                        doDisconnect(devId);

                        // post LE-operation processing
                        if (op_result == LEMGR_RESULT_SUCCESS) {
                            enBtrNotifyState notifyState = notifyTileInfoCloud(tileId, "", LE_TILE_NOTIFY_TYPE_DISCOVERY_ALERT, LE_NOTIFY_MSG_CLOUD_POST_STATUS_OK);
                            std::lock_guard <std::mutex> lock(m_mtxMap);
                            // add/update info on this tile in the map of tiles
                            if (false == is_LeDevPresentInMap(devId)) {
                                // TODO: 2nd call to is_LeDevPresentInMap prints INFO log again (1st call on discovery update)
                                // use "m_LeDevMap.find(devId) == m_LeDevMap.end()" instead?
                                if (LE_NOTIFY_SEND_SUCCESSFULLY == notifyState) {
                                    add_LeDevPropToMap(devId, lePropObj.m_macAddr.c_str(), lePropObj.m_name.c_str(), tileId.c_str());
                                    updateTimestamp_LeDevMap(devId, notifyState, true);
                                    BTRLEMGRLOG_INFO("Successfully Send Cloud Notification for Tile Device [(%llu), (%s)]. \n",
                                                     devId, lePropObj.m_macAddr.c_str());
                                }
                            }
                            else {
                                updateTimestamp_LeDevMap(devId, notifyState, true);
                                BTRLEMGRLOG_INFO("%s Send Cloud Notification for Tile Device [(%llu), (%s)]. \n",
                                                 (char *)((notifyState == LE_NOTIFY_SEND_SUCCESSFULLY) ? "Successfully":"Failed to "), devId, lePropObj.m_macAddr.c_str());
                            }
                            BTRLEMGRLOG_INFO("Tile (device id %llu): %s sending notification to cloud, mac (%s)\n",
                                             devId, notifyState == LE_NOTIFY_SEND_SUCCESSFULLY ? "Success" : "Failure", lePropObj.m_macAddr.c_str());
                        }
                    }
                }
            }
            break;
        case LEMGR_EVENT_DEVICE_OP_READY:
            m_last_discovery_timestamp = std::chrono::system_clock::now();
            break;
        case LEMGR_EVENT_DEVICE_CONNECTION_FAILED:
            m_last_discovery_timestamp = std::chrono::system_clock::now();
            break;
        case LEMGR_EVENT_DEVICE_DISCONNECT_COMPLETE:
        {
            m_last_discovery_timestamp = std::chrono::system_clock::now();
            std::lock_guard <std::mutex> lock(m_request_mutex);
            if(m_stRingAttributes.isRingRequested && (devId == m_stRingAttributes.devId)) {
                m_stRingAttributes.isConnected = false;
                m_stRingAttributes = empty;
                BTRLEMGRLOG_WARN("The requested Ring command for Tile uuid (%s) & Dev_Id(%llu) is Disconnected.\n", m_stRingAttributes.tile_uuid.c_str(), m_stRingAttributes.devId);
            }
        }
        break;
        case LEMGR_EVENT_DEVICE_DISCONNECT_FAILED:
            m_last_discovery_timestamp = std::chrono::system_clock::now();
            break;
        case LEMGR_EVENT_DEVICE_NOTIFICATION:
            if (true == isPresentInDiscoveryList(devId)) { // is a device we requested to connect to, so safe to react
                m_last_discovery_timestamp = std::chrono::system_clock::now();
                BTRLEMGRLOG_INFO("Received Notify Data [%s] for the Tile Dev Id [%llu] & name [%s]. \n", lePropObj.m_notifyData.c_str(), devId, lePropObj.m_name.c_str());
                std::unique_lock <std::mutex> lock(m_request_mutex);
                BTRLEMGRLOG_INFO("Ring state of Tile, for dev_id: [%llu] and isRingRequested [%d] and Ring State [%d]. \n",
                                 m_stRingAttributes.devId, m_stRingAttributes.isRingRequested, m_stRingAttributes.ring_state);

                /*Check for Tile got the rand_t
                 * If yes, then notify to cloud.*/
                if((true == m_stRingAttributes.isRingRequested) &&
                        (!lePropObj.m_notifyData.empty()) && (m_stRingAttributes.devId == devId)) {

                    switch (m_stRingAttributes.ring_state) {
                    case LE_TILE_RING_WRITE_MEP_TOA_OPENNED:
                    {
                        std::string rsp_Rand_A = lePropObj.m_notifyData;
                        BTRLEMGRLOG_INFO("Received MEP_TOA_OPENNED Notify Data [%s]\n", rsp_Rand_A.c_str());
                        send_Rand_T_Notification(devId, rsp_Rand_A);
                    }
                    break;
                    case LE_TILE_RING_WRITE_MEP_TOA_COMMANDS_READY:
                    {
                        std::string rsp_cmd_ready = lePropObj.m_notifyData;
                        BTRLEMGRLOG_INFO("Received MEP_TOA_COMMANDS_READY Notify Data [%s]\n", rsp_cmd_ready.c_str());

                        m_stRingAttributes.cmd_Ready.rsp_notify_data = lePropObj.m_notifyData;
                        m_stRingAttributes.ring_state = LE_TILE_RING_NOTIFY_MEP_TOA_COMMANDS_READY_RESPONSE;

                        lock.unlock();
                        BTRLEMGR_Result_t result = write_TOA_CMD_PLAY(devId);
                        lock.lock();
                        if(LEMGR_RESULT_SUCCESS != result) {
                            if((devId == m_stRingAttributes.devId) && (LE_TILE_RING_NOTIFY_MEP_TOA_COMMANDS_READY_RESPONSE == m_stRingAttributes.ring_state)) {
                                m_stRingAttributes =empty;
                            }
                            else {
                                BTRLEMGRLOG_WARN("Ring COMMANDS_READY follow-up ignored as the request is no longer current.\n");
                            }
                        }
                    }
                    break;
                    case LE_TILE_RING_WRITE_MEP_TOA_COMMANDS_PLAY:
                    {
                        std::string rsp_cmd_play = lePropObj.m_notifyData;
                        BTRLEMGRLOG_INFO("Received MEP_TOA_COMMANDS_PLAY Notify Data [%s]\n", rsp_cmd_play.c_str());
                        m_stRingAttributes.cmd_Play.rsp_notify_data = rsp_cmd_play;
                        m_stRingAttributes.ring_state = LE_TILE_RING_NOTIFY_MEP_TOA_COMMANDS_PLAY_RESPONSE;

                        send_TOA_CMD_Response_Notification(devId);
                        if(m_stRingAttributes.disconnect_on_completion) {
                            lock.unlock();
                            doDisconnect(devId);
                            lock.lock();
                            if((devId != m_stRingAttributes.devId) || (LE_TILE_RING_NOTIFY_MEP_TOA_COMMANDS_PLAY_RESPONSE != m_stRingAttributes.ring_state)) {
                                BTRLEMGRLOG_WARN("Ring COMMANDS_PLAY follow-up ignored as the request is no longer current.\n");
                                break;
                            }

                        }
                        m_stRingAttributes=empty;
                    }
                    break;
                    default:
                        break;
                    }
                }
                lePropObj.m_notifyData.clear();
            }
            break;
        case LEMGR_EVENT_DEVICE_OUT_OF_RANGE:
            deleteFromDiscoveryList(devId);
            delete_LeDevMap(devId);
            break;
        default:
            break;
        }
    }

    BTRLEMGRLOG_INFO("Exiting...\n");
}

// Comment: Why do we expect the LeAppMgr to even call this. Cant we create an event listener as part of the Base classes constructor??
std::thread
BtrTileMgr::eventListenerThread (
    void
) {
    BTRLEMGRLOG_INFO("Starting Tile Event Listener Thread\n");
    return (std::thread([=] {receiver_LeEventQ();}));
}

void
BtrTileMgr::eventListenerQuit (
    bool    abQuitThread
) {
    m_bQuitThread = abQuitThread;
    m_pBtrMgrIfce->shutdownIfce();
}

BTRLEMGR_Result_t
BtrTileMgr::getTileId (
    ui_long_long devId,
    std::string& tileId
) {
    BTRLEMGR_Result_t ret = LEMGR_RESULT_SUCCESS;
    BTRLEMGRLOG_TRACE("Entering for dev %llu\n", devId);

    ret = doConnect(devId);
    if (ret == LEMGR_RESULT_SUCCESS) {
        sleep(3); // Listening on the Connection Complete Event will be more appropriate
        ret = getTileIdCore(devId, tileId);
        doDisconnect(devId);
    }

    return (ret);
}

BTRLEMGR_Result_t
BtrTileMgr::getTileIdCore (
    ui_long_long devId,
    std::string& tileId
) {
    std::string charUuid;

    if (false == m_pBtrMgrIfce->getLeCharProperty (devId, charUuid))
        return LEMGR_RESULT_LE_CHAR_UUID_FAIL;
    else if (false == m_pBtrMgrIfce->performLeOps (devId, LEMGR_OP_READ_VALUE, charUuid, tileId))
        return LEMGR_RESULT_LE_METHOD_OPERATION_FAIL;
    else
        return LEMGR_RESULT_SUCCESS;
}

enBtrNotifyState
BtrTileMgr::notifyTileInfoCloud (
    std::string const &tileId,
    std::string const &sessionId,
    enBtrLeTileNotifyType eLeNotifyMsgType,
    enBtrNotifyState eCloudMsgStatus,
    std::string mac,
    std::string srvData,
    int rssi)
{
    const char* pMac              = NULL;
    const char* pi8CloudURL       = NULL;
    enBtrNotifyState ret          = LE_NOTIFY_SEND_FAILED;
    enBtrLeTileNotifyType eMsgType = LE_TILE_NOTIFY_TYPE_NONE;

    const char *tile_Id = NULL;

    BTRLEMGRLOG_TRACE("Entering..\n");

    tile_Id = tileId.c_str();
    if(!tile_Id) {
        BTRLEMGRLOG_ERROR("Failed, due to NULL Tile ID");
        return (ret);
    }

    pMac = btrLeMgrUtils::getSTBMac();

    if(!pMac) {
        BTRLEMGRLOG_ERROR("Failed to get STB Mac.");
        return (LE_NOTIFY_SEND_FAILED);
    }

    eMsgType = eLeNotifyMsgType;

    /* Specify the POST data */
    //char data[500] = {'\0'};
    bool notify_enable = true;
    std::string uri;
    bool isCodebigSupported = false;
    std::string codebigUrl = codebig_hostname;
    cJSON *root = NULL;
    char* jsonPkt = NULL;

    BTRLEMGRLOG_DEBUG("\t Tile Id 		: \t[%s]\n", tileId.c_str());
    BTRLEMGRLOG_DEBUG("\t Tile Mac 		: \t[%s]\n", mac.c_str());
    BTRLEMGRLOG_DEBUG("\t Tile srvData 	: \t[%s]\n", srvData.length()?srvData.c_str():"Not Present.");

    switch(eMsgType) {
    case LE_TILE_NOTIFY_TYPE_DISCOVERY_ALERT:
    {
        pi8CloudURL    = btrLeMgrUtils::getRfcUrl();

        if(!pi8CloudURL) {
            BTRLEMGRLOG_ERROR("Failed, RFC LE Notification server URL.");
            return (LE_NOTIFY_SEND_FAILED);
        }

        std::string urlStr;
        int isStagingEnv = access( "/tmp/.stagingTileUrl", F_OK );

        if(srvData.length() && (0 == isStagingEnv)) {
            if(LEMGR_RESULT_SUCCESS == populateStagingDiscoveryRingURL()){
                pi8CloudURL = gStagingDiscoveryURL;
            }
            /*staging URL(discovery & Ring) to be present in "/tmp/.stagingTileUrl". else return error*/
            if((gStagingDiscoveryURL[0] == '\0') || (gStagingRingURL[0] == '\0') ){
                BTRLEMGRLOG_ERROR("Staging Discovery/Ring notify URL absent in /tmp/.stagingTileUrl");
                return (LE_NOTIFY_SEND_FAILED);
            }

            urlStr = pi8CloudURL;
        }
        else {
            if (srvData.length()) {
                pi8CloudURL = TILE_BATCHUPADTE_URL;
                urlStr = pi8CloudURL;
            } else {
                urlStr = pi8CloudURL;
            }
        }

        BTRLEMGRLOG_DEBUG("\t The Endpoint URL is 	: \t[%s]\n", pi8CloudURL);

        std::size_t found = urlStr.find(codebigUrl);

        if (found!=std::string::npos) {
            isCodebigSupported = true;
        }
        if(srvData.length() && (0 != isStagingEnv)) {
           uri = tilebatchupdate_uri;
        } else {
           uri = tilealert_uri;
        }
        BTRLEMGRLOG_DEBUG("\t URI is : %s \n",uri.c_str());
        root = cJSON_CreateObject();
        if(root) {
            uuid_t id;
            char idbuff[64];
            uuid_generate_random(id);
            memset(idbuff, 0, sizeof(idbuff));
            uuid_unparse(id, idbuff);

            std::string device_id_val = "mac:";
            device_id_val.append(pMac);
            cJSON_AddStringToObject(root, "device_id",device_id_val.c_str());
            std::string req_id_val = std::string(idbuff);
            cJSON_AddStringToObject(root, "request_id",req_id_val.c_str());

            if(mac.length() && srvData.length())
            {

                std::string payload;
                constructAdvDataPayload_HashTileId(mac,srvData, payload);

                cJSON_AddStringToObject(root, "discovery_timestamp", std::to_string(time(nullptr)).c_str());
                cJSON_AddStringToObject(root, "payload",payload.c_str());
                cJSON_AddNumberToObject(root, "rssi",rssi);
            }
            else
            {
                cJSON_AddStringToObject(root, "tile_id",tile_Id);
            }
            jsonPkt = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
        }
    }
    break;
    case LE_TILE_NOTIFY_TYPE_RING_REQUEST:
        pi8CloudURL    = POC_TILE_RING_STATUS_URL;
        root = cJSON_CreateObject();
        if(root) {
            std::string device_id_val = "mac:";
            device_id_val.append(pMac);
            cJSON_AddStringToObject(root, "device_id",device_id_val.c_str());
            cJSON_AddStringToObject(root, "request_id",sessionId.c_str());
            cJSON_AddStringToObject(root, "tile_id",tile_Id);
            cJSON_AddStringToObject(root, "status", std::to_string(eCloudMsgStatus).c_str());

            jsonPkt = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
        }
        uri = tileRingStatus_uri;
        break;
    case LE_TILE_NOTIFY_TYPE_RING_RAND_T_RESPONSE:
        {
            int isStagingEnv = access( "/tmp/.stagingTileUrl", F_OK );
            BTRLEMGRLOG_INFO("m_stRingAttributes.current_HashID.length() = %llu, isStagingEnv = %d\n",
                    (unsigned long long)m_stRingAttributes.current_HashID.length(),isStagingEnv);
            if(m_stRingAttributes.current_HashID.length() && (0 == isStagingEnv)) {
                pi8CloudURL = gStagingRingURL;
                if(gStagingRingURL[0] == '\0') {
                    BTRLEMGRLOG_ERROR("Staging Ring notify URL absent in /tmp/.stagingTileUrl");
                }
            }
            else {
                pi8CloudURL = PROD_TILE_RING_STATUS_URL;
            }

            if(m_stRingAttributes.isRingRequested == true && LE_TILE_RING_RECEIVED_RAND_T == m_stRingAttributes.ring_state) {
                root = cJSON_CreateObject();
                if(root) {
                    std::string device_id_val = "mac:";
                    device_id_val.append(pMac);
                    cJSON_AddStringToObject(root, "device_id",device_id_val.c_str());
                cJSON_AddStringToObject(root, "tile_uuid",m_stRingAttributes.tile_uuid.c_str());
                cJSON_AddStringToObject(root, "rand_t", m_stRingAttributes.rand_T.c_str());
                cJSON_AddStringToObject(root, "session_token",m_stRingAttributes.session_token.c_str());
                cJSON_AddStringToObject(root, "code", "MEP_TOA_CHANNEL_OPENED");
                cJSON_AddStringToObject(root, "channel_id", m_stRingAttributes.channel_id.c_str());
                cJSON_AddStringToObject(root, "cmst_code", std::to_string(eCloudMsgStatus).c_str());
                m_stRingAttributes.cmst_cpe_to_ctm_ts = time(nullptr);
                cJSON_AddStringToObject(root, "cmst_cpe_to_ctm_ts", std::to_string(m_stRingAttributes.cmst_cpe_to_ctm_ts).c_str());
                cJSON_AddStringToObject(root, "cmst_ctm_from_cpe_ts",  std::to_string(m_stRingAttributes.cmst_ctm_from_cpe_ts).c_str());
                cJSON_AddStringToObject(root, "hashed_id", m_stRingAttributes.current_HashID.c_str());
                cJSON_AddStringToObject(root, "cmst_traceId", m_stRingAttributes.cmst_traceId.c_str());

                jsonPkt = cJSON_PrintUnformatted(root);
                cJSON_Delete(root);
            }

            std::string urlStr = pi8CloudURL;
            std::size_t found = urlStr.find(codebigUrl);

            if (found!=std::string::npos) {
                isCodebigSupported = true;
                uri = "/api/v2/bte/device/tilestatus";
            }

            BTRLEMGRLOG_INFO("m_stRingAttributes.current_HashID.length() = %llu, isStagingEnv = %d\n",
                    (unsigned long long)m_stRingAttributes.current_HashID.length(),isStagingEnv);
            if(m_stRingAttributes.current_HashID.length() && (0 == isStagingEnv)) {
                notify_enable = true;
            }
            else {
                do_webpa_notify (jsonPkt);
                notify_enable = false;
            }


        }
    }
    break;
    case LE_TILE_NOTIFY_TYPE_RING_CMD_RESPONSE:
    {
        int isStagingEnv = access( "/tmp/.stagingTileUrl", F_OK );
        BTRLEMGRLOG_INFO("m_stRingAttributes.current_HashID.length() = %llu, isStagingEnv = %d\n",
                (unsigned long long)m_stRingAttributes.current_HashID.length(),isStagingEnv);
        if(m_stRingAttributes.current_HashID.length() && (0 == isStagingEnv)) {
            pi8CloudURL = gStagingRingURL;
            if(gStagingRingURL[0] == '\0') {
                BTRLEMGRLOG_ERROR("Staging Ring notify URL absent in /tmp/.stagingTileUrl");
            }
        }
        else {
            pi8CloudURL = PROD_TILE_RING_STATUS_URL;
        }

        if(m_stRingAttributes.isRingRequested == true
                && !m_stRingAttributes.cmd_Ready.rsp_notify_data.empty()
                && !m_stRingAttributes.cmd_Play.rsp_notify_data.empty()
          ) {
            root = cJSON_CreateObject();
            if(root) {
                cJSON_AddStringToObject(root, "tile_uuid",m_stRingAttributes.tile_uuid.c_str());
                cJSON_AddStringToObject(root, "code", "MEP_TOA_COMMANDS_SENT");
                m_stRingAttributes.cmst_cpe_to_ctm_ts = time(nullptr);
                cJSON_AddStringToObject(root, "cmst_cpe_to_ctm_ts", std::to_string(m_stRingAttributes.cmst_cpe_to_ctm_ts).c_str());
                cJSON_AddStringToObject(root, "cmst_ctm_from_cpe_ts",  std::to_string(m_stRingAttributes.cmst_ctm_from_cpe_ts).c_str());
                cJSON_AddStringToObject(root, "cmst_traceId", m_stRingAttributes.cmst_traceId.c_str());
                cJSON_AddStringToObject(root, "cmst_code", std::to_string(m_stRingAttributes.cmst_code).c_str());
                cJSON_AddStringToObject(root, "hashed_id", m_stRingAttributes.current_HashID.c_str());
                char** rspArr;
                int rspArrSize = 2;
                int rspArrLen = 128;
                rspArr = (char**)malloc(rspArrSize*sizeof(char*));

                if(rspArr) {
                    for (int i = 0; i < rspArrSize; i++)
                        rspArr[i] = (char*)calloc(rspArrLen, sizeof(char));

                    std::string b64encoded_str;
                    if(rspArr[0]) {
                        convert_Hex2ByteArray_n_Encode_ToaCmdRsp(m_stRingAttributes.cmd_Ready.rsp_notify_data, b64encoded_str);
                        if(!b64encoded_str.empty()) {
                            BTRLEMGRLOG_INFO("Encoded MEP_TOA_COMMANDS_READY Data [%s]\n", b64encoded_str.c_str());
                            snprintf(rspArr[0], rspArrLen, b64encoded_str.c_str());
                        }
                    }
                    b64encoded_str.clear();
                    if(rspArr[1]) {
                        convert_Hex2ByteArray_n_Encode_ToaCmdRsp(m_stRingAttributes.cmd_Play.rsp_notify_data, b64encoded_str);
                        if(!b64encoded_str.empty()) {
                            BTRLEMGRLOG_INFO("Encoded MEP_TOA_COMMANDS_PLAY Data [%s]\n", b64encoded_str.c_str());
                            snprintf(rspArr[1], rspArrLen, b64encoded_str.c_str());
                        }
                    }
                    b64encoded_str.clear();
                    cJSON_AddItemToObject(root, "responses", cJSON_CreateStringArray((const char**)rspArr, rspArrSize));

                    for (int i = 0; i < rspArrSize; i++)
                        free(rspArr[i]);
                    free(rspArr);
                }
                jsonPkt = cJSON_PrintUnformatted(root);
                cJSON_Delete(root);
            }
            std::string urlStr = pi8CloudURL;
            std::size_t found = urlStr.find(codebigUrl);

            if (found!=std::string::npos) {
                isCodebigSupported = true;
                uri = "/api/v2/bte/device/tilestatus";
            }

            BTRLEMGRLOG_INFO("m_stRingAttributes.current_HashID.length() = %llu, isStagingEnv = %d\n",
                   (unsigned long long)m_stRingAttributes.current_HashID.length(),isStagingEnv);
            if(m_stRingAttributes.current_HashID.length() && (0 == isStagingEnv)) {
                notify_enable = true;
            }
            else {
                do_webpa_notify (jsonPkt);
                notify_enable = false;
            }

        }
    }
    break;
    case LE_TILE_NOTIFY_TYPE_NONE:
    default:
        notify_enable = false;
        break;
    }

    if(!jsonPkt) {
        BTRLEMGRLOG_ERROR("Failed to send cloud notification, since Payload is Empty.\n");
        return LE_NOTIFY_SEND_FAILED;
    }

    if(notify_enable) {
        CURL *pCurl = NULL;
        CURLcode res;
        struct curl_slist *headers = NULL;

        curl_global_init(CURL_GLOBAL_ALL);

        pCurl = curl_easy_init();

        if(pCurl) {

            curl_easy_setopt(pCurl, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(pCurl, CURLOPT_URL, pi8CloudURL);

           // This will be executed only if we test batch update/ring with staging CTM wend point which requirs whitelisting.
           // every time device reboots it change the IP and we needed to whitelist the new IP. This to work around of ths whitelisting issue.
           //whitelisting is still needed one time.
            if(0 == (access( "/tmp/.stagingTileUrl", F_OK )))
            {
                char str_ip[25]= {0};
                FILE *fptr;
                int retVal = 0;
                if ((fptr = fopen("/tmp/.estb_ip", "r")) == NULL) {
                    BTRLEMGRLOG_ERROR("Error opening /tmp/.estb_ip file");
                }
                else{
                    retVal = fscanf(fptr, "%s", str_ip);
                    if(1==retVal){
                        BTRLEMGRLOG_INFO("Interface IP(estb_ip) = %s\n",str_ip);
                        curl_easy_setopt(pCurl, CURLOPT_INTERFACE, str_ip);
                    }
                    else{
                        BTRLEMGRLOG_ERROR("Failed to read Interface IP(estb_ip)\n");
                    }
                    fclose(fptr);
                }

            }


            BTRLEMGRLOG_DEBUG("\t [%d]Post to Endpoint \t[%s]\n", __LINE__, pi8CloudURL);

            headers = curl_slist_append(headers, "Content-Type: application/json");

            if(isCodebigSupported) {

                std::string oAuth_header = "Authorization: OAuth ";

                const char* oauth_consumer_key = "oauth_consumer_key=";
                std::string oauth_consumer_value;

                const char* oauth_nonce_key = "oauth_nonce=";
                std::string oauth_nonce_value;

                const char* oauth_signature_method_key = "oauth_signature_method=";
                std::string oauth_signature_method_value;

                const char* oauth_timestamp_key = "oauth_timestamp=";
                std::string oauth_timestamp_value;

                const char* oauth_version_key = "oauth_version=";
                std::string oauth_version_value;

                const char* oauth_signature_key = "oauth_signature=";
                std::string oauth_signature_value;

                bool status = false;
#ifdef BTR_LOCAL_OAUTH_SUPPORT
                status = btrLeMgrUtils::oauth1::generateOauthHeaderDetails(
                             pi8CloudURL,
                             oauth_consumer_value,
                             oauth_nonce_value,
                             oauth_signature_method_value,
                             oauth_timestamp_value,
                             oauth_version_value,
                             oauth_signature_value );
                BTRLEMGRLOG_INFO("Generated OauthHeader Details locally.\n");

#else
                std::string baseStr;
                status = btrLeMgrUtils::generateUrlOauthBaseString(uri.c_str(), baseStr);
                BTRLEMGRLOG_INFO("Generated Oauth credentials using cpg.\n");
                if(status) {
                    BTRLEMGRLOG_DEBUG("Generated base string is : [%s] \n", baseStr.c_str());

                    btrLeMgrUtils::getAuthorizationOathHeaderValue(baseStr.c_str(),
                            oauth_consumer_key,
                            oauth_consumer_value);

                    btrLeMgrUtils::getAuthorizationOathHeaderValue(baseStr.c_str(),
                            oauth_nonce_key,
                            oauth_nonce_value);

                    btrLeMgrUtils::getAuthorizationOathHeaderValue(baseStr.c_str(),
                            oauth_signature_method_key,
                            oauth_signature_method_value);

                    btrLeMgrUtils::getAuthorizationOathHeaderValue(baseStr.c_str(),
                            oauth_timestamp_key,
                            oauth_timestamp_value);

                    btrLeMgrUtils::getAuthorizationOathHeaderValue(baseStr.c_str(),
                            oauth_version_key,
                            oauth_version_value);


                    btrLeMgrUtils::getAuthorizationOathHeaderValue(baseStr.c_str(),
                            oauth_signature_key,
                            oauth_signature_value);
                }
                else {
                    BTRLEMGRLOG_ERROR("Failed to generated OAUTH Credentials.\n");
                }
#endif

                if(status) {
                    if(!oauth_consumer_value.empty()) {
                        oAuth_header += oauth_consumer_key;
                        oAuth_header +=  "\""+oauth_consumer_value+"\",";
                    }

                    if(!oauth_nonce_value.empty()) {
                        oAuth_header += oauth_nonce_key;
                        oAuth_header += "\""+ oauth_nonce_value +"\",";
                    }


                    if(!oauth_signature_method_value.empty()) {
                        oAuth_header +=  oauth_signature_method_key;
                        oAuth_header +=  "\""+ oauth_signature_method_value +"\",";
                    }


                    if(!oauth_timestamp_value.empty()) {
                        oAuth_header +=  oauth_timestamp_key;
                        oAuth_header +=  "\""+ oauth_timestamp_value +"\",";
                    }


                    if(!oauth_version_value.empty()) {
                        oAuth_header +=  oauth_version_key;
                        oAuth_header +=  "\""+ oauth_version_value +"\",";
                    }


                    if(!oauth_signature_value.empty()) {
                        oAuth_header +=  oauth_signature_key;
                        oAuth_header +=  "\""+ oauth_signature_value +"\"";
                    }

                    BTRLEMGRLOG_DEBUG("oauth_consumer_value is : [%s] \n", oauth_consumer_value.c_str());
                    BTRLEMGRLOG_DEBUG("oauth_nonce_value is : [%s] \n", oauth_nonce_value.c_str());
                    BTRLEMGRLOG_DEBUG("oauth_timestamp_value is : [%s] \n", oauth_timestamp_value.c_str());
                    BTRLEMGRLOG_DEBUG("oauth_signature_method_value is : [%s] \n", oauth_signature_method_value.c_str());
                    BTRLEMGRLOG_DEBUG("oauth_version_value is : [%s] \n", oauth_version_value.c_str());
                    BTRLEMGRLOG_DEBUG("oauth_signature_value is : [%s] \n", oauth_signature_value.c_str());

                    headers = curl_slist_append(headers, oAuth_header.c_str());
                }
            }
            curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, headers);

            BTRLEMGRLOG_INFO("The Tile Json payload is ( %s ). \n", jsonPkt);
            curl_easy_setopt(pCurl, CURLOPT_POSTFIELDS, jsonPkt);
            res = curl_easy_perform(pCurl);

            if(res != CURLE_OK) {
                BTRLEMGRLOG_ERROR("Failed  in curl_easy_perform() with error: %s\n", curl_easy_strerror(res));
            }
            else {
                long response_code;
                curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &response_code);

                if(response_code == 200) { /* Message posted successfully if get http response code is 200 OK */
                    ret = LE_NOTIFY_SEND_SUCCESSFULLY;
                    BTRLEMGRLOG_DEBUG("Successfully posted to server, response code (%ld). \n", response_code);
                }
                else {
                    ret = LE_NOTIFY_SEND_FAILED;
                    BTRLEMGRLOG_DEBUG("Failed to Post Tile message, returns with error code (%ld). \n", response_code);
                }
            }

            curl_easy_cleanup(pCurl);
        }
        curl_global_cleanup();
    }
    else {
        BTRLEMGRLOG_INFO("Post Notification to cloud is disabled.\n");
    }
    if(jsonPkt) free(jsonPkt);

    BTRLEMGRLOG_TRACE("Exiting..\n");
    return (ret);
}

void
BtrTileMgr::do_webpa_notify (
    const char* jsonPkt
) {
    BTRLEMGRLOG_INFO("Json payload is ( %s ). \n", jsonPkt);

    HOSTIF_MsgData_t param;
    memset (&param, 0, sizeof(HOSTIF_MsgData_t));
    snprintf (param.paramName, TR69HOSTIFMGR_MAX_PARAM_LEN, "Device.DeviceInfo.X_RDKCENTRAL-COM_xBlueTooth.BLE.Tile.Cmd.Notify");
    snprintf (param.paramValue, TR69HOSTIFMGR_MAX_PARAM_LEN, "%s", jsonPkt);
    param.paramtype = hostIf_StringType;
    IARM_Bus_Call (IARM_BUS_TR69HOSTIFMGR_NAME, IARM_BUS_TR69HOSTIFMGR_API_SetParams, &param, sizeof(param));
}


BTRLEMGR_Result_t
BtrTileMgr::add_LeDevPropToMap (
    ui_long_long devId,
    const char*  mac,
    const char*  name,
    const char*  readValue,
    std::string srvData,
    std::string current_HashID

) {
    BTRLEMGR_Result_t ret = LEMGR_RESULT_SUCCESS;

    BTRLEMGRLOG_TRACE("Entering..\n");

    BtrLeDevProp *leDevPropObj = new BtrLeDevProp();

    time_t currentTimestamp = time(nullptr);

    leDevPropObj->m_devId = devId;
    leDevPropObj->m_isLeDevice = true;
    leDevPropObj->m_macAddr = mac;
    leDevPropObj->m_lastNotifyTimestamp = currentTimestamp;
    leDevPropObj->m_lastConnectTimestamp = currentTimestamp;
    leDevPropObj->m_discoveryCount = 1;
    leDevPropObj->m_notifyState = LE_NOTIFY_SEND_SUCCESSFULLY;
    leDevPropObj->m_readValue = readValue;
    leDevPropObj->m_sericeData = srvData;
    leDevPropObj->m_current_HashID = current_HashID;

    std::lock_guard<std::mutex> lg(m_mtxMap);
    m_LeDevMap.insert({devId, leDevPropObj});

    BTRLEMGRLOG_INFO("Added Le Device to hash map, with device id [%llu], Mac Address [%s] with notification time: %s leDevPropObj->m_sericeData = %s \n", \
                     devId, mac, asctime(localtime(&currentTimestamp)), leDevPropObj->m_sericeData.c_str());

    BTRLEMGRLOG_TRACE("Exiting..\n");
    return (ret);
}

bool
BtrTileMgr::is_LeDevPresentInMap (
    ui_long_long devId
) {
    std::lock_guard<std::mutex> lg(m_mtxMap);

    bool ret = (m_LeDevMap.find(devId) != m_LeDevMap.end());
    BTRLEMGRLOG_INFO("Tile (device id %llu): %s in map.\n", devId, ret ? "present" : "not present");
    if (ret) {
        BTRLEMGRLOG_INFO("Previously discovered device (%llu), present in hashmap.\n", devId);
    }
    return ret;
}

bool
BtrTileMgr::check_LeDevThrottleTimeInMap (
    ui_long_long devId
) {
    std::lock_guard<std::mutex> lg(m_mtxMap);

    bool ret = false;
    auto it = m_LeDevMap.find(devId);
    if(it != m_LeDevMap.end()) {

        time_t currentTimestamp = time(nullptr);
        time_t lastNotifyTimestamp = ((BtrLeDevProp *)(it->second))->m_lastNotifyTimestamp;
        int tInterval = currentTimestamp - lastNotifyTimestamp;

        ((BtrLeDevProp *)(it->second))->m_discoveryCount++;

        if(tInterval >= MAX_LE_TILE_NOTIFY_INTERVAL) {
            BTRLEMGRLOG_INFO("Previously discovered device (%llu). Last notification send on (%d) seconds ago, more then max time out interval (%d).So, again Send Notification... \n",
                             devId, tInterval, MAX_LE_TILE_NOTIFY_INTERVAL);
            ret = true;
        }
        else {
            BTRLEMGRLOG_INFO("Previously discovered device (%llu). Last notification send on (%d) second ago, which is less then max time out (%d). Don't Send Notification... \n",
                             devId, tInterval, MAX_LE_TILE_NOTIFY_INTERVAL);
        }
    }
    return (ret);
}


void
BtrTileMgr::print_LeDevPropMap (
    void
) {
    std::lock_guard<std::mutex> lg(m_mtxMap);

    if(m_LeDevMap.size()) {
        short index = 1;
        BTRLEMGRLOG_INFO("Total numbers of Tile device (%llu) notified to cloud.\n", (unsigned long long)m_LeDevMap.size());
        BTRLEMGRLOG_INFO("===============================================================================\n");
        for (auto it = m_LeDevMap.begin(); it!=m_LeDevMap.end(); ++it) {
            BtrLeDevProp *leObj = it->second;
            char *timeStamp = asctime(localtime(&leObj->m_lastNotifyTimestamp));
            BTRLEMGRLOG_INFO( "[%d] Device Id (%llu) => isLeDevice:(%d), Mac Address:(%s), Last notify state(%d) and TimeStamp : %s",
                              index++, it->first,
                              leObj->m_isLeDevice, leObj->m_macAddr.c_str(), leObj->m_notifyState, timeStamp);
        }
        BTRLEMGRLOG_INFO("===============================================================================\n");
    }
}


void
BtrTileMgr::delete_LeDevMap (
    void
) {
    std::lock_guard<std::mutex> lg(m_mtxMap);

    for (auto it = m_LeDevMap.begin(); it!=m_LeDevMap.end(); ++it) {
        BtrLeDevProp *leObj = it->second;
        delete leObj;
    }
    m_LeDevMap.clear();
}

void
BtrTileMgr::delete_LeDevMap (
    ui_long_long devId
) {
    std::lock_guard<std::mutex> lg(m_mtxMap);

    BTRLEMGRLOG_INFO( "Clearing Tile Hash Map.\n");
    auto it = m_LeDevMap.find(devId);
    if(it != m_LeDevMap.end()) {
        BTRLEMGRLOG_DEBUG ("Found Same device : %llu\n", devId);
        delete ((BtrLeDevProp *)(it->second));
        m_LeDevMap.erase(devId);
    }
}

void
BtrTileMgr::updateTimestamp_LeDevMap (
    ui_long_long     devId,
    enBtrNotifyState state,
    bool             update_connect_ts
) {
    std::lock_guard<std::mutex> lg(m_mtxMap);

    /* Update time and notification state */
    auto it = m_LeDevMap.find(devId);

    if(it != m_LeDevMap.end()) {
        if(LE_NOTIFY_SEND_SUCCESSFULLY == state) {
            ((BtrLeDevProp *)(it->second))->m_lastNotifyTimestamp = time(0);
            ((BtrLeDevProp *)(it->second))->m_lastSeenTimestamp = ((BtrLeDevProp *)(it->second))->m_lastNotifyTimestamp;
            if(update_connect_ts) {
                ((BtrLeDevProp *)(it->second))->m_lastConnectTimestamp = ((BtrLeDevProp *)(it->second))->m_lastNotifyTimestamp;
            }
            ((BtrLeDevProp *)(it->second))->m_notifyState = LE_NOTIFY_SEND_SUCCESSFULLY;
        }
        else {
            ((BtrLeDevProp *)(it->second))->m_notifyState = LE_NOTIFY_SEND_FAILED;
        }
    }
}

void
BtrTileMgr::updateTileUUID_LeDevMap (
        ui_long_long     devId,
        std::string      tile_uuid
        ) {
    std::lock_guard<std::mutex> lg(m_mtxMap);

    /* Update tile_uuid received during MEP_TOA_OPEN_CHANNEL  */
    auto it = m_LeDevMap.find(devId);

    if(it != m_LeDevMap.end()) {
        BTRLEMGRLOG_INFO ("updating tile_uuid(%s) for devID (%llu) received during\
                MEP_TOA_OPEN_CHANNEL.\n", tile_uuid.c_str(), m_stRingAttributes.devId);
        ((BtrLeDevProp *)(it->second))->m_readValue= tile_uuid;
    }
    else{
        BTRLEMGRLOG_ERROR ("unable to find devID(%llu) in LeDevMap\n", devId);
    }
}

void
BtrTileMgr::updateTileServiceData_LeDevMap (
        ui_long_long     devId,
        std::string     serviceData
        ) {
    std::lock_guard<std::mutex> lg(m_mtxMap);

    auto it = m_LeDevMap.find(devId);

    if(it != m_LeDevMap.end()) {
        BTRLEMGRLOG_INFO ("updating serviceData(%s) for devID (%llu) received from TILE with \
                constant MAC and rotating serviceData.\n", serviceData.c_str(), devId);
        ((BtrLeDevProp *)(it->second))->m_sericeData = serviceData;
    }
    else{
        BTRLEMGRLOG_ERROR ("unable to find devID(%llu) in LeDevMap\n", devId);
    }
}
/*
 * This method responsibilities are:
 * 1. To check the hash map for Tile notified devices against the threshold time
 * 2. Reconnect again to previously discovered tile devices after 10 min
 * 3. If successfully connected and notified, then update the time stamped
 * 4. If not, then delete from hash
 *
 */
void
BtrTileMgr::checkThresholdTimeAndReConnectNotify (
    void
) {
    std::vector <BtrLeDevProp *> devlist;
    BTRLEMGRLOG_TRACE("Entering.. \n");

    if(m_stRingAttributes.isRingRequested) {

        time_t current_ts = time(nullptr);
        if((current_ts - m_stRingAttributes.ring_trigger_ts) > 60) {
            m_stRingAttributes = empty;
        }
        else {
            BTRLEMGRLOG_INFO("Tile Ring Request is in Progress.., so not checking threshold time. \n");
            return;
        }
    }
    BTRLEMGRLOG_TRACE("[%d] \n", __LINE__);

    {
        /* Take a snapshot of map. We'll process each entry later once we've unlocked the map. This is necessary because if we do connect,
        it is necessary to give up the below mutex in order to avoid a deadlock. And once you give up the mutex, the iterators are no
        longer guaranteed to be valid.*/
        std::lock_guard<std::mutex> lg(m_mtxMap);
        if(m_LeDevMap.size())
            for (const auto &entry : m_LeDevMap) {
                devlist.push_back(entry.second);
            }
    }

    BTRLEMGRLOG_TRACE("[%d] \n", __LINE__);

    if(m_LeDevMap.size()) {
        for (const auto &leObj: devlist) {
            time_t currentTimestamp = time(nullptr);
            ui_long_long devId = leObj->m_devId;

            BTRLEMGRLOG_TRACE("[%d] \n", __LINE__);

            /* If Tile having service data, then don't reconnect it */
            if (!leObj->m_sericeData.length()) {
                int tInterval = currentTimestamp - leObj->m_lastConnectTimestamp;

                BTRLEMGRLOG_TRACE("[%d] \n", __LINE__);

                if (tInterval >= MAX_LE_TILE_RECONNECT_INTERVAL) {
                    BTRLEMGRLOG_INFO("Previously discovered device (%llu). Last notification send on (%d) seconds ago, more then max time out interval (%d).So, again reconnecting it.\n",
                                     devId, tInterval, MAX_LE_TILE_RECONNECT_INTERVAL);
                    if(LEMGR_RESULT_SUCCESS == doConnect(devId)) {
                        BTRLEMGRLOG_INFO("Successfully Reconnected to previously disovered Tile Device [(%llu), (%s)]. \n", devId, leObj->m_macAddr.c_str());
                    }
                    else { /*In case of Connect Failure delete the Tile entry from hash */
                        delete (leObj);
                        std::lock_guard<std::mutex> lg(m_mtxMap);
                        m_LeDevMap.erase(devId);
                        BTRLEMGRLOG_INFO("Failed to reconnect, so Removed Tile (device id %llu) from map.\n", devId);
                    }
                    break;
                }
            }
        }
    }
    BTRLEMGRLOG_TRACE("Exiting.. \n");
}

ui_long_long
BtrTileMgr::retrieve_DevId_from_RingAttributes (
) {
    ui_long_long    devId = 0;
    bool            isPresent=false;
    std::string     tileId=m_stRingAttributes.tile_uuid;
    std::vector<std::string>received_HashIDs = m_stRingAttributes.received_HashIDs;
    std::string     current_HashID;
    BTRLEMGRLOG_TRACE("Entering.. \n");

    if (isBeaconDetectionLimited()) {
        BTRLEMGRLOG_WARN("Exiting... - Beacon Detection Limited\n");
        return devId;
    }

    std::lock_guard<std::mutex> lg(m_mtxMap);

    if((m_stRingAttributes.received_HashIDs.empty())||(m_stRingAttributes.oper_code.compare("MEP_TOA_OPEN_CHANNEL") != 0)){
        BTRLEMGRLOG_DEBUG("Looking for DevId for requested Tile ID (%s)\n", tileId.c_str());

        if(!m_LeDevMap.empty() && (!tileId.empty())) {
            for (auto it = m_LeDevMap.begin(); it!=m_LeDevMap.end(); ++it) {
                BtrLeDevProp *leObj = it->second;
                BTRLEMGRLOG_INFO( "Device Id (%llu) => Mac Address:(%s), Tile ID: (%s)\n",
                        it->first, leObj->m_macAddr.c_str(), leObj->m_readValue.c_str());
                if(tileId.compare(leObj->m_readValue) == 0) {
                    BTRLEMGRLOG_INFO( "Found Device Id as (%llu)\n", it->first);
                    devId = it->first;
                    isPresent = true;
                    break;
                }
            }
            BTRLEMGRLOG_INFO("Got Device Id: (%llu) for Tile ID: (%s)\n", devId, tileId.c_str());
        }

        BTRLEMGRLOG_INFO("Requested Tile Id (%s) is %s in map.\n", tileId.c_str(), (isPresent)?"Present":"Not Present");
    }
    else{
        BTRLEMGRLOG_DEBUG("Looking for DevId for requested HashIDs \n");
        if(!m_LeDevMap.empty() && (!received_HashIDs.empty())){
            for(std::vector<std::string>::const_iterator itr = received_HashIDs.begin();itr!= received_HashIDs.end(); ++itr){
                current_HashID = itr->c_str();
                BTRLEMGRLOG_INFO( "Comparing Hash Id as (%s)\n", current_HashID.c_str());
                if(!current_HashID.empty()){
                    for (auto it = m_LeDevMap.begin(); it!=m_LeDevMap.end(); ++it) {
                        BtrLeDevProp *leObj = it->second;
                        BTRLEMGRLOG_INFO( "DevProp Device Id (%llu) => Mac Address:(%s), Hash ID: (%s)\n",
                                it->first, leObj->m_macAddr.c_str(), leObj->m_current_HashID.c_str());
                        BTRLEMGRLOG_INFO( "Received Hash Id as (%s)\n", current_HashID.c_str());
                        if(current_HashID.compare(leObj->m_current_HashID) == 0) {
                            BTRLEMGRLOG_INFO( "Found Device Id as (%llu)\n", it->first);
                            devId = it->first;
                            m_stRingAttributes.current_HashID = current_HashID;
                            isPresent = true;
                            break;
                        }
                    }
                }
            }
        }
        else{
            BTRLEMGRLOG_INFO("LeDevMap is %s, received_HashIDs is %s", m_LeDevMap.empty()?"empty":"full", received_HashIDs.empty()?"empty":"full");
        }

        if(isPresent){
            BTRLEMGRLOG_INFO("Got Device Id: (%llu) for Hash ID: (%s)\n", devId, current_HashID.c_str());
        }
        else{
            BTRLEMGRLOG_INFO("Requested Hash Id is not present in map.\n");
        }
    }

    BTRLEMGRLOG_TRACE("Exiting... \n");
    return devId;
}


std::string
BtrTileMgr::retrieve_TileId_from_DevId (
    ui_long_long devId
) {
    std::string foundTileId = std::string();


    if (isBeaconDetectionLimited()) {
        BTRLEMGRLOG_WARN("Exiting... - Beacon Detection Limited\n");
        return foundTileId; //Empty
    }

    BTRLEMGRLOG_DEBUG("Looking for requested Tile ID from DevId (%llu)\n", devId);

    std::lock_guard<std::mutex> lg(m_mtxMap);
    if(!m_LeDevMap.empty() && (devId != 0)) {
        for (auto it = m_LeDevMap.begin(); it!=m_LeDevMap.end(); ++it) {
            BtrLeDevProp *leObj = it->second;
            BTRLEMGRLOG_INFO( "Device Id (%llu) => Mac Address:(%s), Tile ID: (%s)\n",
                              it->first, leObj->m_macAddr.c_str(), leObj->m_readValue.c_str());
        
            if (devId == it->first) {
                BTRLEMGRLOG_INFO( "Found Device Id as (%llu)\n", it->first);
                foundTileId = leObj->m_readValue;
                break;
            }
        }
    }

    return foundTileId;
}


BtrTileMgr::BtrTileMgr (
    std::string uuid
) : BtrLeMgrBase(uuid),
    m_run_request_processor(true),
    m_scan_timer_active(false),
    m_scan_resume_countdown_active(false)
{
    m_bQuitThread = false;
    m_request_processor = std::thread(&BtrTileMgr::request_processor, this);
    m_last_discovery_timestamp = std::chrono::system_clock::now(); //sane default
}

BtrTileMgr::~BtrTileMgr (
) {
    if(m_request_processor.joinable()) {
        std::unique_lock <std::mutex> lock(m_request_mutex);
        m_run_request_processor = false;
        lock.unlock();
        m_request_q_cond.notify_all();
        m_request_processor.join();
    }
}

BtrTileMgr*
BtrTileMgr::getInstance (
) {
    //TODO: Bad idea to have a singleton class.
    // Even if we continuw to use singleton we should use ref counting to delete
    if (instance == NULL) {
        instance = new BtrTileMgr("xfeed");
        BtrTileMgr::checkRfcFeatureOfTileRing();
    }

    return instance;
}

void
BtrTileMgr::releaseInstance (
    void
) {
    if (instance) {
        delete instance;
        instance = NULL;
    }
}

/* This was for POC */
BTRLEMGR_Result_t
BtrTileMgr::doRingATile (
    std::string tileId,
    std::string session
) {

    BTRLEMGRLOG_TRACE("Entering.. \n");
    enBtrNotifyState status = LE_NOTIFY_MSG_CLOUD_POST_STATUS_OK;

    //if(m_isTile_Ring_in_progress)
    //{
    //    return (LEMGR_RESULT_LE_RING_FAIL);
    // }
    //else
    {
        ui_long_long handle = retrieve_DevId_from_RingAttributes();
//        ui_long_long handle = 258975189890458; // hard-coded for PoC purposes only
        BTRLEMGRLOG_INFO("Tile Device ID (%llu), (%s)]. \n", handle, tileId.c_str());

        if(!tileId.empty() && handle) {
            m_pBtrMgrIfce->stopLeDeviceDiscovery();
            sleep(1);
            if(false == m_pBtrMgrIfce->doConnectLeDevice(handle)) {
                BTRLEMGRLOG_INFO ("Failed to Ring a tile (%llu), since Connect to device failed. \n", handle);
                status = LE_NOTIFY_MSG_CLOUD_POST_STATUS_NOT_FOUND;
            }
            else {
                sleep(1);
                BTRLEMGRLOG_INFO ("Successfully connected to Tile device.\n");
                {
                    std::string notifyUuid = "9d410019-35d6-f4dd-ba60-e7bd8dc491c0";
                    std::string value;

                    bool ret;
                    ret = m_pBtrMgrIfce->performLeOps(handle, LEMGR_OP_START_NOTIFY, notifyUuid, value);

                    if ( !ret) {
                        BTRLEMGRLOG_ERROR ("Failed to set notify.\n");
                        status = LE_NOTIFY_MSG_CLOUD_POST_STATUS_NOT_FOUND;
                    }
                    else {
                        BTRLEMGRLOG_INFO ("Successfully set notification. \n");
                        //sleep(1);
                        /*Wrire to Tile Device in char: 9d410018-35d6-f4dd-ba60-e7bd8dc491c0 */
                        std::string writeCharUuid = "9d410018-35d6-f4dd-ba60-e7bd8dc491c0";

                        std::string wValue_TOA_CMD_OPEN_CHANNEL = "0000000000100000000000000000000000000000";
                        std::string wValue_TOA_CMD_READY = "0212000000000000000000000000000000000000";
                        std::string wValue_TOA_CMD_SONG = "020502010300000000";

                        ret = m_pBtrMgrIfce->performLeOps(handle, LEMGR_OP_WRITE_VALUE, writeCharUuid, wValue_TOA_CMD_OPEN_CHANNEL);
                        if ( !ret) {
                            BTRLEMGRLOG_ERROR ("Failed to set TOA_CMD_OPEN_CHANNEL.\n");
                            status = LE_NOTIFY_MSG_CLOUD_POST_STATUS_NOT_FOUND;
                        }
                        else {
                            BTRLEMGRLOG_INFO ("Successfully set TOA_CMD_OPEN_CHANNEL. \n");
                            //sleep(1);
                            /* @TODO: On write, need to listen notify response and parse the message to get CID */
                            ret = m_pBtrMgrIfce->performLeOps(handle, LEMGR_OP_WRITE_VALUE, writeCharUuid, wValue_TOA_CMD_OPEN_CHANNEL);
                            if (!ret) {
                                BTRLEMGRLOG_ERROR ("Failed to set TOA_CMD_READY.\n");
                                status = LE_NOTIFY_MSG_CLOUD_POST_STATUS_NOT_FOUND;
                            }
                            else {
                                BTRLEMGRLOG_INFO ("Successfully set TOA_CMD_OPEN_READY. \n");
                                //sleep(3);
                                /* @TODO: On write, need to listen notify response and parse the message to get CID */
                                ret = m_pBtrMgrIfce->performLeOps(handle, LEMGR_OP_WRITE_VALUE, writeCharUuid, wValue_TOA_CMD_SONG);
                                if (! ret) {
                                    BTRLEMGRLOG_ERROR ("Failed to set TOA_CMD_SONG.\n");
                                    status = LE_NOTIFY_MSG_CLOUD_POST_STATUS_NOT_FOUND;
                                }
                                else {
                                    BTRLEMGRLOG_INFO ("Successfully set TOA_CMD_SONG. \n");
                                }
                            }
                        }
                    }
                }
                m_pBtrMgrIfce->doDisconnectLeDevice(handle);
            }
            notifyTileInfoCloud(tileId, session, LE_TILE_NOTIFY_TYPE_RING_REQUEST, status);
            m_pBtrMgrIfce->startLeDeviceDiscovery("xfeed");
        }
    }
    BTRLEMGRLOG_TRACE ("Exiting... \n");
    return (LEMGR_RESULT_SUCCESS);
}

void
BtrTileMgr::request_processor (
) {
    BTRLEMGRLOG_TRACE("Entering\n");

    while(m_run_request_processor) {
        std::unique_lock <std::mutex> lock(m_request_mutex);

        m_request_q_cond.wait(lock, [this]() {
            return (!m_request_queue.empty() || !m_run_request_processor);
        });

        if(m_run_request_processor) {
            std::string request = std::move(m_request_queue.front());
            m_request_queue.pop_front();
            lock.unlock();
            processTileCmdRequest_stage2(request);
        }
    }

    BTRLEMGRLOG_TRACE ("Exiting.\n");
}


BTRLEMGR_Result_t
BtrTileMgr::processTileCmdRequest (
    std::string request
) {
    BTRLEMGRLOG_TRACE("Entering.. \n");

    BTRLEMGR_Result_t ret = LEMGR_RESULT_SUCCESS;

    if(false == m_isTileRingFeatureEnabled) {
        BTRLEMGRLOG_ERROR("Tile Ring Feature is Disabled, so Failed to process Tile Ring Request.\n");
        return LEMGR_RESULT_LE_RING_FAIL;
    }

    if(request.empty()) {
        BTRLEMGRLOG_ERROR("Failed due to empty Tile Request message.\n");
        return LEMGR_RESULT_LE_RING_FAIL;
    }

    if ((ret = checkTileCmdRequest(request)) != LEMGR_RESULT_SUCCESS) {
        return ret;
    }

    std::unique_lock<std::mutex> lock(m_request_mutex);
    m_request_queue.emplace_back(std::move(request));
    lock.unlock();
    m_request_q_cond.notify_all();

    BTRLEMGRLOG_TRACE ("Exiting...\n");
    return ret;
}


BTRLEMGR_Result_t
BtrTileMgr::checkTileCmdRequest (
    const std::string &request
) {
    BTRLEMGRLOG_TRACE("Entering.. \n");

    BTRLEMGR_Result_t ret = LEMGR_RESULT_SUCCESS;

    const char *strJson = request.c_str();
    BTRLEMGRLOG_DEBUG("***************************\n");
    BTRLEMGRLOG_DEBUG("[%s] The Json string= [ %s ]\n",__FUNCTION__, strJson);
    BTRLEMGRLOG_DEBUG("***************************\n");

    cJSON *cjson = cJSON_Parse(strJson);

    if(NULL == cjson) {
        BTRLEMGRLOG_ERROR("Failed to parse the Json string. \n");
        ret = LEMGR_RESULT_LE_RING_FAIL;
    }
    else if ((m_stRingAttributes.isRingRequested == true) &&
             (m_stRingAttributes.ring_state >= LE_TILE_RING_REQUEST_INITIATED) &&
             (m_stRingAttributes.ring_state <  LE_TILE_RING_DONE)) {
        //first Red the code if its is MEP_TOA_OPEN_CHANNEL
        if ( cJSON_GetObjectItem( cjson, "code") != NULL ) {
            char* cmd = cJSON_GetObjectItem(cjson, "code")->valuestring;

            if(strcasecmp(cmd,"MEP_TOA_OPEN_CHANNEL") == 0) {
                if ( cJSON_GetObjectItem( cjson, "tile_uuid") != NULL ) {
                    if (m_stRingAttributes.tile_uuid == cJSON_GetObjectItem(cjson, "tile_uuid")->valuestring) {
                        BTRLEMGRLOG_INFO("Ring Already inPrgoress on The Tile UUID : [%s]. \n", (m_stRingAttributes.tile_uuid).c_str());
                        ret = LEMGR_RESULT_LE_RING_FAIL_INPROGRESS;
                    }
                }
            }
        }
    }

    cJSON_Delete(cjson);

    return ret;
}


BTRLEMGR_Result_t
BtrTileMgr::processTileCmdRequest_stage2 (
    const std::string &request
) {
    int idx = 0;
    BTRLEMGRLOG_TRACE("Entering.. \n");

    BTRLEMGR_Result_t ret = LEMGR_RESULT_SUCCESS;

    const char *strJson = request.c_str();
    BTRLEMGRLOG_DEBUG("***************************\n");
    BTRLEMGRLOG_DEBUG("[%s] The Json string= [ %s ]\n",__FUNCTION__, strJson);
    BTRLEMGRLOG_DEBUG("***************************\n");

    cJSON *cjson = cJSON_Parse(strJson);
    std::unique_lock <std::mutex> lock(m_request_mutex);

    if(NULL == cjson) {
        BTRLEMGRLOG_ERROR("Failed to parse the Json string. \n");
        return LEMGR_RESULT_LE_RING_FAIL;
    }
    else {
        m_stRingAttributes.ring_state = LE_TILE_RING_REQUEST_INITIATED;
        //first Red the code if its is MEP_TOA_OPEN_CHANNEL
        if ( cJSON_GetObjectItem( cjson, "code") != NULL ) {
            char* cmd = cJSON_GetObjectItem(cjson, "code")->valuestring;

            if(strcasecmp(cmd,"MEP_TOA_OPEN_CHANNEL") == 0) {
                m_stRingAttributes.isRingRequested = true;
                m_stRingAttributes.cmst_ctm_from_cpe_ts = time(nullptr);

                m_stRingAttributes.oper_code = cmd;
                if ( cJSON_GetObjectItem(cjson, "hashed_ids") != NULL ){

                    cJSON * hash_ID_Array = cJSON_GetObjectItem(cjson, "hashed_ids");
                    BTRLEMGRLOG_DEBUG("The array length = %d\n",cJSON_GetArraySize(hash_ID_Array));
                    m_stRingAttributes.received_HashIDs.clear();
                    for(idx=0; idx < cJSON_GetArraySize(hash_ID_Array); idx++){
                        cJSON * hash_ID = cJSON_GetArrayItem(hash_ID_Array, idx);
                        if(hash_ID != NULL){
                            m_stRingAttributes.received_HashIDs.push_back(hash_ID->valuestring);
                        }
                    }
                    for (std::vector<std::string>::iterator itr = m_stRingAttributes.received_HashIDs.begin(); \
                            itr != m_stRingAttributes.received_HashIDs.end(); ++itr){
                        BTRLEMGRLOG_INFO("The Hash IDs : [%s]. \n", itr->c_str());
                    }
                }
                if ( cJSON_GetObjectItem( cjson, "tile_uuid") != NULL ) {
                    m_stRingAttributes.tile_uuid = cJSON_GetObjectItem(cjson, "tile_uuid")->valuestring;
                    BTRLEMGRLOG_INFO("The Tile UUID : [%s]. \n", (m_stRingAttributes.tile_uuid).c_str());
                }
                if ( cJSON_GetObjectItem( cjson, "rand_a") != NULL ) {
                    m_stRingAttributes.rand_A = cJSON_GetObjectItem(cjson, "rand_a")->valuestring;
                    BTRLEMGRLOG_INFO("The Rand_A : [%s]. \n", m_stRingAttributes.rand_A.c_str());
                }
                if ( cJSON_GetObjectItem( cjson, "session_token") != NULL ) {
                    m_stRingAttributes.session_token = cJSON_GetObjectItem(cjson, "session_token")->valuestring;
                    BTRLEMGRLOG_INFO("The Session Token : [%s]. \n", m_stRingAttributes.session_token.c_str());
                }
                if ( cJSON_GetObjectItem( cjson, "cmst_traceId") != NULL ) {
                    m_stRingAttributes.cmst_traceId = cJSON_GetObjectItem(cjson, "cmst_traceId")->valuestring;
                    BTRLEMGRLOG_INFO("The cmst_traceId : [%s]. \n", m_stRingAttributes.cmst_traceId.c_str());
                }
            }
            else if (strcasecmp(cmd,"MEP_TOA_SEND_COMMANDS") == 0) {
                char *tile_uuid= NULL;
                BTRLEMGRLOG_INFO("Received the \"MEP_TOA_SEND_COMMANDS\" request.\n");

                tile_uuid = cJSON_GetObjectItem( cjson, "tile_uuid")->valuestring;

                if(tile_uuid == NULL) {
                    BTRLEMGRLOG_ERROR("Failed to Ring for Tile, since last command for MEP_TOA_OPEN_CHANNEL is for different tile (%s). \n", m_stRingAttributes.tile_uuid.c_str());
                }
                else {
                    BTRLEMGRLOG_INFO("Got the Request to Ring the Tile [%s]. \n",tile_uuid);
                    m_stRingAttributes.oper_code.clear();
                    m_stRingAttributes.oper_code = cmd;
                    if(0 ==strncasecmp(m_stRingAttributes.tile_uuid.c_str(), tile_uuid, strlen(tile_uuid))) {
                        m_stRingAttributes.ring_state = LT_TILE_RING_RECEIVED_MEP_TOA_SEND_COMMANDS;

                        cJSON *disconnect= cJSON_GetObjectItem( cjson, "disconnect_on_completion");

                        if(disconnect!= NULL ) {
                            m_stRingAttributes.disconnect_on_completion = cJSON_IsTrue(disconnect);
                            BTRLEMGRLOG_INFO("The \'disconnect_on_completion\' is [%d]. \n", m_stRingAttributes.disconnect_on_completion);
                        }

                        cJSON * array = cJSON_GetObjectItem(cjson, "commands");
                        BTRLEMGRLOG_DEBUG("The array length = %d\n",cJSON_GetArraySize(array));

                        for (int index = 0; index < cJSON_GetArraySize(array); index++) {
                            cJSON * subitem = cJSON_GetArrayItem(array, index);
                            if(subitem != NULL) {
                                if( cJSON_GetObjectItem(subitem, "command") != NULL ) {
                                    char* cmd = cJSON_GetObjectItem(subitem, "command")->valuestring;
                                    char* resp_mask = cJSON_GetObjectItem(subitem, "response_mask")->valuestring;

                                    if(0 == index) {
                                        BTRLEMGRLOG_DEBUG("The Ring Cmd Ready is [%s].\n",cmd );
                                        m_stRingAttributes.cmd_Ready.cmd = cmd;
                                        m_stRingAttributes.cmd_Ready.rsp_mask = resp_mask;
                                    }
                                    else if(1 == index) {
                                        BTRLEMGRLOG_DEBUG("The Ring Cmd Play is [%s].\n",cmd );
                                        m_stRingAttributes.cmd_Play.cmd = cmd;
                                        m_stRingAttributes.cmd_Play.rsp_mask = resp_mask;
                                    }
                                    else {
                                        BTRLEMGRLOG_ERROR("Wrong Cmd....\n");
                                    }
                                }
                            }
                        } // end for for (
                    }
                }
            }
            else {
                BTRLEMGRLOG_ERROR("Failed, Invalid Ring Request Command. \n");
            }
        }
        /* Check for other messages.*/
        cJSON_Delete(cjson);
    }


    ui_long_long handle = retrieve_DevId_from_RingAttributes();

    if(!handle) {
        BTRLEMGRLOG_ERROR ("Failed to Retrive DeviceId, looks like Failed to discover this Request Ring Tile [%s].\n", m_stRingAttributes.tile_uuid.c_str());
        return LEMGR_RESULT_LE_FAIL_TO_DISCOVER;
    }

    if((m_stRingAttributes.oper_code.compare("MEP_TOA_OPEN_CHANNEL") == 0) && (!m_stRingAttributes.tile_uuid.empty() && handle)) {
        BTRLEMGRLOG_DEBUG ("Found the Ring Tile (uuid: %s) having device Id (%llu).\n", m_stRingAttributes.tile_uuid.c_str(), handle);

        m_stRingAttributes.ring_trigger_ts = time(nullptr);
        m_stRingAttributes.devId = handle;
        m_stRingAttributes.isRingRequested = true;
        m_stRingAttributes.ring_state = LE_TILE_RING_REQUEST_TO_CONNECT;
        m_stRingAttributes.isConnected = true;
        updateTileUUID_LeDevMap(m_stRingAttributes.devId, m_stRingAttributes.tile_uuid);

        lock.unlock();
        if(LEMGR_RESULT_SUCCESS != doConnect(handle)) {
            BTRLEMGRLOG_INFO ("Failed to Connect a tile (%llu).\n", handle);
            lock.lock();
            ret = LEMGR_RESULT_LE_CONNECT_FAIL;
            m_stRingAttributes = empty;
        }
        else {
            BTRLEMGRLOG_INFO ("Triggered Connect to a Ring tile (%llu) for MEP_TOA_OPEN_CHANNEL.\n", m_stRingAttributes.devId);
        }
    }
    else if ((m_stRingAttributes.oper_code.compare("MEP_TOA_SEND_COMMANDS") == 0) && (!m_stRingAttributes.tile_uuid.empty() && handle)) {
        /*
         * Check for Tile connection status,
         * Decode the cmd ready and create payload, then write.
         */
        if (m_stRingAttributes.isConnected &&
                (LT_TILE_RING_RECEIVED_MEP_TOA_SEND_COMMANDS == m_stRingAttributes.ring_state) &&
                (!m_stRingAttributes.cmd_Ready.cmd.empty())) {
            BTRLEMGRLOG_INFO ("Writing TOA_CMD_READY for Tile uuid: (%llu).\n", handle);
            lock.unlock();
            if(LEMGR_RESULT_SUCCESS != write_TOA_CMD_READY(handle)) {
                lock.lock();
                m_stRingAttributes = empty;
            }
        }
    }
    else {
        BTRLEMGRLOG_INFO ("Failed to Ring tile (%s) for MEP_TOA_OPEN_CHANNEL.\n", m_stRingAttributes.tile_uuid.c_str());
        m_stRingAttributes = empty;
    }

    BTRLEMGRLOG_TRACE("Exiting.. \n");
    return ret;
}


BTRLEMGR_Result_t
BtrTileMgr:: write_TOA_CMD_OPEN_CHANNEL (
    ui_long_long tile_devId
) {
    BTRLEMGRLOG_TRACE ("Entering...\n");

    std::string ToaCmdOpenChannel_Payload;

    setNotification(tile_devId, notifyUuid);

    BTRLEMGRLOG_DEBUG ("Now, forming the TOA_CMD_OPEN_CHANNEL Payload\n");

    if(LEMGR_RESULT_SUCCESS == form_TOA_CMD_OPEN_CHANNEL_Payload(ToaCmdOpenChannel_Payload)) {
        if (LEMGR_RESULT_SUCCESS == writeValue( tile_devId, writeCharUuid, ToaCmdOpenChannel_Payload)) {
            BTRLEMGRLOG_INFO ("Successfully set TOA_CMD_OPEN_CHANNEL. \n");
        }
        else {
            BTRLEMGRLOG_ERROR ("Failed to set TOA_CMD_OPEN_CHANNEL.\n");
            m_stRingAttributes = empty;
            return LEMGR_RESULT_LE_WRITE_FAILED;
        }
    }
    else {
        BTRLEMGRLOG_ERROR ("Failed to form TOA_CMD_OPEN_CHANNEL.\n");
        m_stRingAttributes = empty;
        return LEMGR_RESULT_LE_WRITE_FAILED;
    }

    BTRLEMGRLOG_TRACE ("Exiting...\n");
    return LEMGR_RESULT_SUCCESS;
}


BTRLEMGR_Result_t
BtrTileMgr::form_TOA_CMD_OPEN_CHANNEL_Payload (
    std::string& payload
) {
    unsigned char *rand_a = NULL;
    unsigned char *session_token = NULL;
    std::string hexstring;
    BTRLEMGR_Result_t ret = LEMGR_RESULT_SUCCESS;
    size_t rand_decodedlength=0;
    size_t session_token_decodedlength=0;

    BTRLEMGRLOG_TRACE("Entering..\n");

    if (!m_stRingAttributes.rand_A.empty()) {
        const char *buff = m_stRingAttributes.rand_A.c_str();
        if (btrLeMgrUtils::decodeBase64(buff,&rand_a,&rand_decodedlength)) {
            BTRLEMGRLOG_DEBUG ("Successfully Decoded Rand_A.\n");
        }
        else {
            BTRLEMGRLOG_ERROR("Failed to Decoding rand_A.\n");
        }
    }

    if (!m_stRingAttributes.session_token.empty()) {
        const char *buff = m_stRingAttributes.session_token.c_str();
        if (btrLeMgrUtils::decodeBase64(buff,&session_token,&session_token_decodedlength)) {
            BTRLEMGRLOG_DEBUG ("Successfully Decoded the Session_Token.\n");
        }
        else {
            BTRLEMGRLOG_ERROR("Failed to Decoding the Session_Token.\n");
        }
    }

    // Form TOA_CMD hex string
    hexstring.clear();

    if(rand_a && session_token) {
        BTRLEMGRLOG_DEBUG("The rand_a length => %zu & Session_token length => %zu\n",rand_decodedlength,session_token_decodedlength);
        char hexbyte[3] = {'\0'};
        hexstring += _1_BYTE_TOA_CID;

        for(unsigned int i=0; i < session_token_decodedlength ; i++) {
            sprintf(hexbyte,"%02x",session_token[i]);
            hexstring += hexbyte;
        }
        hexstring += _1_BYTE_TOA_CMD_OPEN_CHANNEL;

        for(unsigned int i =0; i < rand_decodedlength; i++) {
            sprintf(hexbyte,"%02x",rand_a[i]);
            hexstring += hexbyte;
        }

        BTRLEMGRLOG_DEBUG("The TOA CMD hex String:%s\n",hexstring.c_str());
        payload = hexstring;
    }

    if(rand_a) free(rand_a);
    if(session_token) free(session_token);

    BTRLEMGRLOG_TRACE ("Exiting...\n");
    return ret;

}


BTRLEMGR_Result_t
BtrTileMgr::send_Rand_T_Notification (
    ui_long_long devId,
    std::string repo_Rand_A
) {
    BTRLEMGRLOG_TRACE("Entering..\n");

    BTRLEMGRLOG_INFO("Now, calculate Rand_T & Channel ID. \n");
    std::string resp_ToaOpen = repo_Rand_A;
    /* This is Rand_T */
    if(resp_ToaOpen.length()) {
        /* Channel Id*/
        char channel_id[3] = {'\0'};
        std::size_t chan_id_len = resp_ToaOpen.copy((char*)channel_id,2,12);
        channel_id[chan_id_len]='\0';
        m_stRingAttributes.channel_id = channel_id;
        BTRLEMGRLOG_INFO("The channel_id is [%s]\n", channel_id);

        unsigned char rand_t[27] = {'\0'};
        /*Use last 13 hex string.*/
        if(resp_ToaOpen.copy((char*)rand_t,26,14)) {
            if(strlen((const char*)rand_t)) {
                BTRLEMGRLOG_INFO("The Rand_T is [%s]\n", rand_t);
                /* Change from hex string to binary*/
                unsigned char rand_t_b_array[14] = {'\0'};
                int byte_arr_len =0;

                btrLeMgrUtils::hexStringToByteArray((const char*) rand_t, rand_t_b_array, &byte_arr_len);

                for(int i = 0; i <= byte_arr_len; i++ ) {
                    BTRLEMGRLOG_DEBUG ("rand_t_byte_array[%d] : %02x\n", i, rand_t_b_array[i]);
                }

                //BTRLEMGRLOG_INFO("The Rand_T byte Arrary is [%s]\n", rand_t_byte_array);
                char *b64encoded_rand_t = btrLeMgrUtils::encodeBase64(rand_t_b_array, byte_arr_len);
                m_stRingAttributes.rand_T = b64encoded_rand_t;
                m_stRingAttributes.ring_state = LE_TILE_RING_RECEIVED_RAND_T;
                BTRLEMGRLOG_INFO("The Encoded Rand_T is [%s]\n", m_stRingAttributes.rand_T.c_str());

                if(b64encoded_rand_t)
                    free (b64encoded_rand_t);
            }
            else {
                BTRLEMGRLOG_ERROR("Failed to receive Rand_T.\n");
            }
        }

        if(m_stRingAttributes.channel_id.length() && m_stRingAttributes.rand_A.length()) {
            BTRLEMGRLOG_INFO("Ready to Send RAND_T Notification to the Tile Cloud\n");
            notifyTileInfoCloud(m_stRingAttributes.tile_uuid, "", LE_TILE_NOTIFY_TYPE_RING_RAND_T_RESPONSE, LE_NOTIFY_MSG_CLOUD_POST_STATUS_OK);

            m_stRingAttributes.ring_state = LE_TILE_RING_NOTIFY_RAND_T;
        }
    }
    BTRLEMGRLOG_TRACE ("Exiting...\n");
    return LEMGR_RESULT_SUCCESS;
}


/*Check for RFC flags*/
void
BtrTileMgr::checkRfcFeatureOfTileRing (
    void
) {
    std::string rfc_ring_param = "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.BLE.Tile.Ring.Enable";
    std::string ring_param_value;

    if(btrLeMgrUtils::checkRFC_ServiceAvailability (rfc_ring_param, ring_param_value)) {
        if(0 == strncasecmp(ring_param_value.c_str(), "true", sizeof("true"))) {
            m_isTileRingFeatureEnabled = true;
        }
    }

    BTRLEMGRLOG_INFO("[%s] The RFC [%s] is [%s].\n", __FUNCTION__, rfc_ring_param.c_str(), (char* )((m_isTileRingFeatureEnabled)?"Enabled":"Disabled") );
}

BTRLEMGR_Result_t
BtrTileMgr:: write_TOA_CMD_READY (
    ui_long_long tile_devId
) {
    BTRLEMGRLOG_TRACE ("Entering...\n");

    std::string ToaCmdReady_Payload;

    BTRLEMGRLOG_DEBUG ("Now, forming the TOA_CMD_READY Payload\n");

    if(LEMGR_RESULT_SUCCESS == form_TOA_CMD_READY_Payload(ToaCmdReady_Payload)) {
        if (LEMGR_RESULT_SUCCESS == writeValue( tile_devId, writeCharUuid, ToaCmdReady_Payload)) {
            BTRLEMGRLOG_INFO ("Successfully set TOA_CMD_READY. \n");
            m_stRingAttributes.ring_state = LE_TILE_RING_WRITE_MEP_TOA_COMMANDS_READY;
        }
        else {
            BTRLEMGRLOG_ERROR ("Failed to set TOA_CMD_READY.\n");
            m_stRingAttributes = empty;
            return LEMGR_RESULT_LE_WRITE_FAILED;
        }
    }
    else {
        BTRLEMGRLOG_ERROR ("Failed to form TOA_CMD_READY.\n");
        m_stRingAttributes = empty;
        return LEMGR_RESULT_LE_WRITE_FAILED;
    }

    BTRLEMGRLOG_TRACE ("Exiting...\n");
    return LEMGR_RESULT_SUCCESS;
}

BTRLEMGR_Result_t
BtrTileMgr::form_TOA_CMD_READY_Payload (
    std::string& payload
) {
    BTRLEMGR_Result_t ret = LEMGR_RESULT_SUCCESS;
    BTRLEMGRLOG_TRACE("Entering..\n");
    unsigned char* b64decoded_cmd = NULL;
    std::string hexstring;

    size_t cmdReady_decodlen=0;

    if(!m_stRingAttributes.cmd_Ready.cmd.empty() && !m_stRingAttributes.cmd_Ready.rsp_mask.empty() ) {
        const char *buff = m_stRingAttributes.cmd_Ready.cmd.c_str();

        if(btrLeMgrUtils::decodeBase64(buff,&b64decoded_cmd,&cmdReady_decodlen)) {
            BTRLEMGRLOG_DEBUG ("Successfully Decoded TOA_CMD_READY.\n");
        }
        else {
            BTRLEMGRLOG_ERROR("Failed to Decoding TOA_CMD_READY.\n");
        }
    }
    // Form TOA_CMD hex string
    hexstring.clear();

    if(b64decoded_cmd && cmdReady_decodlen) {
        BTRLEMGRLOG_DEBUG("The CMD READY Decoded length => %zu \n",cmdReady_decodlen);
        char hexbyte[3] = {'\0'};

        for(unsigned int i=0; i < cmdReady_decodlen ; i++) {
            sprintf(hexbyte,"%02x",b64decoded_cmd[i]);
            hexstring += hexbyte;
        }

        BTRLEMGRLOG_DEBUG("The TOA CMD hex String:%s\n",hexstring.c_str());
        payload = hexstring;

        free(b64decoded_cmd);
    }

    BTRLEMGRLOG_TRACE ("Exiting...\n");
    return ret;
}

BTRLEMGR_Result_t
BtrTileMgr:: write_TOA_CMD_PLAY (
    ui_long_long tile_devId
) {
    BTRLEMGRLOG_TRACE ("Entering...\n");

    std::string ToaCmdPlay_Payload;

    BTRLEMGRLOG_DEBUG ("Now, forming the TOA_CMD_PLAY Payload\n");

    if(LEMGR_RESULT_SUCCESS == form_TOA_CMD_PLAY_Payload(ToaCmdPlay_Payload)) {
        if (LEMGR_RESULT_SUCCESS == writeValue( tile_devId, writeCharUuid, ToaCmdPlay_Payload)) {
            BTRLEMGRLOG_INFO ("Successfully set TOA_CMD_PLAYL. \n");
            m_stRingAttributes.ring_state = LE_TILE_RING_WRITE_MEP_TOA_COMMANDS_PLAY;
        }
        else {
            BTRLEMGRLOG_ERROR ("Failed to set TOA_CMD_PLAY.\n");
            m_stRingAttributes = empty;
            return LEMGR_RESULT_LE_WRITE_FAILED;
        }
    }
    else {
        BTRLEMGRLOG_ERROR ("Failed to form TOA_CMD_PLAYL.\n");
        m_stRingAttributes = empty;
        return LEMGR_RESULT_LE_WRITE_FAILED;
    }

    BTRLEMGRLOG_TRACE ("Exiting...\n");
    return LEMGR_RESULT_SUCCESS;
}

BTRLEMGR_Result_t
BtrTileMgr::form_TOA_CMD_PLAY_Payload (
    std::string& payload
) {
    BTRLEMGR_Result_t ret = LEMGR_RESULT_SUCCESS;

    BTRLEMGRLOG_TRACE("Entering..\n");
    unsigned char* b64decoded_cmd = NULL;
    std::string hexstring;

    size_t cmdPlay_decodlen=0;

    if(!m_stRingAttributes.cmd_Play.cmd.empty() && !m_stRingAttributes.cmd_Play.rsp_mask.empty() ) {
        const char *buff = m_stRingAttributes.cmd_Play.cmd.c_str();

        if(btrLeMgrUtils::decodeBase64(buff,&b64decoded_cmd,&cmdPlay_decodlen)) {
            BTRLEMGRLOG_DEBUG ("Successfully Decoded TOA_CMD_PLAY.\n");
        }
        else {
            BTRLEMGRLOG_ERROR("Failed to Decoding TOA_CMD_PLAY.\n");
        }
    }
    // Form TOA_CMD hex string
    hexstring.clear();

    if(b64decoded_cmd && cmdPlay_decodlen) {
        BTRLEMGRLOG_DEBUG("The CMD PLAY length => %zu\n",cmdPlay_decodlen);
        char hexbyte[3] = {'\0'};

        for(unsigned int i=0; i < cmdPlay_decodlen ; i++) {
            sprintf(hexbyte,"%02x",b64decoded_cmd[i]);
            hexstring += hexbyte;
        }

        BTRLEMGRLOG_DEBUG("The TOA CMD hex String:%s\n",hexstring.c_str());
        payload = hexstring;

        free(b64decoded_cmd);
    }

    BTRLEMGRLOG_TRACE ("Exiting...\n");
    return ret;
}

BTRLEMGR_Result_t
BtrTileMgr::send_TOA_CMD_Response_Notification (
    ui_long_long devId
) {
    BTRLEMGRLOG_TRACE("Entering..\n");

    if(!m_stRingAttributes.cmd_Ready.rsp_notify_data.empty() && m_stRingAttributes.cmd_Play.rsp_notify_data.length()) {
        notifyTileInfoCloud(m_stRingAttributes.tile_uuid, "", LE_TILE_NOTIFY_TYPE_RING_CMD_RESPONSE, LE_NOTIFY_MSG_CLOUD_POST_STATUS_OK);
    }
    else {
        BTRLEMGRLOG_ERROR("Failed to Send TOA_CMD_Response Notification.\n");
    }
    return LEMGR_RESULT_SUCCESS;
}


BTRLEMGR_Result_t
BtrTileMgr::convert_Hex2ByteArray_n_Encode_ToaCmdRsp (
    std::string in_cmd_rsp,
    std::string& b64encoded_str
) {
    const char *cmd_Ready_resp = in_cmd_rsp.c_str();
    int len = in_cmd_rsp.length()/2;
    unsigned char cmdReady_b_array[len] = {'\0'};

    int byte_arr_len;
    btrLeMgrUtils::hexStringToByteArray((const char*) cmd_Ready_resp, cmdReady_b_array, &byte_arr_len);

    for(int i = 0; i <= byte_arr_len-1; i++ ) {
        BTRLEMGRLOG_DEBUG ("cmd_byte_array[%d] : %02x\n", i, cmdReady_b_array[i]);
    }

    char *b64encoded_cmd = btrLeMgrUtils::encodeBase64(cmdReady_b_array, byte_arr_len);
    if(b64encoded_cmd) {
        b64encoded_str = b64encoded_cmd;
        free (b64encoded_cmd);
    }

    return LEMGR_RESULT_SUCCESS;
}

void
BtrTileMgr::scrub_unreachable_devices (
    void
) {
    const int MAX_TIME_BETWEEN_DISCOVERY = MAX_LE_TILE_NOTIFY_INTERVAL + 60; //seconds
    time_t currentTime = time(nullptr);

    std::lock_guard<std::mutex> lock(m_mtxMap);
    for (auto it = m_LeDevMap.cbegin(); it != m_LeDevMap.cend(); ++it) {
        if (MAX_TIME_BETWEEN_DISCOVERY < (currentTime - it->second->m_lastSeenTimestamp)) {
            BTRLEMGRLOG_INFO ("Removing device %llu from map as it is no longer in range.\n", it->second->m_devId);

            delete it->second;
            it = m_LeDevMap.erase(it);
            if(m_LeDevMap.empty()) {
                break;
            }

            it--;
        }
    }
}

void
BtrTileMgr::start_periodic_maintenance (
    void
) {
    m_last_discovery_timestamp = std::chrono::system_clock::now();
    m_scan_timer_active = true;
}

void
BtrTileMgr::run_periodic_maintenance (
    void
) {
    const unsigned int LE_SCAN_MAX_RUNTIME = 60; //seconds
    const unsigned int LE_SCAN_MAX_DOWNTIME = 60 * 3; //seconds

    /* Start scan management code.

    LE is scan is allowed to run until there are no new discovery events for LE_SCAN_MAX_RUNTIME seconds.
    At this point, scanning is disabled for LE_SCAN_MAX_DOWNTIME, then re-enabled again. This strategy is used
    to limit the amount of time we spend running LE scan; it was observed that having LE scan active at the time
    a connect call is made increases the time needed to establish the connection by ~ 1 sec. By having scanning off
    for longer periods, we improve the chances of getting connected quickly. */
    auto current_time = std::chrono::system_clock::now();
    if (true == m_scan_timer_active) {
        if (std::chrono::duration_cast<std::chrono::seconds>(current_time - m_last_discovery_timestamp).count() >= LE_SCAN_MAX_RUNTIME) {
            BTRLEMGRLOG_DEBUG("Pausing LE scan due to no activity.\n");
            if (leScanStop() == LEMGR_RESULT_SUCCESS) {
                m_scan_stoppage_timestamp = std::chrono::system_clock::now();
                m_scan_timer_active = false;
                m_scan_resume_countdown_active = true;
            }
            else {
                m_last_discovery_timestamp = std::chrono::system_clock::now();
            }
        }
    }
    else if (true == m_scan_resume_countdown_active) {
        if (std::chrono::duration_cast<std::chrono::seconds>(current_time - m_scan_stoppage_timestamp).count() >= LE_SCAN_MAX_DOWNTIME) {
            BTRLEMGRLOG_DEBUG("Resuming LE scan to check for activity.\n");
            if (leScanStart() == LEMGR_RESULT_SUCCESS) {
                m_last_discovery_timestamp = std::chrono::system_clock::now();
                m_scan_timer_active = true;
                m_scan_resume_countdown_active = false;
            }
            else {
                m_scan_stoppage_timestamp = std::chrono::system_clock::now();
            }
        }
    }
    /* End scan management code. */

    scrub_unreachable_devices();
    //checkThresholdTimeAndReConnectNotify();
}


void BtrTileMgr::constructAdvDataPayload_HashTileId(std::string &mac, std::string &serviceData, std::string &payload) {

    /*
     * Ad Payload : Tile Mac + AD1 + AD2 + AD3 ()

      AD-1 : Length:02, Type: 01, Flag: 06
      AD-2 : Length: 03,Type: 03, ed fe
      AD-3 : Length: 0d, Type: 16, ServiceData: (Key:edfe + Value:Data )
    */
    payload.empty();
    mac.erase(std::remove(mac.begin(), mac.end(), ':'), mac.end());
    payload.append(mac);
    /* AD-1 : constant data, Flag (02) + Type(01) + Data(06)*/
    payload.append("020106");

    /* AD-2 : constant data, Flag (03) + Type(03) + Data (ed fe)*/
    payload.append("0303edfe");

    /* Service Data payload : Length + Type + ServiceData(key + Value) */
    int servDataLen = 0;
    std::string servDataType = "16";
    std::string servDatakey = "edfe";
    servDataLen = (servDataType.length() + servDatakey.length() + serviceData.length())/2;
    char srvDataLen_buff[9] = {'\0'};
    sprintf(srvDataLen_buff, "%02x", servDataLen);

    payload.append(srvDataLen_buff);
    payload.append(servDataType);

    payload.append(servDatakey);
    payload.append(serviceData);

}

BTRLEMGR_Result_t
BtrTileMgr::populateStagingDiscoveryRingURL (
    void
)
{
    std::string line;
    int index = 0;
    std::string filename = "/tmp/.stagingTileUrl";

    std::ifstream tmpfile(filename);

    if (tmpfile.is_open()){
        while ( (getline (tmpfile,line)) && (index < 2) ){
            if(index==0){
                strcpy(gStagingDiscoveryURL,line.c_str());
            }
            else{
                strcpy(gStagingRingURL,line.c_str());
            }
            index++;
        }
        tmpfile.close();
    }
    else {
        BTRLEMGRLOG_ERROR("Failed to open (%s).\n", filename.c_str());
        return LEMGR_RESULT_GENERIC_FAILURE;
    }
    return LEMGR_RESULT_SUCCESS;
}
