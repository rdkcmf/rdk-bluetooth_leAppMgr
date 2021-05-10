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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include "libIBus.h"
#include "libIARM.h"
//#include "btrLeMgr.h"
#include "lemgr_iarm_interface.h"


volatile int wait = 1;
int uselection = 0;


static void printOptions (void)
{
    printf ("\n\n");
    printf (" 1. Start Le Discovering\n");
    printf (" 2. Stop Le Discovering\n");
    printf (" 3. Get List of Le Discovered Devices\n");
    printf (" 4. Connect to Le Device\n");
    printf (" 5. DisConnect from Le Device\n");
    printf (" 6. Get Le Device Properties\n");
    printf (" 7. Perform method Operation to Le Devices, Options\n");
    printf (" 8. Get Tile device list.\n");
    printf (" 9. Ring a Tile device.\n");
    printf (" 10. Tile Request as a json message , like \n ** {\"mac_address\":\"someMac\",\"session_token\":\"CvobXw==\",\"rand_a\":\"6ii5ULmyQ42zpb9M85c==\"} **.\n");
    printf (" 0. Quit\n");
    printf ("\n\n");
    printf ("Please enter the option that you want to test\t");

    return;
}

static int getUserSelection (void)
{
    int mychoice;
    printf("Enter a choice...\n");
    scanf("%d", &mychoice);
    getchar();//to catch newline
    return mychoice;
}

void test_doLeScanStart() {
}

void test_doLeScanStop() {

}

void test_doGetLeDeviceList() {

}

void test_doLeConnect() {

}

void test_doLeDisconnect() {

}

void test_doLeRingATile() {

}

void test_doGetTileList() {

}

void test_doLeTileCmdRequest() {

    int choice;
    char* jsonStr = NULL;

    printf("This is to set TR181 params: \"Device.DeviceInfo.X_RDKCENTRAL-COM_xBlueTooth.BLE.Tile.Cmd.Request\" as json string.\n");
    printf("\tEnter a choice...\n");
    printf("\t---------------------\n");
    printf("\t 1. Give User Input as CMD Request json string.\n");
    printf("\t 2. Hardcoded RAND_A json string for Pre-provisioned Tile uuid (b8e85e65d022498f) \n");
    printf("\t 3. Hardcoded CMD_READY & CMD_PLAY json string for Pre-provisioned Tile uuid (b8e85e65d022498f).\n");
    printf("\t---------------------\n");
    scanf("%d", &choice);

    if(1 == choice) {
        printf("Input as CMD Request json: \n");
        scanf("%s", jsonStr);
    }
    else if (2 == choice) {
        const char* jsonMsg_RandA = "{\"mac_address\":\"someMac\",\"session_token\":\"AAAAAA==\",\"rand_a\":\"AAAAAAAAAAAAAAAAAAA=\",\"sender_client_uuid\":\"unitTest\",\"tile_uuid\":\"b8e85e65d022498f\",\"code\":\"MEP_TOA_OPEN_CHANNEL\",\"client_ts\":1539394498884}";
        jsonStr = (char*)jsonMsg_RandA;
    }
    else if (3 == choice) {
        const char* cmdStr = "{\"disconnect_on_completion\":true, \"commands\":[{\"command\":\"AhIOAbjKlG0=\",\"response_mask\":\"AQ==\"},{\"command\":\"AgUCAQPiJcw3\",\"response_mask\":\"Bw==\"}],\"sender_client_uuid\":\"unitTest\",\"tile_uuid\":\"b8e85e65d022498f\",\"code\":\"MEP_TOA_SEND_COMMANDS\"}";
        jsonStr = (char*) cmdStr;
    }

    leTileRequestHandleParam_t param;
    memset(&param, 0, sizeof(param));

    printf("[%s] Entering...\r\n", __FUNCTION__);
    IARM_Result_t retVal = IARM_RESULT_SUCCESS;

    strncpy(param.request, jsonStr, strlen(jsonStr));
    retVal = IARM_Bus_Call(IARM_BUS_BTRLEMGR_NAME, IARM_BUS_LEMGR_API_leTileRequest, (void *)&param, sizeof(leTileRequestHandleParam_t ));
    printf("\n***********************************");
    printf("\n \"%s\",  \"%s\"", IARM_BUS_LEMGR_API_leTileRequest, ((retVal == IARM_RESULT_SUCCESS) ?"Successfully set.":"Failed."));
    printf("\n***********************************\n");
    if(retVal == IARM_RESULT_SUCCESS) {
        printf("Succesfully set\n");
    }
    printf("[%s] Exiting..\r\n", __FUNCTION__);

}

