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


#ifndef _BTR_TILE_MGR_H_
#define _BTR_TILE_MGR_H_

#include <string>
#include <thread>
#include <map>
#include <mutex>
#include <list>
#include <condition_variable>
#include <chrono>
#include "cJSON.h"

#include "btrLeMgrBase.h"
#include "btrLeMgr_type.h"


#define LE_TILE_SCAN_CHECK_SLEEP_INTERVAL  15


/* This message type conveys which endpoint to the Post in
 * json message to cloud
 */
typedef enum _enBtrLeTileNotifyType {
    LE_TILE_NOTIFY_TYPE_NONE = 0,
    LE_TILE_NOTIFY_TYPE_DISCOVERY_ALERT,            /* Discovered Msg Notification Type */
    LE_TILE_NOTIFY_TYPE_RING_REQUEST,               /* Notify Ring  Msg Notification Type */
    LE_TILE_NOTIFY_TYPE_RING_RAND_T_RESPONSE,         /* Notify Ring  Rand_T Msg Notification Type */
    LE_TILE_NOTIFY_TYPE_RING_CMD_RESPONSE,         /* Notify Ring  Rand_T Msg Notification Type */
} enBtrLeTileNotifyType;

typedef enum _enBtrLeTileRingState {
    LE_TILE_RING_REQUEST_FAILED       = -1,
    LE_TILE_RING_REQUEST_NO           = 0,
    LE_TILE_RING_REQUEST_INITIATED,
    LE_TILE_RING_REQUEST_TO_CONNECT,
    LE_TILE_RING_REQUEST_CONNECTED,
    LE_TILE_RING_SET_NOTIFY,
    LE_TILE_RING_WRITE_MEP_TOA_OPEN_CHANNEL,        /* Discovered Msg Notification Type */
    LE_TILE_RING_WRITE_MEP_TOA_OPENNED,             /* Discovered Msg Notification Type */
    LE_TILE_RING_RECEIVED_RAND_T,
    LE_TILE_RING_NOTIFY_RAND_T,                     /* Discovered Msg Notification Type */
    LT_TILE_RING_RECEIVED_MEP_TOA_SEND_COMMANDS,
    LE_TILE_RING_WRITE_MEP_TOA_COMMANDS_READY,      /* Discovered Msg Notification Type */
    LE_TILE_RING_NOTIFY_MEP_TOA_COMMANDS_READY_RESPONSE,
    LE_TILE_RING_WRITE_MEP_TOA_COMMANDS_PLAY,
    LE_TILE_RING_NOTIFY_MEP_TOA_COMMANDS_PLAY_RESPONSE,
    LE_TILE_RING_DONE,                              /* Discovered Msg Notification Type */
} enBtrLeTileRingState;

typedef struct _stRingCmds {
    std::string cmd;
    std::string rsp_mask;
    std::string rsp_notify_data;
} stRingCmds;

typedef struct _stRingAttributes {
    bool                 isRingRequested;
    bool                 isConnected;
    std::string          tile_uuid;
    std::string          mac;
    ui_long_long         devId;
    std::string          oper_code;
    std::string          session_token;
    std::string          rand_A;
    std::string          channel_id;
    std::string          rand_T;
    enBtrLeTileRingState ring_state;
    stRingCmds           cmd_Ready;
    stRingCmds           cmd_Play;
    bool                 disconnect_on_completion;
    time_t               ring_trigger_ts;
    time_t               cmst_cpe_to_ctm_ts;
    time_t               cmst_ctm_from_cpe_ts;
    std::string          cmst_traceId;
    int                  cmst_code;
    std::string          current_HashID;
    std::vector <std::string> received_HashIDs;
} stRingAttributes;

/* Defined Tile Le Mgr */
class BtrTileMgr :public BtrLeMgrBase {

public:
    virtual ~BtrTileMgr();
    BTRLEMGR_Result_t          getLeProperty(ui_long_long devId, BtrLeDevProp &leProp);
    std::thread                eventListenerThread(void);
    void                       eventListenerQuit(bool abQuitThread);
    void                       print_LeDevPropMap(void);
    BTRLEMGR_Result_t          doRingATile(std::string tileId, std::string session);
    BTRLEMGR_Result_t          processTileCmdRequest(std::string request);
    void                       start_periodic_maintenance(void);
    void                       run_periodic_maintenance(void);

