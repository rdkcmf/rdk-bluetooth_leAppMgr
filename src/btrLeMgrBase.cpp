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

#include "btrLeMgrBase.h"

#include "btrLeMgr_logger.h"
#include "btrLeMgr_utils.h"

BtrLeMgrBase::BtrLeMgrBase (
    std::string uuid
) {
    m_uuid = uuid;
    m_pBtrMgrIfce = BtrMgrIf::getInstance();

    if (!m_pBtrMgrIfce)
        BTRLEMGRLOG_INFO("FAILED TO CREATE BTMGR IFCE");
}

BtrLeMgrBase::~BtrLeMgrBase (
) {
    if (m_pBtrMgrIfce) {
        BtrMgrIf::releaseInstance();
        m_pBtrMgrIfce = NULL;
    }
}

BTRLEMGR_Result_t
BtrLeMgrBase::leScanStart (
    void
) {
    return (m_pBtrMgrIfce->startLeDeviceDiscovery(m_uuid) ? LEMGR_RESULT_SUCCESS : LEMGR_RESULT_LE_SCAN_FAIL);
}

BTRLEMGR_Result_t
BtrLeMgrBase::leScanStop (
    void
) {
    return (m_pBtrMgrIfce->stopLeDeviceDiscovery() ? LEMGR_RESULT_SUCCESS : LEMGR_RESULT_LE_SCAN_FAIL);
}

BTRLEMGR_Result_t
BtrLeMgrBase::doConnect (
    ui_long_long    devId
) {
    return (m_pBtrMgrIfce->doConnectLeDevice(devId) ? LEMGR_RESULT_SUCCESS : LEMGR_RESULT_LE_CONNECT_FAIL);
}

BTRLEMGR_Result_t
BtrLeMgrBase::doDisconnect (
    ui_long_long    devId
) {
    return (m_pBtrMgrIfce->doDisconnectLeDevice(devId) ? LEMGR_RESULT_SUCCESS : LEMGR_RESULT_LE_DISCONNECT_FAIL);
}

std::map <ui_long_long, stBtLeDevList*>
BtrLeMgrBase::getLeDevList (
    void
) {
    return m_LeDevList;
}

void
BtrLeMgrBase::update_beaconDetectionLimit (
    void
) {
   if (LEMGR_RESULT_SUCCESS == btrLeMgrUtils::updateLimitBeaconDetection())
      m_pBtrMgrIfce->getLimitBeaconDetection(&m_limit);
}

// Comment: Why do we expect the LeAppMgr to even call this.??
std::thread
BtrLeMgrBase::beaconDetectionThread (
    void
) {
    BTRLEMGRLOG_INFO("Starting beacon detection Thread\n");
    bQuitBeaconDetection = false;
    return (std::thread([=] {update_beaconDetectionLimit();}));
}

BTRLEMGR_Result_t
BtrLeMgrBase::setLimitBeaconDetection (
    unsigned char   isLimited
) {
    if (m_pBtrMgrIfce->setLimitBeaconDetection(isLimited) != LEMGR_RESULT_SUCCESS) {
        BTRLEMGRLOG_ERROR ("Failed to set the limitBeaconDetection\n");
        return LEMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        unsigned char   lui8IsLimited = 0;
        if (m_pBtrMgrIfce->getLimitBeaconDetection(&lui8IsLimited) == LEMGR_RESULT_SUCCESS) {
            BTRLEMGRLOG_INFO ("limitBeaconDetection : %s\n", lui8IsLimited ? "true" : "false");
            m_limit = isLimited;
        }
        return LEMGR_RESULT_SUCCESS;
    }
}

BTRLEMGR_Result_t
BtrLeMgrBase::getLimitBeaconDetection (
    unsigned char*  isLimited
) { 
    if (m_pBtrMgrIfce->getLimitBeaconDetection(isLimited) == LEMGR_RESULT_SUCCESS) {
        BTRLEMGRLOG_INFO ("limitBeaconDetection : %s\n", *isLimited ? "true" : "false");
        return LEMGR_RESULT_SUCCESS;
    }

    return  LEMGR_RESULT_GENERIC_FAILURE;
}

