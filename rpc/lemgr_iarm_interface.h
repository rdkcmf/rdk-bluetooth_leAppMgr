/*
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



/**
* @defgroup leappmgr
* @{
* @defgroup rpc
* @{
**/

#ifndef _LEMGR_IARM_EXT_IF_H_
#define _LEMGR_IARM_EXT_IF_H_


#ifdef __cplusplus
extern "C" {
#endif

#define IARM_BUS_BTRLEMGR_NAME  "BTRLeMgrBus"

#define TILE_ID_MAX_SIZE        256
#define SESSION_ID_MAX_SIZE     256
#define TILE_REQUEST_MAX_SIZE   1024

/*
 * Declare RPC leRingATile API names
 */
#define  IARM_BUS_LEMGR_API_leScanStart               "leScanStart"
#define  IARM_BUS_LEMGR_API_leScanStop                "leScanStop"
#define  IARM_BUS_LEMGR_API_leGetDevList              "GetLeDeviceList"
#define  IARM_BUS_LEMGR_API_leConnect                 "leConnect"
#define  IARM_BUS_LEMGR_API_leDisconnect              "leDisonnect"
#define  IARM_BUS_LEMGR_API_leGetTileList             "GetTileList"
#define  IARM_BUS_LEMGR_API_leRingATile               "leRingATile"
#define  IARM_BUS_LEMGR_API_leTileRequest             "leTileRequest"
#define  IARM_BUS_LEMGR_API_leSetLimitBeaconDetection "leSetLimitBeaconDetection"
#define  IARM_BUS_LEMGR_API_leGetLimitBeaconDetection "leGetLimitBeaconDetection"



typedef struct _leRingATileHandleParam_t {
    char        Id[TILE_ID_MAX_SIZE];
    char        sessionId[SESSION_ID_MAX_SIZE];
    int         triggerCmd;
} leRingATileHandleParam_t;

typedef struct _leTileRequestHandleParam_t {
    char        request[TILE_REQUEST_MAX_SIZE];
} leTileRequestHandleParam_t;

typedef struct _leLimitBeaconDetection_t {
    unsigned char adapterIndex;
    unsigned char limitBeaconDetection;
} leLimitBeaconDetection_t;

void btrLeMgr_BeginIARMMode();
void btrLeMgr_TermIARMMode();

#ifdef __cplusplus
}
#endif

#endif /* _LEMGR_IARM_EXT_IF_H_ */


/** @} */
/** @} */

