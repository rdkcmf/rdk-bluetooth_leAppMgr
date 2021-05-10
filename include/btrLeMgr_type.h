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


#ifndef _BTR_LE_MGR_TYPE_H_
#define _BTR_LE_MGR_TYPE_H_

#include <string>
//#include <vector>
//#include <time.h>

typedef unsigned long long int ui_long_long;

typedef enum _enBtrLeType {
    LE_TILE,
    LE_NONE
} enBtrLeType;


typedef enum _enBtrNotifyState {
    LE_NOTIFY_NONE                                             = -1,
    LE_NOTIFY_SEND_FAILED                                      = 0,
    LE_NOTIFY_SEND_SUCCESSFULLY                                = 1,
    LE_NOTIFY_MSG_CLOUD_POST_STATUS_OK                         = 200,
    LE_NOTIFY_MSG_CLOUD_POST_STATUS_BAD_REQUEST                = 400,
    LE_NOTIFY_MSG_CLOUD_POST_STATUS_NOT_FOUND                  = 404,
    LE_NOTIFY_MSG_CLOUD_POST_STATUS_INTERNAL_SERVER_ERROR      = 500,
} enBtrNotifyState;


typedef enum _BTRLLMGR_Result_t {
    LEMGR_RESULT_SUCCESS = 0,
    LEMGR_RESULT_GENERIC_FAILURE          = -1,
    LEMGR_RESULT_INVALID_INPUT            = -2,
    LEMGR_RESULT_INIT_FAILED              = -3,
    LEMGR_RESULT_LE_SCAN_FAIL             = -4,
    LEMGR_RESULT_LE_CONNECT_FAIL          = -5,
    LEMGR_RESULT_LE_CHAR_UUID_FAIL        = -6,
    LEMGR_RESULT_LE_METHOD_OPERATION_FAIL = -7,
    LEMGR_RESULT_LE_DISCONNECT_FAIL       = -8,
    LEMGR_RESULT_LE_RING_FAIL             = -9,
    LEMGR_RESULT_LE_WRITE_FAILED          = -10,
    LEMGR_RESULT_LE_SET_NOTIFY_FAILED     = -11,
    LEMGR_RESULT_LE_MSG_DECODED           = -12,
    LEMGR_RESULT_LE_FAIL_TO_DISCOVER      = -13,
    LEMGR_RESULT_LE_RING_FAIL_INPROGRESS  = -14,
} BTRLEMGR_Result_t;


//typedef struct _stBtrLeAdvData {
//    ui_long_long           m_devId;
//    vector<string>         m_service_uuids;
//    vector<string>         m_solicit_uuids;
//    vector<string>         m_manufacturer_data;
//    vector<string>         m_service_data;
//    string                 m_macAddr;
//    string                 m_local_name;
//    time_t                 m_advertiseTimestamp;
//    string                 m_uuid;
//} stBtrLeAdvData;


typedef enum _LEMGR_Events_t {
    LEMGR_EVENT_DEVICE_OUT_OF_RANGE = 0,
    LEMGR_EVENT_DEVICE_DISCOVERY_STARTED,
    LEMGR_EVENT_DEVICE_DISCOVERY_UPDATE,
    LEMGR_EVENT_DEVICE_DISCOVERY_COMPLETE,
    LEMGR_EVENT_DEVICE_PAIRING_COMPLETE,
    LEMGR_EVENT_DEVICE_UNPAIRING_COMPLETE,
    LEMGR_EVENT_DEVICE_CONNECTION_COMPLETE,
    LEMGR_EVENT_DEVICE_DISCONNECT_COMPLETE,
    LEMGR_EVENT_DEVICE_PAIRING_FAILED,
    LEMGR_EVENT_DEVICE_UNPAIRING_FAILED,
    LEMGR_EVENT_DEVICE_CONNECTION_FAILED,
    LEMGR_EVENT_DEVICE_DISCONNECT_FAILED,
    LEMGR_EVENT_DEVICE_FOUND,
    LEMGR_EVENT_DEVICE_OP_READY,
    LEMGR_EVENT_DEVICE_NOTIFICATION,
    LEMGR_EVENT_MAX
} LEMGR_Events_t;

typedef enum _LEMGR_Op_t {
    LEMGR_OP_INVALID_METHOD = -1,
    LEMGR_OP_READ_VALUE,
    LEMGR_OP_WRITE_VALUE,
    LEMGR_OP_START_NOTIFY,
    LEMGR_OP_STOP_NOTIFY
} LEMGR_Le_t;

typedef struct  _stBtLeDevList {
    ui_long_long           m_devId;
    std::string            m_name;
    bool                   m_isLeDevice;
    std::string            m_macAddr;
    std::string            m_uuid;
    short                  m_discoveryCount;
    LEMGR_Events_t         m_eventType;
    std::string            m_serviceUuid;
    std::string            m_notifyMsg;
    bool                   m_isServDataPresent;
    std::string            m_advSerData;
    int                    m_signalLevel;
} stBtLeDevList;


#endif /* _BTR_LE_MGR_TYPE_H_ */