void
BtrLeMgrBase::quitBeaconDetection (
    void
) {
    bQuitBeaconDetection = true;
    btrLeMgrUtils::terminateBeaconDetection();
}

void
BtrLeMgrBase::clearDiscoveryList (
    void
) {
    std::map <ui_long_long, stBtLeDevList*> currLeDevList = getLeDevList();

    for(auto it = currLeDevList.begin(); it != currLeDevList.end(); ++it) {
        deleteFromDiscoveryList(it->first);
    }
}

BTRLEMGR_Result_t
BtrLeMgrBase::addToDiscoveryList (
    stBtLeDevList* stLeList
) {
    if (m_LeDevList.find(stLeList->m_devId) == m_LeDevList.end()) {
        m_LeDevList.insert({stLeList->m_devId, stLeList});
        BTRLEMGRLOG_INFO("Added to LE Discovered Devices list: device id (%llu), mac (%s), name (%s), empty (%d), count(%d),  RSSI (%d)\n",
                         stLeList->m_devId, stLeList->m_macAddr.c_str(), stLeList->m_name.c_str(), m_LeDevList.empty(), m_LeDevList.size(), stLeList->m_signalLevel);
    }
    else {
        BTRLEMGRLOG_INFO("Already in LE Discovered Devices list: device id (%llu), empty (%d), count(%d)\n", stLeList->m_devId, m_LeDevList.empty(), m_LeDevList.size());
    }

    return LEMGR_RESULT_SUCCESS;
}

bool
BtrLeMgrBase::isPresentInDiscoveryList (
    ui_long_long devId
) {
    bool found = (m_LeDevList.find(devId) != m_LeDevList.end());
    BTRLEMGRLOG_INFO("%s in LE Discovered Devices list: device id (%llu), empty (%d), count(%d)\n", found ? "Found" : "Not found", devId, m_LeDevList.empty(), m_LeDevList.size());
    return found;
}

void
BtrLeMgrBase::deleteFromDiscoveryList (
    ui_long_long devId
) {
    auto it = m_LeDevList.find(devId);
    if(it != m_LeDevList.end()) {
        delete ((stBtLeDevList *)(it->second));
        m_LeDevList.erase(devId);
        BTRLEMGRLOG_INFO("Removed from LE Discovered Devices list: device id (%llu), empty (%d), count(%d)\n", devId, m_LeDevList.empty(), m_LeDevList.size());
    }
}

BTRLEMGR_Result_t
BtrLeMgrBase::writeValue (
    ui_long_long    in_devId,
    std::string     in_writeCharUuid,
    std::string     in_value
) {
    if (m_pBtrMgrIfce->writeValue(in_devId, in_writeCharUuid, in_value)) {
        BTRLEMGRLOG_INFO ("Successfully set writeValue [%s], for characteristic uuid [%s] to device Id [%llu]. \n", in_value.c_str(), in_writeCharUuid.c_str(), in_devId);
    }
    else {
        BTRLEMGRLOG_ERROR ("Failed to writeValue.\n");
        return LEMGR_RESULT_LE_WRITE_FAILED;
    }

    return LEMGR_RESULT_SUCCESS;
}

BTRLEMGR_Result_t
BtrLeMgrBase::setNotification (
    ui_long_long    tile_devId,
    std::string     in_notifyCharUuid
) {
    std::string value;
    if(m_pBtrMgrIfce->performLeOps(tile_devId, LEMGR_OP_START_NOTIFY, in_notifyCharUuid.c_str(), value)) {
        BTRLEMGRLOG_INFO ("Successfully set notification. \n");
    }
    else {
        BTRLEMGRLOG_ERROR ("Failed to set notify.\n");
        return LEMGR_RESULT_LE_SET_NOTIFY_FAILED;
    }

    return LEMGR_RESULT_SUCCESS;
}

unsigned char
BtrLeMgrBase::isBeaconDetectionLimited (
    void
) {
    return m_limit;
}
