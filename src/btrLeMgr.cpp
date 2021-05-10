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

// Comment: May be we can change this file name to btrLeAppMgr.cpp which would use the classes
//          btrLeMgrBase or its child classes btrTileMgr/btrAdvMgr...

#include <string.h>
#include <thread>
#include <memory>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include "btrLeMgr_logger.h"
#include "btrLeMgr_utils.h"
#include "btrTileMgr.h"
#include "lemgr_iarm_interface.h"

#ifdef INCLUDE_BREAKPAD
#include <client/linux/handler/exception_handler.h>
#endif


static bool gbExitBTRLeMgr = false;

#ifdef INCLUDE_BREAKPAD
static bool
breakpadDumpCallback (
    const google_breakpad::MinidumpDescriptor& descriptor,
    void* context,
    bool succeeded
) {
     printf("breakpadDumpCallback: Bluetooth Le App Manager crashed ---- Dump path: %s\n", descriptor.path());
     return succeeded;
}
#endif


static void
btrLeMgr_SignalHandler (
    int i32SignalNumber
) {
    time_t curr = 0;

    time(&curr);
    printf ("Received SIGNAL %d = %s - %s\n", i32SignalNumber, strsignal(i32SignalNumber), ctime(&curr));
    fflush(stdout);

    if (i32SignalNumber == SIGTERM)
        gbExitBTRLeMgr = true;
}



int
main (
    int     argc,
    char*   argv[]
) {

#ifdef RDK_LOGGER_ENABLED
    const char* debugConfigFile = NULL;
#endif

    gbExitBTRLeMgr = false;
    int itr=0;

    while (itr < argc) {
        if(strcmp(argv[itr],"--debugconfig")==0) {
            itr++;
            if (itr < argc) {
#ifdef RDK_LOGGER_ENABLED
                debugConfigFile = argv[itr];
                BTRLEMGRLOG_DEBUG("debugconfig is [%s].\n\n", debugConfigFile);
#endif
            }
            else {
                break;
            }
        }

        if(strcmp(argv[itr],"--stbmac")==0) {
            itr++;
            if (itr < argc) {
                char *mac = argv[itr];
                BTRLEMGRLOG_DEBUG("--stbmac is [%s].\n\n", mac);
                btrLeMgrUtils::setSTBMac(mac);
            }
            else {
                break;
            }
        }

        if(strcmp(argv[itr],"--rfcUrl")==0) {
            itr++;
            if (itr < argc) {
                char *rfcUrl = argv[itr];
                BTRLEMGRLOG_DEBUG("--rfcUrl is [%s].\n\n", rfcUrl);
                btrLeMgrUtils::setRfcUrl(rfcUrl);
            }
            else {
                break;
            }
        }

        itr++;
    }

    int ret = 0;
    time_t curr;

#ifdef INCLUDE_BREAKPAD
    google_breakpad::MinidumpDescriptor descriptor("/opt/minidumps/");
    google_breakpad::ExceptionHandler eh(descriptor, NULL, breakpadDumpCallback, NULL, true, -1);
#endif
#ifdef RDK_LOGGER_ENABLED
    rdk_logger_init(debugConfigFile);
#endif

    BTRLEMGRLOG_INFO (" Starting BTR LE Application manager..!!!");

    BTRLEMGRLOG_INFO (" Starting IARM External interface for BTR LE Application manager..!!!");
    btrLeMgr_BeginIARMMode();

    unsigned char isLimited   = 0;

    // Comment: Would prefer to use a Baseclass Pointer rather than using auto which would translate to BtrTileMgr*
    //          Use a name other than tileLeMgr like leMgrBasePtr, so that address of child objects  can be ased to base class ptr
    //          and operated on them
    BtrTileMgr* tileLeMgr = BtrTileMgr::getInstance();

    std::thread beacon_thread = tileLeMgr->beaconDetectionThread();

    //BtrTileMgr* tileLeMgr = std::unique_ptr<BtrTileMgr> (new BtrTileMgr("xfeed"));
    //BtrTileMgr* tileLeMgr = new BtrTileMgr("xfeed");

    // Comment: Event listener Thread would be required as part of every child class, would prefer that the thread gets created within the
    //          Child class avoding the need to call it from from BtrLeAppMgr
    std::thread tile_thread = tileLeMgr->eventListenerThread();

    signal(SIGTERM, btrLeMgr_SignalHandler);

    while (gbExitBTRLeMgr == false) {

        tileLeMgr->getLimitBeaconDetection(&isLimited);
        if (!isLimited) {
            // Comment: Would prefer to have this call as part of the BtrLeMgrBase class, so that the BtrLeAppMgr is not aware of BtrMgrIf
            /* Start Le Discovery */
            tileLeMgr->leScanStart();
            tileLeMgr->start_periodic_maintenance();

            unsigned char counter = 0;
            while (gbExitBTRLeMgr == false) {
                tileLeMgr->run_periodic_maintenance();
                if (0 == (++counter % 4)) { //This part runs once a minute.
                    time(&curr);
                    BTRLEMGRLOG_INFO("BTR_LE_MGR: HeartBeat at %s", ctime(&curr));
                    tileLeMgr->print_LeDevPropMap();
                    tileLeMgr->getLimitBeaconDetection(&isLimited);
                    if (isLimited) {
                        BTRLEMGRLOG_INFO("BTR_LE_MGR: Beacon Detection is limited. ");
                        tileLeMgr->leScanStop();
                        break;
                    }
                }
                sleep(LE_TILE_SCAN_CHECK_SLEEP_INTERVAL);
            }
        }

        sleep(5);
    }

    BTRLEMGRLOG_INFO ("BTR_LE_MGR: Quitting %s\n", ctime(&curr));

    /* Stop Le Discovery */
    tileLeMgr->leScanStop();

    tileLeMgr->eventListenerQuit(true);

    tileLeMgr->quitBeaconDetection();

    // Comment: What happens when we do a systemctl restart of btrLeAppMgr? Is all memory at every layer released gracefully back to the system
    //          Or are we leaving things for the kernel to clean up?
    tile_thread.join();
    BTRLEMGRLOG_INFO ("BTR_LE_MGR: Quitting Tile Thread\n");

    beacon_thread.join();
    BTRLEMGRLOG_INFO ("BTR_LE_MGR: Quitting Beacon Thread\n");

    btrLeMgr_TermIARMMode();

    BtrTileMgr::releaseInstance();

    BTRLEMGRLOG_INFO ("BTR_LE_MGR: Exit %s\n", ctime(&curr));

    return (ret);
}

