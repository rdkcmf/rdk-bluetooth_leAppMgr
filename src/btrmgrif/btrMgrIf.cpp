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

#include <string.h>
#include <string>
#include <unistd.h>

#include "btrMgrIf.h"
#include "btmgr.h"
#include "btrLeMgr_logger.h"
#include "btrLeMgr_utils.h"

BtrMgrIf* BtrMgrIf::m_instance = NULL;

BTRMGR_Result_t
BtrMgrIf::eventCallback (
    BTRMGR_EventMessage_t   event
) {
    BTRLEMGRLOG_DEBUG ("\t[%s] EventCallback :: EvtID %d\n", BTRMGR_GetDeviceTypeAsString(event.m_pairedDevice.m_deviceType)
                       , event.m_eventType);
    switch(event.m_eventType) {
    case BTRMGR_EVENT_DEVICE_FOUND:
        BTRLEMGRLOG_INFO("\tReceived BTRMGR_EVENT_DEVICE_FOUND Event from BTRMgr\n");
        BTRLEMGRLOG_INFO("\tYour device %s is Up and Ready\n", event.m_pairedDevice.m_name);
        break;

    case BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE:

        BTRLEMGRLOG_INFO ("\tReceived DEVICE_DISCOVERY_UPDATE Event for \"%s\" (%s). \n", event.m_discoveredDevice.m_name, event.m_discoveredDevice.m_deviceAddress);

        if (event.m_discoveredDevice.m_isLowEnergyDevice) {

            BTRLEMGRLOG_DEBUG ("\t DevHandle   %lld\n", event.m_discoveredDevice.m_deviceHandle);
            BTRLEMGRLOG_DEBUG ("\t DevName     %s\n",   event.m_discoveredDevice.m_name);
            BTRLEMGRLOG_DEBUG ("\t isLeDevice  %d\n",   event.m_discoveredDevice.m_isLowEnergyDevice);
            BTRLEMGRLOG_DEBUG ("\t DevAddr     %s\n",   event.m_discoveredDevice.m_deviceAddress);
            BTRLEMGRLOG_DEBUG ("\t RSSI signalLevel     %d\n", event.m_discoveredDevice.m_signalLevel);

            /*
             * Check Device Properties & check for respective device
             */
            BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
            BTRMGR_DevicesProperty_t deviceProperty;
            memset (&deviceProperty, 0, sizeof(deviceProperty));
            std::string servData;

            rc = BTRMGR_GetDeviceProperties(0, event.m_discoveredDevice.m_deviceHandle, &deviceProperty);
            if (BTRMGR_RESULT_SUCCESS != rc)
                BTRLEMGRLOG_ERROR ("failed\n");
            else {

                for (int i = 0; i < deviceProperty.m_serviceInfo.m_numOfService; i++) {

                    unsigned short  uuid = deviceProperty.m_serviceInfo.m_profileInfo[i].m_uuid;

                    if(m_instance->isLeDev_Tile(uuid)) {
                        BTRLEMGRLOG_INFO("Discovered Tile(%llu) and Mac address (%s), deviceProperty.m_adServiceData=%p,\
                                deviceProperty.m_adServiceData[%d].m_len (%d)\n\n", event.m_discoveredDevice.m_deviceHandle,\
                                event.m_discoveredDevice.m_deviceAddress, deviceProperty.m_adServiceData, i, \
                                deviceProperty.m_adServiceData[i].m_len);

                        m_instance->setServiceUuid(uuid);

                        stBtMgrIfLeProp stTileProp;
                        {
                            stTileProp.m_name.clear();
                            stTileProp.m_macAddr.clear();
                            stTileProp.m_serviceUuid.clear();
                            stTileProp.m_notifyData.clear();
                        }
                        {
                            stTileProp.m_devId      = event.m_discoveredDevice.m_deviceHandle;
                            stTileProp.m_macAddr    = event.m_discoveredDevice.m_deviceAddress;
                            stTileProp.m_name       = event.m_discoveredDevice.m_name;
                            stTileProp.m_isLeDevice = event.m_discoveredDevice.m_isLowEnergyDevice;
                            stTileProp.m_eventType  = LEMGR_EVENT_DEVICE_DISCOVERY_UPDATE;
                            stTileProp.m_uuid       = deviceProperty.m_serviceInfo.m_profileInfo[i].m_uuid;
                            stTileProp.m_serviceUuid=m_instance->getServiceUuid();
                            BTRLEMGRLOG_DEBUG("This is Tile device. - %s\n", stTileProp.m_serviceUuid.c_str());
                            stTileProp.m_signalLevel = event.m_discoveredDevice.m_signalLevel;
                            stTileProp.m_isServDataPresent = false;/*Initial value, wil be overwritten when servData is present*/
                            for(int k=0;k<BTRMGR_MAX_DEVICE_PROFILE;k++)
                            {
                                if(0 != deviceProperty.m_adServiceData[k].m_len)
                                {
                                    char buffer [3];
                                    for(unsigned int j = 0; j < deviceProperty.m_adServiceData[k].m_len; j++)
                                    {
                                        sprintf( buffer,("%02x"),deviceProperty.m_adServiceData[k].m_ServiceData[j]);
                                        stTileProp.m_adServiceData.append(buffer);
                                        memset( buffer, 0, 3*sizeof(char));
                                    }

                                    BTRLEMGRLOG_INFO("Tile (%s) having ServiceData [ %s]\n\n", stTileProp.m_macAddr.c_str(), stTileProp.m_adServiceData.c_str());
                                    servData = stTileProp.m_adServiceData;
                                    stTileProp.m_isServDataPresent = true;
                                }
                            }
                        }
                        m_instance->pushEvent(&stTileProp);
                    }
                }
            }
            BTRLEMGRLOG_DEBUG ("\t ServiceData     %s\n",  (servData.length())?servData.c_str():"Not Present");
        }
        break;
    case BTRMGR_EVENT_DEVICE_DISCOVERY_STARTED:
        BTRLEMGRLOG_INFO ("\tReceived BTRMGR_EVENT_DEVICE_DISCOVERY_STARTED\n");
        {
            stBtMgrIfLeProp stTileProp;
            {
                stTileProp.m_name.clear();
                stTileProp.m_macAddr.clear();
                stTileProp.m_serviceUuid.clear();
                stTileProp.m_notifyData.clear();
            }
            {
                stTileProp.m_devId  = 0;
                stTileProp.m_eventType = LEMGR_EVENT_DEVICE_DISCOVERY_STARTED;
            }
            m_instance->pushEvent(&stTileProp);
        }
        break;
    case BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE:
        BTRLEMGRLOG_INFO ("\tReceived BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE\n");
        {
            stBtMgrIfLeProp stTileProp;
            {
                stTileProp.m_name.clear();
                stTileProp.m_macAddr.clear();
                stTileProp.m_serviceUuid.clear();
                stTileProp.m_notifyData.clear();
            }
            {
                stTileProp.m_devId  = 0;
                stTileProp.m_eventType = LEMGR_EVENT_DEVICE_DISCOVERY_COMPLETE;
            }
            m_instance->pushEvent(&stTileProp);
        }
        break;
    case BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE:
        BTRLEMGRLOG_INFO ("\tReceived %s DEVICE_CONNECTION_COMPLETE Event from BTRMgr for (%s). \n", event.m_pairedDevice.m_name, event.m_pairedDevice.m_deviceAddress);
        if (event.m_pairedDevice.m_isLowEnergyDevice) {

            BTRLEMGRLOG_DEBUG ("\tDevHandle   %lld\n",  event.m_pairedDevice.m_deviceHandle);
            BTRLEMGRLOG_DEBUG ("\tDevName     %s\n",    event.m_pairedDevice.m_name);
            BTRLEMGRLOG_DEBUG ("\tisLeDevice  %d\n",    event.m_pairedDevice.m_isLowEnergyDevice);
            BTRLEMGRLOG_DEBUG ("\tDevAddr     %s\n",    event.m_pairedDevice.m_deviceAddress);

            stBtMgrIfLeProp stTileProp;
            {
                stTileProp.m_name.clear();
                stTileProp.m_macAddr.clear();
                stTileProp.m_serviceUuid.clear();
                stTileProp.m_notifyData.clear();
            }
            {
                stTileProp.m_devId      = event.m_pairedDevice.m_deviceHandle;
                stTileProp.m_macAddr    = event.m_pairedDevice.m_deviceAddress;
                stTileProp.m_name       = event.m_pairedDevice.m_name;
                stTileProp.m_isLeDevice = event.m_pairedDevice.m_isLowEnergyDevice;
                stTileProp.m_signalLevel = event.m_discoveredDevice.m_signalLevel;
                stTileProp.m_eventType  = LEMGR_EVENT_DEVICE_CONNECTION_COMPLETE;
            }
            m_instance->pushEvent(&stTileProp);
        }
        break;
    case BTRMGR_EVENT_DEVICE_DISCONNECT_COMPLETE:
        BTRLEMGRLOG_INFO ("\tReceived %s DEVICE_DISCONNECT_COMPLETE Event from BTRMgr for (%s). \n", event.m_pairedDevice.m_name, event.m_pairedDevice.m_deviceAddress);
        if (event.m_pairedDevice.m_isLowEnergyDevice) {

            BTRLEMGRLOG_DEBUG ("\tDevHandle   %lld\n",  event.m_pairedDevice.m_deviceHandle);
            BTRLEMGRLOG_DEBUG ("\tDevName     %s\n",    event.m_pairedDevice.m_name);
            BTRLEMGRLOG_DEBUG ("\tisLeDevice  %d\n",    event.m_pairedDevice.m_isLowEnergyDevice);
            BTRLEMGRLOG_DEBUG ("\tDevAddr     %s\n",    event.m_pairedDevice.m_deviceAddress);

            stBtMgrIfLeProp stTileProp;
            {
                stTileProp.m_name.clear();
                stTileProp.m_macAddr.clear();
                stTileProp.m_serviceUuid.clear();
                stTileProp.m_notifyData.clear();
            }
            {
                stTileProp.m_devId      = event.m_pairedDevice.m_deviceHandle;
                stTileProp.m_macAddr    = event.m_pairedDevice.m_deviceAddress;
                stTileProp.m_name       = event.m_pairedDevice.m_name;
                stTileProp.m_isLeDevice = event.m_pairedDevice.m_isLowEnergyDevice;
                stTileProp.m_signalLevel= event.m_discoveredDevice.m_signalLevel;
                stTileProp.m_eventType  = LEMGR_EVENT_DEVICE_DISCONNECT_COMPLETE;
            }
            m_instance->pushEvent(&stTileProp);
        }
        break;
    case BTRMGR_EVENT_DEVICE_CONNECTION_FAILED:
        BTRLEMGRLOG_INFO("\tReceived %s DEVICE_CONNECTION_FAILED %d Event from BTRMgr for (%s). \n", event.m_pairedDevice.m_name, event.m_eventType, event.m_pairedDevice.m_deviceAddress);
        if (event.m_pairedDevice.m_isLowEnergyDevice) {

            BTRLEMGRLOG_INFO ("\tReceived BTRMGR_EVENT_DEVICE_CONNECTION_FAILED Event for \"%s\" (%s). \n", event.m_pairedDevice.m_name, event.m_pairedDevice.m_deviceAddress);

            BTRLEMGRLOG_DEBUG ("\tDevHandle   %lld\n",  event.m_pairedDevice.m_deviceHandle);
            BTRLEMGRLOG_DEBUG ("\tDevName     %s\n",    event.m_pairedDevice.m_name);
            BTRLEMGRLOG_DEBUG ("\tisLeDevice  %d\n",    event.m_pairedDevice.m_isLowEnergyDevice);
            BTRLEMGRLOG_DEBUG ("\tDevAddr     %s\n",    event.m_pairedDevice.m_deviceAddress);

            stBtMgrIfLeProp stTileProp;
            {
                stTileProp.m_name.clear();
                stTileProp.m_macAddr.clear();
                stTileProp.m_serviceUuid.clear();
                stTileProp.m_notifyData.clear();
            }
            {
                stTileProp.m_devId      = event.m_pairedDevice.m_deviceHandle;
                stTileProp.m_macAddr    = event.m_pairedDevice.m_deviceAddress;
                stTileProp.m_name       = event.m_pairedDevice.m_name;
                stTileProp.m_isLeDevice = event.m_pairedDevice.m_isLowEnergyDevice;
                stTileProp.m_signalLevel= event.m_discoveredDevice.m_signalLevel;
                stTileProp.m_eventType  = LEMGR_EVENT_DEVICE_CONNECTION_FAILED;
            }
            m_instance->pushEvent(&stTileProp);
        }
        break;
    case BTRMGR_EVENT_DEVICE_DISCONNECT_FAILED:
        BTRLEMGRLOG_INFO("\tReceived %s DEVICE_DISCONNECT_FAILED %d Event from BTRMgr for (%s). \n", event.m_pairedDevice.m_name, event.m_eventType, event.m_pairedDevice.m_deviceAddress);
        if (event.m_pairedDevice.m_isLowEnergyDevice) {

            BTRLEMGRLOG_INFO ("\tReceived BTRMGR_EVENT_DEVICE_DISCONNECT_FAILED Event for \"%s\" (%s). \n", event.m_pairedDevice.m_name, event.m_pairedDevice.m_deviceAddress);

            BTRLEMGRLOG_DEBUG ("\tDevHandle   %lld\n",  event.m_pairedDevice.m_deviceHandle);
            BTRLEMGRLOG_DEBUG ("\tDevName     %s\n",    event.m_pairedDevice.m_name);
            BTRLEMGRLOG_DEBUG ("\tisLeDevice  %d\n",    event.m_pairedDevice.m_isLowEnergyDevice);
            BTRLEMGRLOG_DEBUG ("\tDevAddr     %s\n",    event.m_pairedDevice.m_deviceAddress);

            stBtMgrIfLeProp stTileProp;
            {
                stTileProp.m_name.clear();
                stTileProp.m_macAddr.clear();
                stTileProp.m_serviceUuid.clear();
                stTileProp.m_notifyData.clear();
            }
            {
                stTileProp.m_devId      = event.m_pairedDevice.m_deviceHandle;
                stTileProp.m_macAddr    = event.m_pairedDevice.m_deviceAddress;
                stTileProp.m_name       = event.m_pairedDevice.m_name;
                stTileProp.m_isLeDevice = event.m_pairedDevice.m_isLowEnergyDevice;
                stTileProp.m_eventType = LEMGR_EVENT_DEVICE_DISCONNECT_FAILED;
            }
            m_instance->pushEvent(&stTileProp);
        }
        break;
    case BTRMGR_EVENT_DEVICE_OUT_OF_RANGE:
        BTRLEMGRLOG_INFO("\tReceived BTRMGR_EVENT_DEVICE_OUT_OF_RANGE  Event for \"%s\".\n", event.m_discoveredDevice.m_deviceAddress);
        /*
        if (event.m_pairedDevice.m_isLowEnergyDevice) {

            BTRLEMGRLOG_INFO ("\tReceived BTRMGR_EVENT_DEVICE_OUT_OF_RANGE Event for \"%s\" (%s). \n", event.m_pairedDevice.m_name, event.m_pairedDevice.m_deviceAddress);

            BTRLEMGRLOG_DEBUG ("\tDevHandle   %lld\n",  event.m_pairedDevice.m_deviceHandle);
            BTRLEMGRLOG_DEBUG ("\tDevName     %s\n",    event.m_pairedDevice.m_name);
            BTRLEMGRLOG_DEBUG ("\tisLeDevice  %d\n",    event.m_pairedDevice.m_isLowEnergyDevice);
            BTRLEMGRLOG_DEBUG ("\tDevAddr     %s\n",    event.m_pairedDevice.m_deviceAddress);

            stBtMgrIfLeProp stTileProp;
            memset(&stTileProp,0,sizeof(stTileProp));
            {
                stTileProp.m_devId      = event.m_pairedDevice.m_deviceHandle;
                stTileProp.m_macAddr    = event.m_pairedDevice.m_deviceAddress;
                stTileProp.m_name       = event.m_pairedDevice.m_name;
                stTileProp.m_isLeDevice = event.m_pairedDevice.m_isLowEnergyDevice;
                stTileProp.m_eventType = LEMGR_EVENT_DEVICE_OUT_OF_RANGE;
            }
            m_instance->pushEvent(&stTileProp);
        } */
        break;
    case BTRMGR_EVENT_DEVICE_OP_READY:
        if(BTRMGR_DEVICE_TYPE_TILE == event.m_deviceOpInfo.m_deviceType) {

            if(strlen(event.m_deviceOpInfo.m_notifyData)) {
                BTRLEMGRLOG_INFO ("\tReceived BTRMGR_EVENT_DEVICE_OP_READY Event for \"%s\" (%llu), Value : [%s]. \n", event.m_deviceOpInfo.m_name, event.m_deviceOpInfo.m_deviceHandle, event.m_deviceOpInfo.m_notifyData);
                stBtMgrIfLeProp stTileProp;
                {
                    stTileProp.m_name.clear();
                    stTileProp.m_macAddr.clear();
                    stTileProp.m_serviceUuid.clear();
                    stTileProp.m_notifyData.clear();
                }
                {
                    stTileProp.m_devId      = event.m_deviceOpInfo.m_deviceHandle;
                    // stTileProp.m_macAddr = event.m_deviceOpInfo.m_deviceAddress;
                    stTileProp.m_name       = event.m_deviceOpInfo.m_name;
                    stTileProp.m_notifyData = event.m_deviceOpInfo.m_notifyData;
                    stTileProp.m_eventType  = LEMGR_EVENT_DEVICE_OP_READY;
                }
                m_instance->pushEvent(&stTileProp);
            }
            else {
                BTRLEMGRLOG_ERROR ("\tReceived BTRMGR_EVENT_DEVICE_OP_READY Event for \"%s\" (%llu) with Empty Value. \n", event.m_deviceOpInfo.m_name, event.m_deviceOpInfo.m_deviceHandle);
            }
        }
        break;
    case BTRMGR_EVENT_DEVICE_OP_INFORMATION:
        if(BTRMGR_DEVICE_TYPE_TILE == event.m_deviceOpInfo.m_deviceType) {

            if(strlen(event.m_deviceOpInfo.m_notifyData)) {
                BTRLEMGRLOG_INFO ("\tReceived BTRMGR_EVENT_DEVICE_OP_INFORMATION Event for \"%s\" (%llu), Value : [%s]. \n", event.m_deviceOpInfo.m_name, event.m_deviceOpInfo.m_deviceHandle, event.m_deviceOpInfo.m_notifyData);
                stBtMgrIfLeProp stTileProp;
                {
                    stTileProp.m_name.clear();
                    stTileProp.m_macAddr.clear();
                    stTileProp.m_serviceUuid.clear();
                    stTileProp.m_notifyData.clear();
                }
                {
                    stTileProp.m_devId      = event.m_deviceOpInfo.m_deviceHandle;
                    // stTileProp.m_macAddr = event.m_deviceOpInfo.m_deviceAddress;
                    stTileProp.m_name       = event.m_deviceOpInfo.m_name;
                    stTileProp.m_notifyData = event.m_deviceOpInfo.m_notifyData;
                    stTileProp.m_eventType  = LEMGR_EVENT_DEVICE_NOTIFICATION;
                }
                m_instance->pushEvent(&stTileProp);
            }
            else {
                BTRLEMGRLOG_ERROR ("\tReceived BTRMGR_EVENT_DEVICE_OP_INFORMATION Event for \"%s\" (%llu) with Empty Value. \n", event.m_deviceOpInfo.m_name, event.m_deviceOpInfo.m_deviceHandle);
            }
        }
        break;
    default:
        BTRLEMGRLOG_INFO("\tReceived %d Event from BTRMgr\n", event.m_eventType);
        break;
    }

    return (BTRMGR_RESULT_SUCCESS);
}


