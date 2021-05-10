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
 * lemgr_iarm_int_if.cpp
 *
 *  Created on: Sep 12, 2018
 *      Author: rdey001c
 */

#include "libIBus.h"
#include "libIARM.h"

#include "btrTileMgr.h"
#include "btrLeMgr_type.h"
#include "btrLeMgr_logger.h"
#include "lemgr_iarm_interface.h"

/* Static Function Prototypes */
static IARM_Result_t leMgr_leScanStart(void* arg);
static IARM_Result_t leMgr_leScanStop(void* arg);
static IARM_Result_t leMgr_getLeDeviceList(void* arg);
static IARM_Result_t leMgr_leConnect(void* arg);
static IARM_Result_t leMgr_leDisconnect(void* arg);
static IARM_Result_t leMgr_getTileList(void* arg);
static IARM_Result_t leMgr_doRingATile (void* arg);
static IARM_Result_t leMgr_setLimitBeaconDetection (void* arg);
static IARM_Result_t leMgr_getLimitBeaconDetection (void* arg);
static IARM_Result_t leMgr_tileCmdRequest (void* arg);


static unsigned char gIsLeMgr_Internal_Inited = 0;

/* Static Function Definition */
static IARM_Result_t leMgr_leScanStart ( void* arg) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;


    BTRLEMGRLOG_TRACE ("Exiting..!!!\n");
    return retCode;
}

static IARM_Result_t leMgr_leScanStop (void* arg)
{
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;

    BTRLEMGRLOG_TRACE ("Exiting..!!!\n");
    return retCode;
}

static IARM_Result_t leMgr_getLeDeviceList(void* arg) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;

    BTRLEMGRLOG_TRACE ("Exiting..!!!\n");
    return retCode;
}

static IARM_Result_t leMgr_leConnect(void* arg) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;

    BTRLEMGRLOG_TRACE ("Exiting..!!!\n");
    return retCode;
}

static IARM_Result_t leMgr_leDisconnect(void* arg) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;

    BTRLEMGRLOG_TRACE ("Exiting..!!!\n");
    return retCode;
}
/* Static Function Definition */
static IARM_Result_t leMgr_tileCmdRequest (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    leTileRequestHandleParam_t *param = (leTileRequestHandleParam_t*) arg;

    BTRLEMGRLOG_TRACE ("Entering..!!!\n");

    BtrTileMgr* tileLeMgr = BtrTileMgr::getInstance();
    std::string request = param->request;
    BTRLEMGRLOG_INFO ("Tile Cmd Request : [ %s ]\n", request.c_str());
    if (LEMGR_RESULT_SUCCESS != tileLeMgr->processTileCmdRequest(request)) {
        retCode = IARM_RESULT_INVALID_STATE;
    }

    BTRLEMGRLOG_TRACE ("Exiting..!!!\n");
    return retCode;
}

static IARM_Result_t leMgr_setLimitBeaconDetection (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    leLimitBeaconDetection_t*  params = (leLimitBeaconDetection_t*) arg;

    BTRLEMGRLOG_TRACE ("Entering..!!!\n");

    BtrTileMgr* tileLeMgr = BtrTileMgr::getInstance();

    retCode = (IARM_Result_t)tileLeMgr->setLimitBeaconDetection((unsigned char)params->limitBeaconDetection);
    if(retCode != IARM_RESULT_SUCCESS)
        BTRLEMGRLOG_ERROR ("Failed to set the limitBeaconDetection\n");

    BTRLEMGRLOG_TRACE ("Exiting..!!!\n");
    return retCode;
}

static IARM_Result_t leMgr_getLimitBeaconDetection (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    leLimitBeaconDetection_t*  params = (leLimitBeaconDetection_t*) arg;

    BTRLEMGRLOG_INFO ("Entering..!!!\n");

    BtrTileMgr* tileLeMgr = BtrTileMgr::getInstance();
    retCode = (IARM_Result_t)tileLeMgr->getLimitBeaconDetection((unsigned char*)&params->limitBeaconDetection);
    if(retCode != IARM_RESULT_SUCCESS)
       BTRLEMGRLOG_ERROR ("Failed to get the limitBeaconDetection\n");
    BTRLEMGRLOG_TRACE ("Exiting..!!!\n");
    return retCode;
}


/* Static Function Definition */
static IARM_Result_t leMgr_doRingATile (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    leRingATileHandleParam_t*  ringParams = (leRingATileHandleParam_t*) arg;

    BTRLEMGRLOG_INFO ("Entering..!!!\n");

    BtrTileMgr* tileLeMgr = BtrTileMgr::getInstance();

    std::string tileId = ringParams->Id;
    std::string session = ringParams->sessionId;

    BTRLEMGRLOG_INFO ("Tile Id : %s, Session Id : %s\n", tileId.c_str(), session.c_str());

    tileLeMgr->doRingATile(tileId, session);

    BTRLEMGRLOG_INFO ("Exiting..!!!\n");
    return retCode;
}

