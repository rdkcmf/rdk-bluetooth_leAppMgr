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

/*
 * btrMgrIf.h
 * This file will be interface to BTR Mgr interface, it will call all IARM call to BTR Mgr
 *
 */

#ifndef _BTR_MGR_IF_H_
#define _BTR_MGR_IF_H_

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "btrLeMgr_type.h"
#include "btmgr.h"


typedef struct  _stBtMgrIfLeProp {
    ui_long_long           m_devId;
    bool                   m_isLeDevice;
    unsigned short         m_uuid;
    short                  m_discoveryCount;
    LEMGR_Events_t         m_eventType;
    std::string            m_name;
    std::string            m_macAddr;
    std::string            m_serviceUuid;
    std::string            m_notifyData;
    bool                   m_isServDataPresent;
    std::string            m_adServiceData;
    int                    m_signalLevel;
} stBtMgrIfLeProp;

class BtrMgrIf
{

public:
    static BtrMgrIf*    getInstance();
    static void         releaseInstance(void);

    void                shutdownIfce(void);
    bool                getLeCharProperty(ui_long_long devId, std::string &charUUID);
    bool                performLeOps(ui_long_long devId, LEMGR_Le_t methodName, const std::string& charUuid, std::string &value);
    bool                getLeDeviceList();
    bool                startLeDeviceDiscovery(std::string asUuid);
    bool                stopLeDeviceDiscovery();
    bool                doConnectLeDevice(unsigned long long int devId);
    bool                doDisconnectLeDevice(unsigned long long int devId);
    bool                writeValue(ui_long_long in_devId, const std::string in_writeCharUuid, std::string in_Value);
    BTRLEMGR_Result_t   getLimitBeaconDetection(unsigned char *isLimited);
    BTRLEMGR_Result_t   setLimitBeaconDetection(unsigned char isLimited);
    stBtMgrIfLeProp     popEvent();


private:
    static BtrMgrIf*             m_instance;

    std::queue <stBtMgrIfLeProp> m_queue;
    std::mutex                   m_mutex;
    std::condition_variable      m_condition;

    std::string                  m_serviceUuid;

    BtrMgrIf();
    ~BtrMgrIf();

    static BTRMGR_Result_t eventCallback (BTRMGR_EventMessage_t event);

    bool            isLeDev_Tile(unsigned short uuid); //Comment: This can be isLeDev, the determination of whether it is a Tile or not should be done in  BtrTileMgr or the child class of BtrLeMgrBase
    std::string&    getServiceUuid(void);
    void            setServiceUuid(unsigned short uuid);

    void            pushEvent(stBtMgrIfLeProp*);
};



#endif /* _BTR_MGR_IF_H_ */