BtrMgrIf::BtrMgrIf (
) {
    if (BTRMGR_RESULT_SUCCESS != BTRMGR_Init()) {
        BTRLEMGRLOG_ERROR("Failed to init BTRMgr...!\n");
    }
    else {
        BTRLEMGRLOG_INFO("Success init BTRMgr...!\n");
        BTRMGR_RegisterEventCallback (BtrMgrIf::eventCallback);
        BTRLEMGRLOG_INFO("Successfully BTRMGR_RegisterEventCallback...!\n");
    }
}

BtrMgrIf::~BtrMgrIf (
) {
}

BtrMgrIf*
BtrMgrIf::getInstance (
) {
    if (NULL == m_instance ) {
        BTRLEMGRLOG_INFO("BTRMgr instance created first time..\n" );
        m_instance = new BtrMgrIf();
    }

    return (m_instance);
}

void
BtrMgrIf::releaseInstance (
    void
) {
    if (m_instance ) {
        delete m_instance;
        m_instance = NULL;
    }
}

void
BtrMgrIf::shutdownIfce (
    void
) {
    stBtMgrIfLeProp stTileProp;
    {
        stTileProp.m_name.clear();
        stTileProp.m_macAddr.clear();
        stTileProp.m_serviceUuid.clear();
        stTileProp.m_notifyData.clear();
    }
    {
        stTileProp.m_devId  = 0;
        stTileProp.m_eventType = LEMGR_EVENT_DEVICE_DISCOVERY_COMPLETE;
    }

    m_instance->pushEvent(&stTileProp);
}