static IARM_Result_t leMgr_getTileList ( void*   arg )
{
    BtrTileMgr* tileLeMgr = BtrTileMgr::getInstance();
    tileLeMgr->print_LeDevPropMap();
    return (IARM_RESULT_SUCCESS);
}
/* Public Functions */
void btrLeMgr_BeginIARMMode ()
{
    BTRLEMGRLOG_INFO ("Entering\n");
    IARM_Result_t err = IARM_RESULT_SUCCESS;

    if (!gIsLeMgr_Internal_Inited) {
        gIsLeMgr_Internal_Inited = 1;
        IARM_Bus_Init(IARM_BUS_BTRLEMGR_NAME);
        IARM_Bus_Connect();

        BTRLEMGRLOG_INFO ("IARM Interface Initializing\n");

        if(IARM_RESULT_SUCCESS != IARM_Bus_RegisterCall(IARM_BUS_LEMGR_API_leScanStart, leMgr_leScanStart))
        {
            BTRLEMGRLOG_ERROR ("Error registering call(leMgr_leScanStart) in IARM.. error code : %d\n",err);
        }
        if(IARM_RESULT_SUCCESS != IARM_Bus_RegisterCall(IARM_BUS_LEMGR_API_leScanStop, leMgr_leScanStop))
        {
            BTRLEMGRLOG_ERROR ("Error registering call(leMgr_leScanStop) in IARM.. error code : %d\n",err);
        }
        if(IARM_RESULT_SUCCESS != IARM_Bus_RegisterCall(IARM_BUS_LEMGR_API_leGetDevList, leMgr_getLeDeviceList))
        {
            BTRLEMGRLOG_ERROR ("Error registering call(GetLeDeviceList) in IARM.. error code : %d\n",err);
        }
        if(IARM_RESULT_SUCCESS != IARM_Bus_RegisterCall(IARM_BUS_LEMGR_API_leConnect, leMgr_leConnect))
        {
            BTRLEMGRLOG_ERROR ("Error registering call(leConnect) in IARM.. error code : %d\n",err);
        }
        if(IARM_RESULT_SUCCESS != IARM_Bus_RegisterCall(IARM_BUS_LEMGR_API_leDisconnect, leMgr_leDisconnect))
        {
            BTRLEMGRLOG_ERROR ("Error registering call(leDisonnect) in IARM.. error code : %d\n",err);
        }
        if(IARM_RESULT_SUCCESS != IARM_Bus_RegisterCall(IARM_BUS_LEMGR_API_leGetTileList, leMgr_getTileList))
        {
            BTRLEMGRLOG_ERROR ("Error registering call(GetTileList) in IARM.. error code : %d\n",err);
        }
        if(IARM_RESULT_SUCCESS !=  IARM_Bus_RegisterCall(IARM_BUS_LEMGR_API_leRingATile, leMgr_doRingATile))
        {
            BTRLEMGRLOG_ERROR ("Error registering call(leRingATile) in IARM.. error code : %d\n",err);
        }
        if(IARM_RESULT_SUCCESS !=  IARM_Bus_RegisterCall(IARM_BUS_LEMGR_API_leGetLimitBeaconDetection, leMgr_getLimitBeaconDetection))
        {
            BTRLEMGRLOG_ERROR ("Error registering call(getLimitBeaconDetection) in IARM.. error code : %d\n",err);
        }
        if(IARM_RESULT_SUCCESS !=  IARM_Bus_RegisterCall(IARM_BUS_LEMGR_API_leSetLimitBeaconDetection, leMgr_setLimitBeaconDetection))
        {
            BTRLEMGRLOG_ERROR ("Error registering call(setLimitBeaconDetection) in IARM.. error code : %d\n",err);
        }
        if(IARM_RESULT_SUCCESS !=  IARM_Bus_RegisterCall(IARM_BUS_LEMGR_API_leTileRequest, leMgr_tileCmdRequest))
        {
            BTRLEMGRLOG_ERROR ("Error registering call(leMgr_tileCmdRequest) in IARM.. error code : %d\n",err);
        }

        BTRLEMGRLOG_INFO ("IARM Interface Inited Successfully\n");

    }
    else {
        BTRLEMGRLOG_INFO ("IARM Interface Already Inited\n");
    }

    return;
}


void 
btrLeMgr_TermIARMMode (
    void
) {
    if (gIsLeMgr_Internal_Inited) {
        IARM_Bus_Disconnect();
        IARM_Bus_Term();
    }
}
