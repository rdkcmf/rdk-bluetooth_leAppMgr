#!/bin/sh
# If not stated otherwise in this file or this component's Licenses.txt file the
# following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

. /etc/device.properties
. /etc/include.properties
. $RDK_PATH/utils.sh

MAX_WAIT_TIME=60

BLE_DISCOVERY_PARAM="Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.BLE.Discovery"
BLE_REPORTING_URL_PARAM="Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.BLE.Tile.ReportingURL"


get_STBMac()
{
    hw_mac=`getEstbMacAddress`
    if [ -z $hw_mac ]; then
        timer=0
        while :
            do
            if [ -e "/tmp/.macAddress" ]; then
               hw_mac=`cat /tmp/.macAddress | sed 's/://g'`;
               echo "$hw_mac"
               break
            else
               sleep 2
               timer=`expr $timer + 2`
               if [ $timer -eq $MAX_WAIT_TIME ]; then
                  break
               fi
            fi
        done
    else  
       echo "$hw_mac"
    fi
}


startUp_btrLeAppMgr()
{
    # Check for RFC  
    BLE_DIS_STATE="$(tr181Set $BLE_DISCOVERY_PARAM 2>&1 > /dev/null)"
    BLE_REPORTING_URL="$(tr181Set $BLE_REPORTING_URL_PARAM 2>&1 > /dev/null)"

    if [ "$BLE_REPORTING_URL" == "" ]; then
       echo RFC flag for ["$BLE_REPORTING_URL_PARAM"] is NOT Configured, so failed to send LE Notification to Cloud.
    fi 

    if [ "$BLE_DIS_STATE" == "ON" ] || [ "$BLE_DIS_STATE" == "true" ] ; then
        HwMac=`get_STBMac `
        if [ -z $HwMac ]; then
           echo "Failed to start btrLeAppMgr process, Can't get STB macAddress. "
        else
            # Check for RFC  
            BLE_DIS_STATE="$(tr181Set $BLE_DISCOVERY_PARAM 2>&1 > /dev/null)"
            if [ "$BLE_DIS_STATE" == "ON" ] || [ "$BLE_DIS_STATE" == "true" ] ; then
                /usr/bin/btrLeAppMgr --debugconfig $DEBUGINIFILE --stbmac $HwMac --rfcUrl $BLE_REPORTING_URL
                echo "RFCFlag $BLE_DISCOVERY_PARAM is $BLE_DIS_STATE, Starting btrLeAppMgr Process"
            else
                echo "RFC Flag is OFF, so not statring btrLeAppMgr"
                systemctl stop btrLeAppMgr
            fi 
        fi
    else
      systemctl stop btrLeAppMgr
    fi

}

startUp_btrLeAppMgr