BTRLEMGR_Result_t
BtrMgrIf::getLimitBeaconDetection (
    unsigned char*  isLimited
) {
    BTRLEMGR_Result_t   ret = LEMGR_RESULT_GENERIC_FAILURE;

    if (NULL != isLimited) {
        if (BTRMGR_GetLimitBeaconDetection(0, isLimited) == BTRMGR_RESULT_SUCCESS) {
            ret = LEMGR_RESULT_SUCCESS;
            BTRLEMGRLOG_INFO("Beacon Detection Limited : %s\n", (*isLimited ? "true" : "false"));
        }
        else {
            BTRLEMGRLOG_ERROR("Failed to retrieve from BTRMGR_GetLimitBeaconDetection\n");
        }
    }
    else {
        BTRLEMGRLOG_ERROR("Failed to check whether beacon detection is limited or not\n");
    }

    return ret;
}

BTRLEMGR_Result_t
BtrMgrIf::setLimitBeaconDetection (
    unsigned char   isLimited
) {
    BTRLEMGR_Result_t   ret = LEMGR_RESULT_GENERIC_FAILURE;

    BTRLEMGRLOG_INFO("Beacon Detection Limited : %s\n", (isLimited ? "true" : "false"));
    if (BTRMGR_SetLimitBeaconDetection(0, isLimited) == BTRMGR_RESULT_SUCCESS) {
        ret = LEMGR_RESULT_SUCCESS;
    }
    else {
        BTRLEMGRLOG_ERROR("Failed to set limit to beacon detection \n");
    }

    return ret;
}