    static BtrTileMgr*         getInstance(void);
    static void                releaseInstance(void);

private:
    static BtrTileMgr* instance;
    static std::string         m_sessionIdOfRingTrigger;
    static bool                m_isTile_Ring_in_progress;
    static bool                m_isTileRingFeatureEnabled;

    bool                       m_bQuitThread;
    std::map <ui_long_long, BtrLeDevProp*> m_LeDevMap;
    std::mutex                 m_mtxMap;
    std::thread                m_request_processor;
    std::list <std::string>    m_request_queue;
    std::condition_variable    m_request_q_cond;
    std::mutex                 m_request_mutex;
    bool                       m_run_request_processor;
    stRingAttributes           m_stRingAttributes;
    bool                       m_scan_timer_active;
    bool                       m_scan_resume_countdown_active;
    std::chrono::time_point<std::chrono::system_clock> m_scan_stoppage_timestamp;
    std::chrono::time_point<std::chrono::system_clock> m_last_discovery_timestamp;

    BtrTileMgr(std::string uuid);

    void                       receiver_LeEventQ(void);  /* Event listening thread */
    bool                       check_LeDevThrottleTimeInMap(ui_long_long devId);
    /* Adding discovered Tile device to Hash Map */
    BTRLEMGR_Result_t          add_LeDevPropToMap(ui_long_long devId, const char* devMac, const char* name, const char* readVal, std::string srvData="",std::string current_hashID="");
    bool                       is_LeDevPresentInMap(ui_long_long devId);
    void                       delete_LeDevMap (void);
    void                       delete_LeDevMap (ui_long_long devId);
    void                       updateTimestamp_LeDevMap(ui_long_long devId, enBtrNotifyState state, bool update_connect_ts);
    void                       updateTileUUID_LeDevMap (ui_long_long devId, std::string tile_uuid);
    void                       updateTileServiceData_LeDevMap (ui_long_long devId, std::string serviceData);
    BTRLEMGR_Result_t          getTileId(ui_long_long devId, std::string& tileId);
    BTRLEMGR_Result_t          getTileIdCore (ui_long_long devId, std::string& tileId);
    enBtrNotifyState           notifyTileInfoCloud(std::string const &tileId, std::string const &sessionID, enBtrLeTileNotifyType notifyType,  enBtrNotifyState status, std::string mac="", std::string srvData="", int rssi=0);
    std::string                retrieve_TileId_from_DevId(ui_long_long devId);
    ui_long_long               retrieve_DevId_from_RingAttributes(void);
    BTRLEMGR_Result_t          write_TOA_CMD_OPEN_CHANNEL(ui_long_long tile_devId);
    BTRLEMGR_Result_t          form_TOA_CMD_OPEN_CHANNEL_Payload(std::string& payload);
    BTRLEMGR_Result_t          write_TOA_CMD_READY(ui_long_long tile_devId);
    BTRLEMGR_Result_t          form_TOA_CMD_READY_Payload(std::string& payload);
    BTRLEMGR_Result_t          write_TOA_CMD_PLAY(ui_long_long tile_devId);
    BTRLEMGR_Result_t          form_TOA_CMD_PLAY_Payload(std::string& payload);
    BTRLEMGR_Result_t          send_Rand_T_Notification(ui_long_long tile_devId, std::string repo_Rand_A);
    BTRLEMGR_Result_t          send_TOA_CMD_Response_Notification(ui_long_long tile_devId);
    BTRLEMGR_Result_t          convert_Hex2ByteArray_n_Encode_ToaCmdRsp(std::string in_cmd_rsp, std::string& b64encoded_str);
    void                       request_processor(void);
    static void                checkRfcFeatureOfTileRing(void);
    void                       do_webpa_notify (const char* jsonPkt);
    BTRLEMGR_Result_t          checkTileCmdRequest (const std::string &request);
    BTRLEMGR_Result_t          processTileCmdRequest_stage2(const std::string &request);
    void                       checkThresholdTimeAndReConnectNotify(void);
    void                       scrub_unreachable_devices(void);
    void                       constructAdvDataPayload_HashTileId(std::string &mac, std::string &serviceData, std::string &payload);
    BTRLEMGR_Result_t          populateStagingDiscoveryRingURL(void);
};

#endif /* _BTR_TILE_MGR_H_ */