void test_doRingATile()
{
//    BTRLEMGR_Result_t rc = LEMGR_RESULT_SUCCESS;
    leRingATileHandleParam_t param;
    memset(&param, 0, sizeof(param));
    printf("[%s] Entering...\r\n", __FUNCTION__);

    //unsigned long long id;
    std::string sess;
    bool trigger;

    printf( "This would required tile Id and Session Id to trigger Ring Tile Device.\n" );
    //std::cout << "Enter Tile id :" ;
    //std::cin >>id;
    //std::cout << "\nEnter Session Id   :" ;
    //std::cin >> sess;
    //std::cout << "\nEnter Trigger [0/1] :" ;
    //std::cin >> trigger;
    const char* id = "d1a2d1a2010900bf";
    sess = "0x1234";
    trigger = 1;
    if(trigger) {
        std::cout << "\nYou  Trigger [0/1] :" ;
        strcpy (param.Id, (char *)id);
        strcpy (param.sessionId, (char *)sess.c_str());
        param.triggerCmd = trigger;
        IARM_Result_t retVal = IARM_RESULT_SUCCESS;

        retVal = IARM_Bus_Call(IARM_BUS_BTRLEMGR_NAME, IARM_BUS_LEMGR_API_leRingATile, (void *)&param, sizeof(leRingATileHandleParam_t ));
        printf("\n***********************************");
        printf("\n \"%s\",  \"%s\"", IARM_BUS_LEMGR_API_leRingATile, ((param.triggerCmd)?"true":"false"));
        printf("\n***********************************\n");
        if(retVal == IARM_RESULT_SUCCESS) {
            printf("Succesfully set\n");
        }
        printf("[%s] Exiting..\r\n", __FUNCTION__);
    }
}

void test_getTileList()
{
    printf("[%s] Entering...\r\n", __FUNCTION__);

    IARM_Result_t retVal = IARM_RESULT_SUCCESS;
    retVal = IARM_Bus_Call(IARM_BUS_BTRLEMGR_NAME, IARM_BUS_LEMGR_API_leGetTileList,NULL, 0);
    printf("\n***********************************");
    printf("\n \"%s\", ", IARM_BUS_LEMGR_API_leGetTileList);
    printf("\n***********************************\n");
    if(retVal == IARM_RESULT_SUCCESS) {
        printf("Succesfully set\n");
    }
    printf("[%s] Exiting..\r\n", __FUNCTION__);
}
int main()
{
    //BTRLEMGR_Result_t rc = LEMGR_RESULT_SUCCESS;
    IARM_Bus_Init("btrLeMgrTest");
    IARM_Bus_Connect();

    int loop = 1, i = 0;

    while(loop)
    {
        printOptions();
        i = getUserSelection();
        switch (i)
        {
        case 1:
            test_doLeScanStart();
            break;
        case 2:
            test_doLeScanStop();
            break;
        case 3:
            break;
        case 4:
            test_doLeConnect();
            break;
        case 5:
            test_doLeDisconnect();
            break;
        case 6:
            break;
        case 7:
            break;
        case 8:
            test_getTileList();
            break;
        case 9:
            test_doRingATile();
            break;
        case 10:
            test_doLeTileCmdRequest();
            break;
        case 0:
            loop = 0;
            break;
        default:
            printf ("Invalid Selection.....\n");
            break;
        }
    }
    return 0;
}