bool
BtrMgrIf::getLeDeviceList (
) {
    BTRLEMGRLOG_TRACE("Entering..\n");

    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    BTRMGR_DiscoveredDevicesList_t discoveredDevices;
    rc = BTRMGR_GetDiscoveredDevices(0, &discoveredDevices);
    if (BTRMGR_RESULT_SUCCESS != rc) {
        BTRLEMGRLOG_INFO ("Failed \n");
    }
    else {
        for (int j = 0; j< discoveredDevices.m_numOfDevices; j++) {
            BTRMGR_DevicesProperty_t deviceProperty;
            memset (&deviceProperty, 0, sizeof(deviceProperty));
            BTRMgrDeviceHandle handle = discoveredDevices.m_deviceProperty[j].m_deviceHandle;

            rc = BTRMGR_GetDeviceProperties(0, handle, &deviceProperty);
            if (BTRMGR_RESULT_SUCCESS != rc) {
                BTRLEMGRLOG_ERROR ("failed\n");
            }
            else {

                for (int i = 0; i < deviceProperty.m_serviceInfo.m_numOfService; i++) {

                    if (deviceProperty.m_serviceInfo.m_profileInfo[i].m_uuid == 0xfeed ||
                            deviceProperty.m_serviceInfo.m_profileInfo[i].m_uuid == 0xfeec ) {

                        BTRLEMGRLOG_INFO("Found tile device..\n");
                        //BTRLEMGRLOG_INFO ("Profile ID   : 0x%.4x\n", deviceProperty.m_serviceInfo.m_profileInfo[i].m_uuid);

                        BTRLEMGRLOG_INFO("\n\t Device Id: %d \n\t Device Name: %s \n\t Device Address: %-17llu\n\n", \
                                         j, discoveredDevices.m_deviceProperty[j].m_name, \
                                         discoveredDevices.m_deviceProperty[j].m_deviceHandle);

                        /* Connect to device */
                        rc = BTRMGR_ConnectToDevice(0, handle, BTRMGR_DEVICE_OP_TYPE_LE);
                        if (BTRMGR_RESULT_SUCCESS != rc) {
                            BTRLEMGRLOG_INFO ("Connect to Le Device (%d) failed\n", j);
                        }
                        else {
                            BTRLEMGRLOG_INFO ("\nSuccessfully connected...\n");
                            sleep(5);
                            char charUuid[BTRMGR_NAME_LEN_MAX] = {'\0'};
                            BTRMGR_DeviceServiceList_t  deviceServiceList;
                            const char *serviceUuid = "\0";
                            unsigned char aui8AdapterIdx = 0;

                            if (deviceProperty.m_serviceInfo.m_profileInfo[i].m_uuid == 0xfeed) {
                                serviceUuid = "feed";
                            } else if (deviceProperty.m_serviceInfo.m_profileInfo[i].m_uuid == 0xfeec) {
                                serviceUuid = "feec";
                            }

                            if (BTRMGR_RESULT_SUCCESS == BTRMGR_GetLeProperty (aui8AdapterIdx, handle, serviceUuid, BTRMGR_LE_PROP_UUID, (void*)&deviceServiceList)) {
                                unsigned char idx = 0;

                                for (; idx < deviceServiceList.m_numOfService; idx++) {
                                    if (deviceServiceList.m_uuidInfo[idx].flags & BTRMGR_GATT_CHAR_FLAG_READ) {
                                        strncpy (charUuid, deviceServiceList.m_uuidInfo[idx].m_uuid, BTRMGR_NAME_LEN_MAX-1);
                                        break;
                                    }
                                }
                                BTRLEMGRLOG_INFO ("\nNo. of UUIDs : %d | Obtained charUuid : %s\n", deviceServiceList.m_numOfService, charUuid);

                                /*Read UUID */
                                if(charUuid[0] != '\0') {
                                    char apTileId[BTRMGR_MAX_STR_LEN] = {'\0'};
                                    char apArg[BTRMGR_MAX_STR_LEN] = {'\0'};
                                    if(BTRMGR_RESULT_SUCCESS == BTRMGR_PerformLeOp (aui8AdapterIdx, handle, charUuid, BTRMGR_LE_OP_READ_VALUE, apArg, apTileId)) {
                                        BTRLEMGRLOG_INFO ("\n\n Discovered Tile ID is [%s].\n\n", apTileId);
                                        if(apTileId[0] != '\0') {
                                            btrLeMgrUtils::notifyLeDeviceToCloud(apTileId);
                                        }
                                    }
                                }
                            }

                            sleep(1);
                        }
                    }
                }
            }
        }
    }

    BTRLEMGRLOG_TRACE("Exiting..\n");
    return (true);
}


