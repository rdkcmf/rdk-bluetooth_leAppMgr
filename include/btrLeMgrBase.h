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

#ifndef _BTR_LE_MGR_H_
#define _BTR_LE_MGR_H_

#include <string>
#include <thread>
#include <time.h>
#include <ctime>
#include <cerrno>
#include <map>

#include "btrLeMgr_type.h"
#include "btrMgrIf.h"


// Comment: Why do we need a Class just for data, wouldn't a structure be sufficient ?
class BtrLeDevProp {
public:
    BtrLeDevProp() {
        //BTRLEMGRLOG_INFO("constructed.. >>> \n");
    }
    ~ BtrLeDevProp() {
        //BTRLEMGRLOG_INFO("destructed..<<<< \n");
    }
    ui_long_long           m_devId;
    std::string            m_name;
    bool                   m_isLeDevice;
    std::string            m_macAddr;
    enBtrNotifyState       m_notifyState;
    time_t                 m_lastNotifyTimestamp;
    time_t                 m_lastConnectTimestamp;
    time_t                 m_lastSeenTimestamp;
    std::string            m_uuid;
    short                  m_discoveryCount;
    LEMGR_Events_t         m_eventType;
    std::string            m_serviceUuid;
    std::string            m_readValue;
    std::string            m_sericeData;
    std::string            m_current_HashID;
};


class BtrLeMgrBase {
public:
    BtrLeMgrBase(std::string uuid);
    virtual                ~BtrLeMgrBase();

    BTRLEMGR_Result_t      leScanStart();
    BTRLEMGR_Result_t      leScanStop();

    std::map <ui_long_long, stBtLeDevList*>  getLeDevList();
    BTRLEMGR_Result_t      getLeProperty(ui_long_long devId, BtrLeDevProp &leProp);

    BTRLEMGR_Result_t      doConnect(ui_long_long devId);
    BTRLEMGR_Result_t      doDisconnect(ui_long_long devId);

    std::thread            beaconDetectionThread(void);
    BTRLEMGR_Result_t      getLimitBeaconDetection(unsigned char *isLimited);
    BTRLEMGR_Result_t      setLimitBeaconDetection(unsigned char isLimited);
    void                   quitBeaconDetection(void);

    void                   clearDiscoveryList(void);
    BTRLEMGR_Result_t      addToDiscoveryList(stBtLeDevList *leDevMap);
    bool                   isPresentInDiscoveryList(ui_long_long devId);
    void                   deleteFromDiscoveryList(ui_long_long devId);

    // Comment: These are the functions we want in implementation as part of every child class
    // These should be made pure virtual as only child classes can decide what to read and what to write
    BTRLEMGR_Result_t      writeValue(ui_long_long in_devId, std::string in_writeCharUuid, std::string in_value);
    BTRLEMGR_Result_t      setNotification(ui_long_long tile_devId, std::string in_notifyCharUuid);
    std::string            readValue();

    // Comment: Shouldn't be part of the BaseClass
    // BTRLEMGR_Result_t   setAdvertise(stBtrLeDevProp & adv);

protected:
    BtrMgrIf*              m_pBtrMgrIfce;
    unsigned char          isBeaconDetectionLimited(void);
    std::map <ui_long_long, stBtLeDevList*> m_LeDevList;


private:
    std::string            m_uuid;
    unsigned char          m_limit;

    bool                   bQuitBeaconDetection;
    void                   update_beaconDetectionLimit();

};


#endif /* _BTR_LE_MGR_H_ */