bool
BtrMgrIf::getLeCharProperty (
    ui_long_long    devId,
    std::string&    charUuid
) {
    bool ret = false;
    BTRMGR_DeviceServiceList_t  deviceServiceList;
    const char *serviceUuid = getServiceUuid().c_str();
    unsigned char aui8AdapterIdx = 0;
    BTRMgrDeviceHandle handle = devId;

    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    rc = BTRMGR_GetLeProperty (aui8AdapterIdx, handle, serviceUuid, BTRMGR_LE_PROP_UUID, (void*)&deviceServiceList);

    if (BTRMGR_RESULT_SUCCESS == rc) {
        BTRLEMGRLOG_DEBUG ("Successfully  GetLeProperty for [%llu].\n\n", devId);
        unsigned char idx = 0;

        for (; idx < deviceServiceList.m_numOfService; idx++) {
            if (deviceServiceList.m_uuidInfo[idx].flags & BTRMGR_GATT_CHAR_FLAG_READ) {
                charUuid = deviceServiceList.m_uuidInfo[idx].m_uuid;
                ret = true;
                break;
            }
        }
        BTRLEMGRLOG_INFO ("No. of UUIDs : %d | Obtained charUuid : %s\n", deviceServiceList.m_numOfService, charUuid.c_str());
    }
    else {
        BTRLEMGRLOG_ERROR("Failed to GetLeProperty for [%llu].\n", devId);
    }
    return (ret);
}


bool
BtrMgrIf::performLeOps (
    ui_long_long        in_devId,
    LEMGR_Le_t          in_methodName,
    const std::string&  in_charUuid,
    std::string&        in_out_Value
) {
    BTRMGR_Result_t rc;
    char readValue[BTRMGR_MAX_STR_LEN] = {'\0'};
    char writeValArg[BTRMGR_MAX_STR_LEN] = {'\0'};
    unsigned char aui8AdapterIdx = 0;
    const char *serviceUuid = in_charUuid.c_str();

    BTRMGR_LeOp_t le_Op = BTRMGR_LE_OP_READ_VALUE;

    switch (in_methodName) {
    case LEMGR_OP_READ_VALUE:
        le_Op = BTRMGR_LE_OP_READ_VALUE;
        break;
    case LEMGR_OP_WRITE_VALUE:
        le_Op = BTRMGR_LE_OP_WRITE_VALUE;
        strncpy(writeValArg, in_out_Value.c_str(), BTRMGR_MAX_STR_LEN-1);
        break;
    case LEMGR_OP_START_NOTIFY:
        le_Op = BTRMGR_LE_OP_START_NOTIFY;
        break;
    case LEMGR_OP_STOP_NOTIFY:
        le_Op = BTRMGR_LE_OP_STOP_NOTIFY;
        break;
    default:
        break;
    }

    rc = BTRMGR_PerformLeOp (aui8AdapterIdx, in_devId, serviceUuid, le_Op, writeValArg, readValue);

    if(BTRMGR_RESULT_SUCCESS == rc) {
        if(BTRMGR_LE_OP_READ_VALUE == le_Op) {
            in_out_Value = (readValue[0] != '\0')?readValue:"";
        }
        BTRLEMGRLOG_INFO("Successfully performed Le Operation for dev Id[%llu].\n", in_devId);
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRLEMGRLOG_ERROR("Failed to Perform Le Operation for [%llu].\n", in_devId);
        return false;
    }
    return true;
}


bool
BtrMgrIf::startLeDeviceDiscovery (
    std::string asUuid
) {
    BTRLEMGRLOG_TRACE("Entering..\n");
    BTRMGR_DiscoveryStatus_t discoveryStatus;
    BTRMGR_DeviceOperationType_t devOpType = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;

    if ((BTRMGR_RESULT_SUCCESS == BTRMGR_GetDiscoveryStatus(0, &discoveryStatus, &devOpType)) &&
        (BTRMGR_DISCOVERY_STATUS_IN_PROGRESS ==  discoveryStatus) &&
        (BTRMGR_DEVICE_OP_TYPE_LE == devOpType)) {
        stopLeDeviceDiscovery();
    }


    if ((BTRMGR_RESULT_SUCCESS == BTRMGR_GetDiscoveryStatus(0, &discoveryStatus, &devOpType)) &&
        (BTRMGR_DISCOVERY_STATUS_OFF == discoveryStatus) &&
        (BTRMGR_RESULT_SUCCESS == BTRMGR_StartDeviceDiscovery(0, BTRMGR_DEVICE_OP_TYPE_LE))) {
        BTRLEMGRLOG_INFO("Successfully started Le Discovery.\n");
        return true;
    }
    else {
        BTRLEMGRLOG_ERROR("Start Le Discovery failed.\n");
        return false;
    }
}


bool
BtrMgrIf::stopLeDeviceDiscovery (
) {
    BTRLEMGRLOG_TRACE("Entering..\n");
    BTRMGR_DiscoveryStatus_t discoveryStatus;
    BTRMGR_DeviceOperationType_t devOpType = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;

    if ((BTRMGR_RESULT_SUCCESS == BTRMGR_GetDiscoveryStatus(0, &discoveryStatus, &devOpType )) &&
        (BTRMGR_DISCOVERY_STATUS_IN_PROGRESS ==  discoveryStatus) &&
        (BTRMGR_DEVICE_OP_TYPE_LE == devOpType) &&
        (BTRMGR_RESULT_SUCCESS == BTRMGR_StopDeviceDiscovery(0, BTRMGR_DEVICE_OP_TYPE_LE))) {
        BTRLEMGRLOG_INFO("Stop Le Discovery.\n");
        return true;
    }
    else if (BTRMGR_DISCOVERY_STATUS_OFF == discoveryStatus) {
        BTRLEMGRLOG_INFO("Stopped Discovery Internally.\n");
        return true;
    }
    else {
        BTRLEMGRLOG_ERROR("Failed to Stop Le Discovery.\n");
        return false;
    }
}


bool
BtrMgrIf::isLeDev_Tile (
    unsigned short uuid
) {
    bool ret = (uuid == 0xfeed || uuid == 0xfeec);

    if (ret) {
        BTRLEMGRLOG_DEBUG("This is Tile device.\n");
    }

    return ret;
}

std::string&
BtrMgrIf::getServiceUuid (
    void
) {
    BTRLEMGRLOG_DEBUG("This is Tile device. - %s\n", m_serviceUuid.c_str());
    return m_serviceUuid;
}

void
BtrMgrIf::setServiceUuid (
    unsigned short uuid
) {
    if (uuid == 0xfeed) {
        m_serviceUuid = "feed";
    }
    else if (uuid == 0xfeec) {
        m_serviceUuid = "feec";
    }
}


bool
BtrMgrIf::doConnectLeDevice (
    unsigned long long int devId
) {
    BTRLEMGRLOG_INFO ("Issuing BTRMGR_ConnectToDevice (%llu).\n", devId);

    BTRMgrDeviceHandle handle = devId;
    /* Connect to device */
    if (BTRMGR_RESULT_SUCCESS != BTRMGR_ConnectToDevice(0, handle, BTRMGR_DEVICE_OP_TYPE_LE)) {
        BTRLEMGRLOG_ERROR ("Failed to connect Le Device (%llu).\n", devId);
        return (false);
    }

    BTRLEMGRLOG_INFO ("Successfully Connected to (%llu).\n", handle);
    return (true);
}

bool
BtrMgrIf::doDisconnectLeDevice (
    unsigned long long int devId
) {
    BTRLEMGRLOG_INFO ("Issuing BTRMGR_DisconnectFromDevice (%llu).\n", devId);

    return (BTRMGR_RESULT_SUCCESS == BTRMGR_DisconnectFromDevice(0, devId));
}

void
BtrMgrIf::pushEvent (
    stBtMgrIfLeProp* ptr
) {
    BTRLEMGRLOG_TRACE("[postTileEventMsQ] Enter...\n");

    std::lock_guard<std::mutex> guard(m_mutex);

    BTRLEMGRLOG_INFO("current queue size = %u. Pushing event (type = %d, device id %llu, mac %s).\n",
                     m_queue.size(), ptr->m_eventType, ptr->m_devId, ptr->m_macAddr.c_str());

    m_queue.push(*ptr);
    BTRLEMGRLOG_INFO("[postTileEventMsQ] Posting Tile [(%lld) ,( %s)] properties to message queue (size %u).\n",
                     ptr->m_devId, ptr->m_macAddr.c_str(), m_queue.size());
    m_condition.notify_one();
    BTRLEMGRLOG_INFO("[postTileEventMsQ] Notified Tile receiver_LeEventQ.. \n");
}

stBtMgrIfLeProp
BtrMgrIf::popEvent (
) {
    std::unique_lock<std::mutex> lock(m_mutex);

    while (m_queue.empty())
    {
        BTRLEMGRLOG_INFO("Queue is empty. Waiting for a new event to arrive.\n");
        BTRLEMGRLOG_INFO("[receiver_LeEventQ] Waiting on Tile Event Notification. \n");
        m_condition.wait(lock);
        BTRLEMGRLOG_INFO("[receiver_LeEventQ] Received Tile Event Notification...\n");
    }
    auto lePropObj = m_queue.front();

    BTRLEMGRLOG_INFO("current queue size = %u. Popping event (type = %d, device id %llu, mac %s).\n",
                     m_queue.size(), lePropObj.m_eventType, lePropObj.m_devId, lePropObj.m_macAddr.c_str());
    BTRLEMGRLOG_INFO("[receiver_LeEventQ] Tile device Id (%llu),  Mac address (%s) in message queue ( of size %d).\n",
                     lePropObj.m_devId, lePropObj.m_macAddr.c_str(), m_queue.size());

    m_queue.pop();

    return lePropObj;
}

bool
BtrMgrIf::writeValue (
    ui_long_long        in_devId,
    const std::string   in_writeCharUuid,
    std::string         in_Value
) {
    char res[BTRMGR_MAX_STR_LEN] = "\0";
    char uuid[BTRMGR_MAX_STR_LEN] = "\0";
    char w_val[BTRMGR_MAX_STR_LEN] = "\0";

    if(!in_writeCharUuid.empty())
        snprintf(uuid,BTRMGR_MAX_STR_LEN-1,"%s", in_writeCharUuid.c_str());
    else {
        BTRLEMGRLOG_ERROR("Failed to writeValue due to Empty UUID for dev id [%llu].\n", in_devId);
        return false;
    }

    if(!in_Value.empty())
        snprintf(w_val,BTRMGR_MAX_STR_LEN-1, in_Value.c_str());
    else {
        BTRLEMGRLOG_ERROR("Failed to writeValue due to Empty Value for dev id [%llu].\n", in_devId);
        return false;
    }

    if(BTRMGR_RESULT_SUCCESS == BTRMGR_PerformLeOp (0, in_devId, uuid, BTRMGR_LE_OP_WRITE_VALUE, w_val, res)) {
        BTRLEMGRLOG_INFO("Successfully write the value [%s] for char UUID [%s] to dev Id[%llu] \n",w_val, uuid, in_devId);
    }
    else {
        BTRLEMGRLOG_ERROR("Failed to writeValue for char UUID [%s] to dev_Id [%llu].\n", uuid, in_devId);
        return false;
    }
    return true;
}
