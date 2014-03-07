/******************************************************************************
  Filename:       ProjectSpecific.c
  Revised:        $Date: 2010-12-21 10:27:34 -0800 (Tue, 21 Dec 2010) $
  Revision:       $Revision: 24670 $

  Description:    ProjectSpecific.c contains project specific code. Namely,
                  a project specific init routine, and project specific comm
                  routines. 
******************************************************************************/


/**************************************************************************************************
 *                                             INCLUDES
 **************************************************************************************************/
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"

#include "BaseED.h"
#include "BaseED_support.h"
#include "BaseED_supportsettings.h"

#include "DebugTrace.h"

#if !defined( WIN32 )
  #include "OnBoard.h"
#endif

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_uart.h"
#include "hal_occsensors.h"
#include "hal_xnv.h"
#include "hal_ota.h"
#include "OSAL_Nv.h"

#include "MT_TASK.h"
#include "MT_UART.h"
#include "MT_RPC.h"
#include "AppUDMT.h"
#include "ota_common.h"
#include "bitmapfs.h"
#include "stub_aps.h"

/**************************************************************************************************
 *                                            GLOBAL
 **************************************************************************************************/

//Declaring the global application instance
appInstance_t appInstance;

extern uint8 *gpacketbitmap;
extern uint16 gtotalpackets;
extern uint16 gtotalMissingPackets;
extern const endPointDesc_t BaseED_epDesc;

/**************************************************************************************************
 *                                            CONSTANTS
 **************************************************************************************************/
/*
NOTE: While debugging, you might not want to program the device as a non-comissionined device.
To do this, perform the following steps:

1. Change the macro APP_NV_COMMISSIONED_STATUS_DEFAULT to DEVICE_ACTIVE : This will make it boot up directly as a active device
2. Change  nv_device_info_default to have final values, i.e. 
    a device id which is not 0xFFFF, 
    a device type which is not SYNERGY_INVALID_DEVICE
    a user id which is not 0xFFFF
3. Do the same for the nv_device_info structure
3. While programming the device, make sure erase flash options is selected

*/

/*
Steps to add a new NV parameter:

1. First add a Macro for the default value.
2. Then add a const variable initialized to that macro.
3. Initialize the RAM shadow values to this new macro.
4. Add the entry in appNVItemDefaultValuesTable. Make sure the new NV item is defined in a ZComDef.h
5. Add the entry in appNVItemTable to point to the RAM shadow values.

Doing the above does the following:

After programming a virgin flash, both the default values and the table values
will start out in sync. Being a virgin flash, the NV item won't exist in it. So
it would get created and initialized with the default value.

From this point onwards, any change in the NV value will update the corresponding
RAM shadow values/Table too. This is where the default value and the NV will
diverge. 

At any point, therefore we can revert back to the default value.
*/

//#define APP_NV_XNV_OTA_UNIT_TIMER         0x040C    
//#define APP_NV_XNV_OTA_REPEAT_COUNT_VALUE 0x040D 

// Macros for default values. Add all default value macros here - update for step 1 
#define APP_NV_UNIT_TIMER_VALUE_DEFAULT            1000
#define APP_NV_REPEAT_COUNT_VALUE_DEFAULT          1
#define APP_NV_PACKET_SIZE_DEFAULT                 10
#define APP_NV_PANLIST_IDX_DEFAULT                 0
#define APP_NV_COORD_RESET_DEFAULT                 COORD_RESET_NORMAL
#define APP_NV_GET_COORD_PARMS_FLAG_DEFAULT        1         
#define APP_NV_COMMISSIONED_STATUS_DEFAULT         DEVICE_COMMISSIONED // NON_COMMISSIONED // NETWORK_COMMISSIONING_IN_PROGRESS //DEVICE_ACTIVE // //NON_COMMISSIONED  //should be NON_COMMISSIONED for a brand new device
#define APP_NV_NUM_DISCOVERED_NWKS_DEFAULTS        0
#define APP_NV_XNV_PACKETS_WRITTEN_DEFAULTS        0
#define APP_NV_XNV_OTA_IN_PROGRESS_DEFAULT         OTA_DL_NOTINPROGRESS
#define APP_NV_XNV_OTA_UNIT_TIMER_DEFAULT          15000
#define APP_NV_XNV_OTA_REPEAT_COUNT_VALUE_DEFAULT  120   //Default timeout of 15000 * 120 ms = 30 mins)
#define APP_NV_CLEAN_ALL_NV_ITEMS_DEFAULT  false         //By default, we never clean our NV items at powerup
#define APP_NV_LAST_STARTUP_SLEEP_COUNT_DEFAULT    1

#define NUM_LQI_PARTITION_LVLS          4
#define NUM_DEVS_PER_LQI_PARTITION      5


#define RADIO_SLEEP_TIMER_DEFAULT                  15000
#define RADIO_SLEEP_TIMER_CNT_DEFAULT              1

// Variables for default values. These will go into the default table - update for step 2
const uint16 app_nv_unit_timer_value_default       = APP_NV_UNIT_TIMER_VALUE_DEFAULT;
const uint16 app_nv_repeat_count_value_default     = APP_NV_REPEAT_COUNT_VALUE_DEFAULT;
const uint16 app_nv_packet_size_default            = APP_NV_PACKET_SIZE_DEFAULT;
const uint8  app_nv_panlist_idx_default            = APP_NV_PANLIST_IDX_DEFAULT;
const uint8  app_nv_coord_reset_default            = APP_NV_COORD_RESET_DEFAULT;
const uint8  nv_get_coord_parms_flag_default       = APP_NV_GET_COORD_PARMS_FLAG_DEFAULT;
const uint8  nv_commissioned_status_default        = APP_NV_COMMISSIONED_STATUS_DEFAULT;
const uint8  nv_num_discovered_nwks_default        = APP_NV_NUM_DISCOVERED_NWKS_DEFAULTS;
const uint16 nv_xnv_num_packets_written_default    = APP_NV_XNV_PACKETS_WRITTEN_DEFAULTS;
const uint8  nv_xnv_ota_in_progress_default        = APP_NV_XNV_OTA_IN_PROGRESS_DEFAULT;
const uint16 nv_xnv_ota_unit_timer_default         = APP_NV_XNV_OTA_UNIT_TIMER_DEFAULT;
const uint16 nv_xnv_ota_repeat_count_value_default = APP_NV_XNV_OTA_REPEAT_COUNT_VALUE_DEFAULT;
const uint16 nv_clean_all_nv_items_default         = APP_NV_CLEAN_ALL_NV_ITEMS_DEFAULT;
const uint16 nv_radio_sleep_timer_cnt_default      = RADIO_SLEEP_TIMER_CNT_DEFAULT;
const uint16 nv_last_startup_sleep_count_default   = APP_NV_LAST_STARTUP_SLEEP_COUNT_DEFAULT;

// Statically defining the default pan info array
// NOTE: update this as new members like RSSI, LQI are added into the NWInfo_t definition
const NWInfo_t nv_pan_info_default_array[MAX_PANS_SCANNED] = {
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
};

// Statically defining the default 'DeviceInfo' structure.
// Change this back when doing real commissioning
#define PRECOMMISSIONED
#define TESTDEVICEID 0057
#ifndef PRECOMMISSIONED
const DeviceInfo_t nv_device_info_default = {
  0xFFFF,
  SYNERGY_INVALID_DEVICE,
  0xFFFF,
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
};

#else
#undef APP_NV_COMMISSIONED_STATUS_DEFAULT
#define APP_NV_COMMISSIONED_STATUS_DEFAULT DEVICE_COMMISSIONED
const DeviceInfo_t nv_device_info_default = {
  TESTDEVICEID,
  //SYNERGY_TESTBED_DEVICE,
  SYNERGY_OCCUPANCY_SENSOR,
  2113,
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
};
#endif

// RAM shadow values for our nv items. These will be always in sync with their
// NV counterparts - for step 3
uint16 nv_unit_timer_value           = APP_NV_UNIT_TIMER_VALUE_DEFAULT;
uint16 nv_repeat_count_value         = APP_NV_REPEAT_COUNT_VALUE_DEFAULT;
uint16 nv_packet_size                = APP_NV_PACKET_SIZE_DEFAULT;
uint8  nv_panlist_idx                = APP_NV_PANLIST_IDX_DEFAULT;
uint8  nv_coord_reset                = APP_NV_COORD_RESET_DEFAULT;
uint8  nv_get_coord_parms_flag       = APP_NV_GET_COORD_PARMS_FLAG_DEFAULT;
uint8  nv_commissioned_status        = APP_NV_COMMISSIONED_STATUS_DEFAULT;
uint8  nv_num_discovered_nwks        = APP_NV_NUM_DISCOVERED_NWKS_DEFAULTS;
uint16 nv_xnv_num_packets_written    = APP_NV_XNV_PACKETS_WRITTEN_DEFAULTS;
uint8  nv_xnv_ota_in_progress        = APP_NV_XNV_OTA_IN_PROGRESS_DEFAULT;
uint16 nv_xnv_ota_unit_timer         = APP_NV_XNV_OTA_UNIT_TIMER_DEFAULT;
uint16 nv_xnv_ota_repeat_count_value = APP_NV_XNV_OTA_REPEAT_COUNT_VALUE_DEFAULT;
uint16 nv_clean_all_nv_items         = APP_NV_CLEAN_ALL_NV_ITEMS_DEFAULT;
uint16 nv_radio_sleep_timer_cnt      = RADIO_SLEEP_TIMER_CNT_DEFAULT;
uint16 nv_last_startup_sleep_count   = APP_NV_LAST_STARTUP_SLEEP_COUNT_DEFAULT;

static appInstance_t appInstance_default;


// Allocating space for the default pan info array
NWInfo_t nv_pan_info_array[MAX_PANS_SCANNED] = {
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0},
};

// Statically defining the 'DeviceInfo' structure.
// Starts the same as the 'default' but will diverge during device commissioning.

//This is what it should be during normal operation
#ifndef PRECOMMISSIONED
DeviceInfo_t nv_device_info = {
  0xFFFF,
  SYNERGY_INVALID_DEVICE,
  0xFFFF,
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
};
#else

//This is just for testing purposes, disable later


DeviceInfo_t nv_device_info = {
  TESTDEVICEID,
  //SYNERGY_TESTBED_DEVICE,
  SYNERGY_OCCUPANCY_SENSOR,
  2113,
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
};
#endif

// IMPORTANT: The order of the default values and their runtime counterparts as
// listed in appNVItemTable MUST be exactly same.
// These will be used to revert back to the default state if needed. - update for step 4
const appNVItemDefaultValues_t appNVItemDefaultValuesTable[] =
{
  {
    APP_NV_UNIT_TIMER_VALUE, sizeof(app_nv_unit_timer_value_default), &app_nv_unit_timer_value_default
  },
  {
    APP_NV_REPEAT_COUNT_VALUE, sizeof(app_nv_repeat_count_value_default), &app_nv_repeat_count_value_default 
  },
  {
    APP_NV_PACKET_SIZE, sizeof(app_nv_packet_size_default), &app_nv_packet_size_default
  },
  {
    APP_NV_PANLIST_IDX, sizeof(app_nv_panlist_idx_default), &app_nv_panlist_idx_default
  },
  {
    APP_NV_PANINFO_STRUCT, sizeof(NWInfo_t) * MAX_PANS_SCANNED, nv_pan_info_default_array
  },
  {
    APP_NV_GET_COORD_PARMS_FLAG, sizeof(nv_get_coord_parms_flag_default), &nv_get_coord_parms_flag_default
  },
  {
    APP_NV_COMMISSIONED_STATUS, sizeof(nv_commissioned_status_default), &nv_commissioned_status_default
  }, 
  {
    APP_NV_NUM_DISCOVERED_NWKS, sizeof(nv_num_discovered_nwks_default), &nv_num_discovered_nwks_default
  },    
  {
    APP_NV_DEVICE_INFO_STRUCT, sizeof(nv_device_info_default), &nv_device_info_default
  },
  {
    APP_NV_XNV_PACKETS_WRITTEN, sizeof(nv_xnv_num_packets_written_default), &nv_xnv_num_packets_written_default
  },
  {
    APP_NV_XNV_OTA_IN_PROGRESS, sizeof(nv_xnv_ota_in_progress_default), &nv_xnv_ota_in_progress_default
  },
  {
    APP_NV_XNV_OTA_UNIT_TIMER, sizeof(nv_xnv_ota_unit_timer_default), &nv_xnv_ota_unit_timer_default
  },  
  {
    APP_NV_XNV_OTA_REPEAT_COUNT_VALUE, sizeof(nv_xnv_ota_repeat_count_value_default), &nv_xnv_ota_repeat_count_value_default
  },  
  {
    APP_NV_CLEAN_ALL_NV_ITEMS, sizeof(nv_clean_all_nv_items_default), &nv_clean_all_nv_items_default
  },
  {
    APP_NV_COORD_RESET, sizeof(app_nv_coord_reset_default), &app_nv_coord_reset_default
  },
  {
    APP_NV_RADIO_SLEEP_TIMER, sizeof( nv_radio_sleep_timer_cnt_default ), &nv_radio_sleep_timer_cnt_default
  },
  {
    APP_NV_LAST_STARTUP_SLEEP_COUNT, sizeof( nv_last_startup_sleep_count_default ), &nv_last_startup_sleep_count_default
  },
  {
    APP_NV_APP_INSTANCE, sizeof( appInstance_default ), &appInstance_default
  },
  // Last item -- DO NOT MOVE IT!
  {
    0x00, 0, NULL
  }
};

// This table contains references to the RAM shadow values. All program code shall
// make use of these shadow variables for all practical purposes. - update for step 5
const appNVItemTab_t appNVItemTable[] =
{
  {
    APP_NV_UNIT_TIMER_VALUE, sizeof(nv_unit_timer_value), &nv_unit_timer_value, 
  },
  {
     APP_NV_REPEAT_COUNT_VALUE, sizeof(nv_repeat_count_value), &nv_repeat_count_value
  },
  {
     APP_NV_PACKET_SIZE, sizeof(nv_packet_size), &nv_packet_size
  },
  {
    APP_NV_PANLIST_IDX, sizeof(nv_panlist_idx), &nv_panlist_idx
  },
  {
    APP_NV_PANINFO_STRUCT, sizeof(NWInfo_t) * MAX_PANS_SCANNED, nv_pan_info_array
  },
  {
    APP_NV_GET_COORD_PARMS_FLAG, sizeof(nv_get_coord_parms_flag), &nv_get_coord_parms_flag
  },
  {
    APP_NV_COMMISSIONED_STATUS, sizeof(nv_commissioned_status), &nv_commissioned_status
  },
  {
    APP_NV_NUM_DISCOVERED_NWKS, sizeof(nv_num_discovered_nwks), &nv_num_discovered_nwks
  },
  {
    APP_NV_DEVICE_INFO_STRUCT, sizeof(nv_device_info), &nv_device_info
  },
  {
    APP_NV_XNV_PACKETS_WRITTEN, sizeof(nv_xnv_num_packets_written), &nv_xnv_num_packets_written
  },
  {
    APP_NV_XNV_OTA_IN_PROGRESS, sizeof(nv_xnv_ota_in_progress), &nv_xnv_ota_in_progress
  },
  {
    APP_NV_XNV_OTA_UNIT_TIMER, sizeof(nv_xnv_ota_unit_timer), &nv_xnv_ota_unit_timer
  },  
  {
    APP_NV_XNV_OTA_REPEAT_COUNT_VALUE, sizeof(nv_xnv_ota_repeat_count_value), &nv_xnv_ota_repeat_count_value
  },   
  {
    APP_NV_CLEAN_ALL_NV_ITEMS, sizeof(nv_clean_all_nv_items), &nv_clean_all_nv_items
  },
  {
    APP_NV_COORD_RESET, sizeof(nv_coord_reset), &nv_coord_reset
  },
  {
    APP_NV_RADIO_SLEEP_TIMER, sizeof(nv_radio_sleep_timer_cnt), &nv_radio_sleep_timer_cnt
  },
  {
    APP_NV_LAST_STARTUP_SLEEP_COUNT, sizeof( nv_last_startup_sleep_count ), &nv_last_startup_sleep_count
  },
  {
    APP_NV_APP_INSTANCE, sizeof( appInstance ), &appInstance
  },
  // Last item -- DO NOT MOVE IT!
  {
    0x00, 0, NULL
  }
};

static const uint16 appNVItemTable_size = sizeof(appNVItemTable) / sizeof(appNVItemTab_t);

#ifdef SYNERGY_BOOTLOADER
// Initialize the checksum shadow and image length used by the bootloader
#pragma location="CRC"
const CODE otaCrc_t OTA_CRC =
{
  0xFFFF,        // CRC
  0xFFFF,        // CRC Shadow
};
#pragma required=OTA_CRC
#endif  //SYNERGY_BOOTLOADER

/**************************************************************************************************
 *                                            LOCAL
 **************************************************************************************************/
byte PresenceSensor_TaskID; 

static uint8 sleepCount = 0;
static uint16 elapsedTurns = 0;
static uint8 rfShutdownCount = 0;
static uint8 coordInfoResponses = 0;
static uint8 BaseED_rf_shutdown_count = BaseED_RF_SHUTDOWN_REPEAT_COUNT;
static uint8 check_network_status_count;
static uint8 check_network_status_repeats;

uint16 numbytes = 0;
uint16 num_pkts = 0;
uint16 totalLQI = 0;
//uint8 lastLQI = 0;
int16 totalRSSI = 0;

static uint8 laps = 0;
static uint8 interPanMsgIndex = 0;

uint16 currentDataRate = 0x00;
uint8 currentLQIAvg = 0x00;
int16 currentRSSIAvg = 0x00;   //RSSI is a signed quantity

extern uint8  nwkJoinAttemptCount;   //This will keep track of how many times we have called ZDApp_StartJoiningCycle()

// This is defined in ZGlobals.c
//extern zgItem_t zgItemTable[];

static uint8 SerialED_RxBuf[ SERIAL_APP_TX_MAX + 1 ];

/**************************************************************************************************
 *                                        FUNCTIONS - Local
 **************************************************************************************************/
void ProjectSpecific_TestBitMap(void);
void BaseCoordinator_TestXNVMemory(void);
void ProjSpecific_InitNvItems(void);
void ProjSpecific_InitNvItemsToDefault(void);
void ProjSpecific_CleanAllNVItems(void);
uint8 APPNVItemInit(uint16 id, uint16 len, void *buf, uint8 setDefault);
uint8 SetAppNVItem(uint16 id, uint16 offset, void *buf);
uint8 GetAppNVItem(uint16 id, void *buf);
int FindNVItemIndex(uint16 id);

void ProjectSpecific_ProcessMTResp(mtOSALSerialData_t *MSGpkt);
void ProjectSpecific_ProcessAppSpecificMTReq(uint8 *mt_buffer, uint8 mt_packet_len, uint16 shortAddr);
void AppUDMT_SendMTRespWrapper(uint8 *mtbuff, uint8 mtbufflen, uint8 respType, bool isOTA);
void ProjSpecific_ScanforNetworks(void);
void *ZDO_NwkDiscCB(void *pBuff);
void *ZDO_NwkLeaveCB(void *pBuff);
void ProjectSpecific_PowerUpRadio(uint8 init, uint8 repeat);
void ProjectSpecific_PowerDownRadio(uint8 hold);
void ProjectSpecific_PowerDownRadioTimer(void);
void ProjectSpecific_TurnDownPolling(void);
void ProjectSpecific_TurnUpPolling(void);
void ProjSpecific_InitializePanList(void);
void ProjectSpecific_PlannedRestart(void);
void ProjectSpecific_StartCheckNwStatusEvt(uint8 delay);
void ProjectSpecific_CheckNetworkStatus(void);
void ProjectSpecific_JoinNextNw(void);
void ProjectSpecific_UpdatePanInfoArray(uint8 *paninfo_buff, uint8 bufflen);
void ProjectSpecific_ApplyJoinPolicy(void);
void PresenceSensor_HandleNwkStatusCheck(void);
static void ProjectSpecific_CallBack(uint8 port, uint8 event);
uint8 ProjectSpecific_GetCommissionStatus(void);
void ProjectSpecific_SendLeaveReq(void);
uint16 ProjectSpecific_UartWrite(uint8 port, uint8 *buf, uint16 len);
void ProjectSpecific_HexDump(uint8 *ptr, uint16 len);
static void ParseSerialCommand(void);
void ProjectSpecific_SendMTResp(uint8 *mtbuff, uint8 mtbufflen, uint8 respType);
void ProjectSpecific_InitAppInstance(uint8 taskid);
void ProjectSpecific_ChangeToOTAMode(void);
void ProjectSpecific_RestoreToNormalMode(void);
uint8 extPanIdEqual(uint8* devPanID, uint8* testPanID);
#ifdef INTER_PAN
  void BaseED_SendInterPanInitPackets(void);
#endif

/**************************************************************************************************
 * @fn      ProjSpecific_InitDevice
 *
 * @brief   This function gets called during initialization
 *
 * @param   none
 *
 * @return  None
 **************************************************************************************************/
void ProjSpecific_InitDevice( uint8 task_id )
{
  halUARTCfg_t uartConfig;
 
  // Record TaskID
  PresenceSensor_TaskID = task_id;
  
  // Turn off IDLE receive
  uint8 RxOnIdle = TRUE;
  ZMacSetReq( ZMacRxOnIdle, &RxOnIdle );
  
  uartConfig.configured           = TRUE;              // 2x30 don't care - see uart driver.
  uartConfig.baudRate             = HAL_UART_BR_57600; //HAL_UART_BR_38400; //HAL_UART_BR_115200;
  uartConfig.flowControl          = FALSE;
  uartConfig.flowControlThreshold = 64;                // 2x30 don't care - see uart driver.
  uartConfig.rx.maxBufSize        = 128;               // 2x30 don't care - see uart driver.
  uartConfig.tx.maxBufSize        = 128;               // 2x30 don't care - see uart driver.
  uartConfig.idleTimeout          = 6;                 // 2x30 don't care - see uart driver.
  uartConfig.intEnable            = TRUE;              // 2x30 don't care - see uart driver.
  uartConfig.callBackFunc         = ProjectSpecific_CallBack;
  HalUARTOpen (ZBC_PORT, &uartConfig);

  //SProjectSpecific_UartWrite(ZBC_PORT, (unsigned char *)MSG0, MSG0_LEN); 

  // We also need to tell Power manager that we are a battery powered device
  // ZDO does this only after joining, but we need to tell OSAL that we are
  // battery powered even when fcwe have not joined with any coordinator
  osal_pwrmgr_device(PWRMGR_BATTERY);
  ProjSpecific_InitNvItems();          //Init the NV items as our first order of business
  
  // After initializing the NV items, now check if we need to do a clean of all nv items
  uint8 cleanflag = false;
  GetAppNVItem(APP_NV_CLEAN_ALL_NV_ITEMS, &cleanflag);
  if (cleanflag == true)
  {
    ProjectSpecific_UartWrite(ZBC_PORT, "NTC\n\r", 5); 
    // Means we have set this flag to tell ourselves to clean all NV items
    ProjSpecific_CleanAllNVItems();
    
    // Now reinitialize all NV items to default values again. This time the cleanflag would be 
    // reset to false, because that is the default value
    ProjSpecific_InitNvItemsToDefault(); 
    
    // Need to reset security key here too so that the commissioning key is used
    uint8 commissioning_key[] = DEFAULT_KEY;
    osal_nv_write(ZCD_NV_PRECFGKEY, 0, SEC_KEY_LEN, commissioning_key);
    // Need to reset the PAN to commissioning PAN here
    uint16 cpan = COMMISSIONING_PAN;
    osal_nv_write(ZCD_NV_PANID, 0, osal_nv_item_len(ZCD_NV_PANID), &cpan);
      
    // The device will now powereup as a non-commissionined device. 
    osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_RESET_EVT, 5000);
  }
  else
  {
    // "NNTC" = Non Need To Clean
    ProjectSpecific_UartWrite(ZBC_PORT, "NNTC\n\r", 6); 
  }
  
  //Init the app instance structure
  ProjectSpecific_InitAppInstance(task_id);
  
  MT_RegisterAppTaskId(task_id); //Register our task ID with MT_TASK
  ANALED1_OFF();
  ANALED2_OFF();
  
  /*************************************************
   * Max asks: Can't all this go in BaseED_Init()? *
   *************************************************/
  
  // Device has just been commissioned using the C-App.  It's now ready to find
  // a coordinator to assocaite with.  Starts a scan, saves all visible PANs,
  // and joins the PAN with the highest LQI
  if(nv_commissioned_status == DEVICE_COMMISSIONED)
  {
    ProjectSpecific_UartWrite(ZBC_PORT, "DC\n\r", 4); 
    // Start timer that will initiate scanning of nearby PANs. But do this only if we have been already 
    // device commissioned. Without being device commissioned, the node will try to attach itself to the commissioning PAN.
#if RFD_RCVC_ALWAYS_ON==FALSE
    if (nv_coord_reset != COORD_RESET_PLANNED)
#endif
    {
      osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_SCAN_NETWORKS_EVT, PRESENCE_SCAN_NETWORKS_TIMER);
    }
  }

  // Device has found a PAN to join and is attempting to connect to it.  At this point,
  // the PAN is selected, but no messages are passing between end device and coordinator.
  // Once it successfully connects to the PAN, it will query all visible coordinators for
  // data statistics (should probably change this so it doesn't happen every time)
  else if(nv_commissioned_status == NETWORK_COMMISSIONING_IN_PROGRESS)
  {
    ProjectSpecific_UartWrite(ZBC_PORT, "CIP\n\r", 5);
    uint8 comm_flag = NETWORK_COMMISSIONED;
    SetAppNVItem(APP_NV_COMMISSIONED_STATUS, 0, &comm_flag);
  }
  
  // If this happens, it means the device was connected to a PAN or was attempting
  // to connect to a PAN but was reset.  Could be from a physical reset of the end device,
  // or from trying to send a message but not getting an acknowledgement from its coordinator.
  // Wait to reconnect to original coordinator or try to join a new PAN.
  else if (nv_commissioned_status == NETWORK_COMMISSIONED) {
    // Do we still need this?  Merge with NETWORK_COMMISSIONING_COMPLETED?
  }
  
  // Deprecated for now... figure out a way to work it back in
  else if(nv_commissioned_status == NETWORK_COMMISSIONING_COMPLETED)
  {
    ProjectSpecific_UartWrite(ZBC_PORT, "CC\n\r", 4); 
    // Since network commissioning is completed, mark the device as 'ACTIVE' so that
    // it can do whatever it was destined to do
    uint8 comm_stat = DEVICE_ACTIVE;
    SetAppNVItem(APP_NV_COMMISSIONED_STATUS, 0, &comm_stat);
    // This timer is needed only when the device is active, so we are setting this up after we know we are active
    osal_start_reload_timer(PresenceSensor_TaskID, PRESENCE_DATARATE_CALC_EVT, PRESENCE_DATARATE_SAMPLE_TIMER);
  }
  
  /**********************
   * End Max's question *
   **********************/
  
  if(nv_commissioned_status == DEVICE_ACTIVE)
  {
      osal_start_timerEx(task_id, BaseED_NWK_JOIN_STATUS_EVT, BaseED_NWK_JOIN_STATUS_TIMEOUT);
  }
  #if RFD_RCVC_ALWAYS_ON==FALSE
  else if (nv_commissioned_status == NON_COMMISSIONED || nv_coord_reset == COORD_RESET_PLANNED) {
    devState = DEV_HOLD;
    uint8 RxOnIdle = FALSE;
    ZMacSetReq( ZMacRxOnIdle, &RxOnIdle );
    
    // There is probably already a timer set to start network initialization
    // Turn it off before it starts the scanning process
    osal_stop_timerEx(ZDAppTaskID, ZDO_NETWORK_INIT);
    //initOnRadioPowerUp = 1;
  }
  #endif
  
  
  /* Start a timer to check whether we need to find a new PAN to join
   * (length of timer depends on nv_coord_reset flag
   */
  if (nv_commissioned_status != NON_COMMISSIONED) {
    if (nv_coord_reset == COORD_RESET_PLANNED) {
      #ifdef DEBUG
      //ProjectSpecific_UartWrite(ZBC_PORT, "Planned Rst\r\n", 13);
      ProjectSpecific_UartWrite(ZBC_PORT, "Sleeping for: ", 14);
      ProjectSpecific_HexDump((uint8*) &nv_radio_sleep_timer_cnt, 2);
      #endif
      
      //Set event to turn radio on after time specified by nv radio off timer length.      
      #if RFD_RCVC_ALWAYS_ON==FALSE
      osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_RADIO_ON_EVT, RADIO_SLEEP_TIMER_DEFAULT );
      #endif
    }
    else if (nv_coord_reset == COORD_RESET_NORMAL) {
      #ifdef DEBUG
      ProjectSpecific_UartWrite(ZBC_PORT, "Normal Rst\r\n", 12);
      #endif
      ProjectSpecific_StartCheckNwStatusEvt(PRESENCE_NORMAL_RESET_DELAY);
    }
    else {
      #ifdef DEBUG
      ProjectSpecific_UartWrite(ZBC_PORT, "Immediate Rst\r\n", 15);
      #endif
      ProjectSpecific_StartCheckNwStatusEvt(0);
    }
  }
  
  /* After every reset, coord reset flag should return to default */
  uint8 rst = APP_NV_COORD_RESET_DEFAULT;
  SetAppNVItem(APP_NV_COORD_RESET, 0, &rst);

  {
  uint16 pan = 0;
  uint8 strbuff[6] = {0};
  pan = zgConfigPANID;
  _itoa(pan, strbuff, 16);
  ProjectSpecific_UartWrite(ZBC_PORT, "\n\r****\n\r", 8); 
  ProjectSpecific_UartWrite(ZBC_PORT, strbuff, 4); 
  ProjectSpecific_UartWrite(ZBC_PORT, "\n\r****\n\r", 8); 
  }

  //Hack for now
  /*if(nv_commissioned_status == DEVICE_ACTIVE)
  {
      osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_GATHER_NW_PARMS_EVT, 15000);
      ANALED2_ON();
  }*/

  ProjSpecific_InitializePanList();
  //BaseCoordinator_TestXNVMemory();
  //ProjectSpecific_TestBitMap();
}

void ProjectSpecific_CheckNetworkStatus() {
  if (get_nwk_status() != NWK_JOINED) {
    ProjectSpecific_UartWrite(ZBC_PORT, "Not joined!\r\n", 13);
    uint8 comm_flag;
    if (nv_get_coord_parms_flag) {
      comm_flag = NETWORK_COMMISSIONING_IN_PROGRESS;
    }
    else {
      comm_flag = DEVICE_COMMISSIONED;
    }

    SetAppNVItem(APP_NV_COMMISSIONED_STATUS, 0, &comm_flag);
    osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_GATHER_NW_PARMS_EVT, PRESENCE_GATHER_NW_PARMS_TIMER);
  }
  else {
    ProjectSpecific_UartWrite(ZBC_PORT, "Still joined!\r\n", 15); 
  }
}

// Delay is in 15 second increments
void ProjectSpecific_StartCheckNwStatusEvt(uint8 delay) {
  check_network_status_count = 0;
  if (delay) {
    check_network_status_repeats = delay;
    osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_CHECK_NETWORK_STATUS_EVT, PRESENCE_CHECK_NETWORK_STATUS_TIMER);
  }
  else {
    check_network_status_repeats = 0;
    osal_set_event(PresenceSensor_TaskID, PRESENCE_CHECK_NETWORK_STATUS_EVT);
  }
}

/*********************************************************************
 * @fn      ProjSpecific_ZDO_state_change
 *
 * @brief   Project specific function when ZDO statechange happens
 *
 */
void ProjSpecific_ZDO_state_change( void )
{
  //uint8 status = 0;
  //uint16 newval = 2;
  static uint16 sA = 0;
  uint8 strbuff[6] = {0};
  
  ProjectSpecific_UartWrite(ZBC_PORT, "SChange\n\r", 9);
  
#ifdef TESTBED_ANAREN_DEVICE
  osal_start_timerEx(PresenceSensor_TaskID, BaseED_TESTBED_PERIODIC_PACKET_EVT, nv_unit_timer_value);
#endif TESTBED_ANAREN_DEVICE
  
#if RFD_RCVC_ALWAYS_ON==FALSE
  if (nv_commissioned_status == NON_COMMISSIONED)   
  {
    //Turn on the LED to indicate that it has associated

    osal_stop_timerEx(PresenceSensor_TaskID, BaseED_RF_SHUTDOWN_EVT);
    osal_stop_timerEx(PresenceSensor_TaskID, BaseED_TOGGLE_LED_EVT);
    uint8 rxOnIdle = TRUE;
    ZMacSetReq( ZMacRxOnIdle, &rxOnIdle );
    HAL_TURN_ON_LED_PRESENCE();
    ProjectSpecific_PowerDownRadioTimer();
  }
  uint16 count = 1;
  SetAppNVItem(APP_NV_LAST_STARTUP_SLEEP_COUNT, 0, &count);
#endif
  
  uint16 pan;
  osal_nv_read( ZCD_NV_PANID, 0, sizeof( pan ), &pan );
  
  if (nv_commissioned_status == DEVICE_ACTIVE)
  {
   ANALED2_ON();
   ProjectSpecific_UartWrite(ZBC_PORT, "ACT\n\r", 5);
   
   sA = nv_unit_timer_value;
   _itoa(sA, strbuff, 10);
   ProjectSpecific_UartWrite(ZBC_PORT, "\n\rXXX\n\r", 8); 
   ProjectSpecific_UartWrite(ZBC_PORT, strbuff, 4); 
   ProjectSpecific_UartWrite(ZBC_PORT, "\n\rXXX\n\r", 8); 
   
   // Also start the timer to send out the device announce packet
   osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_TIMER_DEV_ANNOUNCE_EVT, 3000);
  }
  
  else if (nv_commissioned_status == NETWORK_COMMISSIONED && nv_get_coord_parms_flag)
  {
    // Start timer that will send out packet with 'init' packet
    uint16 initJitter = Onboard_rand() | PRESENCE_SEND_COORD_INIT_PACKET_TIMER;
    osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_SEND_COORD_INIT_PACKET_EVT, initJitter);
    ANALED2_ON();
    ProjectSpecific_UartWrite(ZBC_PORT, "INP\n\r", 5);
  }
  else if (appInstance.otaStatus != NOT_IN_PROGRESS) { 
    if (appInstance.panid == pan) {
      if (gpacketbitmap)
      {
          BM_FREE(gpacketbitmap);
          gpacketbitmap = NULL;
      }
      gtotalpackets = appInstance.totalpackets;
      gpacketbitmap = BM_ALLOC(appInstance.totalpackets);
      XNV_Read(appInstance.imgarea, FLASH_IMAGE_TYPE_OTA_BITMAP, gpacketbitmap, 0, BM_MAX_BYTES(appInstance.totalpackets));
    
      switch (appInstance.otaStatus) {
        case IN_PROGRESS:
          ProjectSpecific_ChangeToOTAMode();
          uint16 unitTimer;
          GetAppNVItem(APP_NV_XNV_OTA_UNIT_TIMER, &unitTimer);
          osal_start_timerEx(appInstance.appTaskid, PRESENCE_TIMER_OTA_TIMEOUT_EVT, unitTimer);
          break;
        case FILLING_MISSING_PACKETS:
          AppUDMT_XNVEndPacketTransferRsp(NULL, true);
          break;
      }
    }
    else {
      appInstance.otaStatus = NOT_IN_PROGRESS;
      SetAppNVItem(APP_NV_APP_INSTANCE, 0, &appInstance);
    }

  }
    
  /*
  //sA = NLME_GetShortAddr();
  _itoa(sA, strbuff, 16);
  ProjectSpecific_UartWrite(ZBC_PORT, "\n\r****\n\r", 8); 
  ProjectSpecific_UartWrite(ZBC_PORT, strbuff, 4); 
  ProjectSpecific_UartWrite(ZBC_PORT, "\n\r****\n\r", 8); 
  */
}


/*********************************************************************
 * @fn      ProjectSpecific_ProcessEvent
 *
 * @brief   Project specific events can be placed here. 
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events   - Bit map of events to process.
 *
 * @return  Event flags of all unprocessed events.
 */
UINT16 ProjectSpecific_ProcessEvent( UINT16 events )
{
    
  if(events & PRESENCE_TIMER_DEV_ANNOUNCE_EVT)
  {
    AppUDMT_SendDeviceAnnounceCmd(true);  //this sends out the device announce packet
    return (events ^ PRESENCE_TIMER_DEV_ANNOUNCE_EVT);
  }
  
  if(events & PRESENCE_RESET_EVT)
  {
    SystemReset();
    return (events ^ PRESENCE_RESET_EVT);
  }
  
  if (events & PRESENCE_RADIO_ON_EVT)
  {
    GetAppNVItem(APP_NV_RADIO_SLEEP_TIMER, &nv_radio_sleep_timer_cnt );
    
    // If this is not the last repitition, store decremented value of count back to NV
    // And set a new timer of the same length.
    if ( (--nv_radio_sleep_timer_cnt) != 0 )
    {
      SetAppNVItem( APP_NV_RADIO_SLEEP_TIMER, 0, &nv_radio_sleep_timer_cnt );
      
      ProjectSpecific_UartWrite(ZBC_PORT, "keep sleeping: ", 15);
      ProjectSpecific_HexDump((uint8*) &nv_radio_sleep_timer_cnt, 2);
      osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_RADIO_ON_EVT, RADIO_SLEEP_TIMER_DEFAULT );
    }
    else
    {
      // Turn on radio and initialize device
      ProjectSpecific_PowerUpRadio(1, 0);
      //start a timer to see if it should join a new PAN
      ProjectSpecific_UartWrite(ZBC_PORT, "pwr up!\r\n", 9);
      if (nv_commissioned_status == DEVICE_COMMISSIONED) {
        osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_SCAN_NETWORKS_EVT, PRESENCE_SCAN_NETWORKS_TIMER);
      }
      else {
        ProjectSpecific_StartCheckNwStatusEvt(PRESENCE_NORMAL_RESET_DELAY);
      }
    }
  }
  
  if (events & PRESENCE_SEND_COORD_INIT_PACKET_EVT) 
  {
    interPanMsgIndex = 0;
    coordInfoResponses = 0;
    uint8 parmsFlag = 0;
    SetAppNVItem(APP_NV_GET_COORD_PARMS_FLAG, 0, &parmsFlag);
    ProjectSpecific_TurnUpPolling();
    ProjectSpecific_PowerUpRadio(0, 2);
    osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_SEND_INTER_PAN_INIT_EVT, PRESENCE_SEND_INTER_PAN_INIT_TIMER);
    osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_GATHER_NW_PARMS_EVT, 30000);
    return (events ^ PRESENCE_SEND_COORD_INIT_PACKET_EVT);
  }
  
  if (events & PRESENCE_SEND_INTER_PAN_INIT_EVT) {
    StubAPS_RegisterApp((endPointDesc_t *)&BaseED_epDesc);
    // We store channel using a bit in a 32 bit field, StubAPS requires it as an 8 bit integer
    uint32 channelBit = nv_pan_info_array[interPanMsgIndex].channel;
    uint8 channel = 0;
    while (channelBit & 0xFFFFFFFE) {
      channelBit = channelBit >> 1;
      ++channel;
    }
    ZStatus_t chanResult = StubAPS_SetInterPanChannel(channel);
    ProjectSpecific_UartWrite(ZBC_PORT, "Chan Change: ", 13);
    ProjectSpecific_HexDump(&chanResult, 1);
    
    uint16 panid = nv_pan_info_array[interPanMsgIndex].panID;
    uint16 addr = nv_pan_info_array[interPanMsgIndex].address;
    
    uint16 devicePan;
    osal_nv_read( ZCD_NV_PANID, 0, sizeof( devicePan ), &devicePan ); 
    
    // Here we send out a packet that will ask the coordinator to send their 'info'
    // packet. Which is nothing but a UD-MT packet.
    uint8 initreq[7];    
    initreq[0] = MT_SOF;
    initreq[1] = 0x02;
    initreq[2] = 0x2B;
    initreq[3] = 0x08;
    initreq[4] = LO_UINT16(devicePan);
    initreq[5] = HI_UINT16(devicePan);
    initreq[6] = CalcFCS(initreq+1, MT_UD_HDR_LEN + 2);
    
    #ifdef DEBUG
    ProjectSpecific_UartWrite(ZBC_PORT, "INIT: ", 6);
    ProjectSpecific_HexDump((uint8*) &(nv_pan_info_array[interPanMsgIndex].panID), 2);
    #endif //DEBUG

    #ifdef INTER_PAN
    BaseED_SendInterPan(CORECOMMS_CLUSTER, END_DEVICE_MESSAGE_TYPE_OTA_MT_REQ, 
                0,
                panid,
                addr,
                7, 
                initreq);
    StubAPS_SetIntraPanChannel();
    #endif //INTER_PAN
    
    ++interPanMsgIndex;
    if (interPanMsgIndex < nv_num_discovered_nwks) {
      osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_SEND_INTER_PAN_INIT_EVT, PRESENCE_SEND_INTER_PAN_INIT_TIMER);
    }

    return (events ^ PRESENCE_SEND_INTER_PAN_INIT_EVT);
  }

  if (events & PRESENCE_CHECK_NETWORK_STATUS_EVT) {
    ++check_network_status_count;
    if (check_network_status_count < check_network_status_repeats) {
      osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_CHECK_NETWORK_STATUS_EVT, PRESENCE_CHECK_NETWORK_STATUS_TIMER);
    }
    else {
      ProjectSpecific_CheckNetworkStatus();
    }
    return events ^ PRESENCE_CHECK_NETWORK_STATUS_EVT;
  }
  
  if(events & PRESENCE_GATHER_NW_PARMS_EVT)
  {
    //ANALED2_OFF();
    ProjectSpecific_UartWrite(ZBC_PORT, "LNW\n\r", 5); 
    // First we leave current network, if we already have joined one
    if (get_nwk_status() == NWK_JOINED)
    {
      //ANALED1_ON();
      //ZDO_RegisterForZdoCB(ZDO_LEAVE_CNF_CBID, ZDO_NwkLeaveCB);
      //ProjectSpecific_SendLeaveReq();
      ProjectSpecific_UartWrite(ZBC_PORT, "NNW=J\n\r", 7); 
      ProjectSpecific_ApplyJoinPolicy();
    }
    else
    {
      // Since we are not connected to any network and don't have to do a leave, we can
      // directly start the JoinNext process here.
      ProjectSpecific_UartWrite(ZBC_PORT, "NNW=NJ\n\r", 8); 
      ProjectSpecific_JoinNextNw();
    }
    return (events ^ PRESENCE_GATHER_NW_PARMS_EVT);
  }
  
#ifdef TESTBED_ANAREN_DEVICE
  if (events & BaseED_TESTBED_PERIODIC_PACKET_EVT) {
    static uint32 seq = 0;
    uint8 *payload = osal_mem_alloc(21);
    uint16 addr = NLME_GetShortAddr();
    payload[0]  = 2;
    payload[1]  = 1;
    payload[2]  = 0x12;
    payload[3]  = 0x34;
    payload[4]  = LO_UINT16(addr);
    payload[5]  = HI_UINT16(addr);
    payload[6]  = LO_UINT16(nv_device_info.deviceId);
    payload[7]  = HI_UINT16(nv_device_info.deviceId);
    payload[8]  = BREAK_UINT32(seq, 0);
    payload[9]  = BREAK_UINT32(seq, 1);
    payload[10] = BREAK_UINT32(seq, 2);
    payload[11] = BREAK_UINT32(seq, 3);
    payload[12] = 17;
    osal_memcpy( payload + 13, appInstance.extaddr, 8 );
    BaseED_Send( DEVSPECIFIC_CLUSTER, DEV_SPECIFIC_OCCUPANCY_MSG, 0, 21, payload );
    osal_mem_free( payload );
   
    ++seq;
    osal_start_timerEx(PresenceSensor_TaskID, BaseED_TESTBED_PERIODIC_PACKET_EVT, nv_unit_timer_value);
    return events ^ BaseED_TESTBED_PERIODIC_PACKET_EVT;
  }
  
#else
  if (events & BaseED_RF_SHUTDOWN_EVT) {
    if (rfShutdownCount < BaseED_rf_shutdown_count - 1) {
      ++rfShutdownCount;
      ProjectSpecific_UartWrite(ZBC_PORT, "rfShutdownCount: ", 17);
      ProjectSpecific_HexDump(&rfShutdownCount, 1);
      osal_start_timerEx(PresenceSensor_TaskID, BaseED_RF_SHUTDOWN_EVT, BaseED_RF_SHUTDOWN_TIMEOUT);
    }
    else {
      #ifdef DEBUG
      ProjectSpecific_UartWrite(ZBC_PORT, "RF Shutdown\r\n", 13);
      #endif
      
      osal_stop_timerEx(PresenceSensor_TaskID, BaseED_TOGGLE_LED_EVT);

      #if RFD_RCVC_ALWAYS_ON==FALSE  //TODO Move these to project specific

      ProjectSpecific_TurnDownPolling();
      ProjectSpecific_PowerDownRadio(1);

      #endif
      
      BaseED_rf_shutdown_count = BaseED_RF_SHUTDOWN_REPEAT_COUNT;
    }

    return (events ^ BaseED_RF_SHUTDOWN_EVT);
  }
#endif TESTBED_ANAREN_DEVICE
  
  if (events & BaseED_TOGGLE_LED_EVT) {
    #if RFD_RCVC_ALWAYS_ON==FALSE
    HAL_TOGGLE_LED_PRESENCE();
    #endif
    osal_start_timerEx(PresenceSensor_TaskID, BaseED_TOGGLE_LED_EVT, BaseED_TOGGLE_LED_TIMER);
    return (events ^ BaseED_TOGGLE_LED_EVT);
  }
  
  if (events & PRESENCE_SCAN_NETWORKS_EVT)
  {
    // Register for the Callback first
    ZDO_RegisterForZdoCB(ZDO_NWK_DISCOVERY_CNF_CBID, ZDO_NwkDiscCB);
    ProjSpecific_ScanforNetworks();
    return (events ^ PRESENCE_SCAN_NETWORKS_EVT);
  }
  
  if (events & PRESENCE_TIMER_OTA_TIMEOUT_EVT)
  {
    #define OTA_MAX_MISSING_PACKET_ATTEMPTS 3600 // Define this somewhere else
    elapsedTurns++;
    if (gtotalMissingPackets && elapsedTurns < OTA_MAX_MISSING_PACKET_ATTEMPTS)
    {
      AppUDMT_XNVGetNextMissingImgBlock();
    }
    else if(elapsedTurns >= nv_xnv_ota_repeat_count_value)
    {
      elapsedTurns = 0;
      appInstance.otaStatus = NOT_IN_PROGRESS;
      SetAppNVItem(APP_NV_APP_INSTANCE, 0, &appInstance);
      ProjectSpecific_UartWrite(ZBC_PORT, "OAD Fill-in aborted!\n", 21);
    
      //Step1: Turn OFF the MAC for idle
      //Step2: restore the broadcast parameters
      //Step3: restore the polling rate
      ProjectSpecific_RestoreToNormalMode();    
    }
    else
    {
      //Start the timer for the next iteration
      osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_TIMER_OTA_TIMEOUT_EVT, nv_xnv_ota_unit_timer);
    }
    return (events ^ PRESENCE_TIMER_OTA_TIMEOUT_EVT);
  }
   
  if (events & BaseED_NWK_JOIN_STATUS_EVT)
  {
    ProjectSpecific_UartWrite(ZBC_PORT, (unsigned char *)MSG1, MSG1_LEN);  
    PresenceSensor_HandleNwkStatusCheck();                //we call function to check our network status, i.e. whether we are joined to a coordinator or not.
    return (events ^ BaseED_NWK_JOIN_STATUS_EVT);
  }
  
  if (events & BaseED_NWK_JOIN_RETRY_EVT)
  {
    sleepCount++;
    if (sleepCount >= BaseED_NWK_JOIN_RETRY_REPEAT_COUNT)
    {
        sleepCount = 0;  //Need to reset this for the next iteration
        // Start the timer again, to check the status after sometime
        ProjectSpecific_UartWrite(ZBC_PORT, (unsigned char *)MSG2, MSG2_LEN); 
        osal_start_timerEx(PresenceSensor_TaskID, BaseED_NWK_JOIN_STATUS_EVT, BaseED_NWK_JOIN_STATUS_TIMEOUT);
        uint8 RxOnIdle = TRUE;
        ZMacSetReq( ZMacRxOnIdle, &RxOnIdle );
        ZDApp_StartJoiningCycle();
        nwkJoinAttemptCount++;  //now increment the counter which counts how many times we have tried to join our network
    }
    else
    {
        // Start the timer for the next interval
        osal_start_timerEx(PresenceSensor_TaskID, BaseED_NWK_JOIN_RETRY_EVT, BaseED_NWK_JOIN_RETRY_TIMEOUT);
    }
    return (events ^ BaseED_NWK_JOIN_RETRY_EVT);
  }
  
  // we need to calculate data rate again. store it in currentDataRate.
  if ( events & PRESENCE_DATARATE_CALC_EVT )
  {
    laps++;
    if (laps >= PRESENCE_DATARATE_SAMPLE_PERIOD)
    {
      unsigned short drate = 0;
      unsigned short lqi = 0;
      short rssi = 0;
      laps = 0; // reset for next iteration
      // Our sampling rate is every 32 seconds  
      
      // drate
      drate = numbytes / 5;
      currentDataRate = drate;
      numbytes = 0;  // reset numbytes for next iteration
      
      // lqi
      lqi = totalLQI / 5;
      currentLQIAvg = lqi;
      totalLQI = 0;  // reset totalLQI for next iteration
      
      // rssi
      rssi = totalRSSI / 5;
      currentRSSIAvg = rssi;
      totalRSSI = 0;  // reset totalRSSI for next iteration
    }
    return ( events ^ PRESENCE_DATARATE_CALC_EVT );
  }  
  
  return 0;  // discard unknown events   
}

/*********************************************************************
 * @fn      ProjectSpecific_ProcessSystemEvent
 *
 * @brief   Project specific system events can be placed here. 
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events   - Bit map of events to process.
 *
 * @return  Event flags of all unprocessed events.
 */
void ProjectSpecific_ProcessSystemEvent( afIncomingMSGPacket_t *MSGpkt )
{
  switch ( MSGpkt->hdr.event )
  {
    case MT_SYS_SERIAL_MSG:
      //ProjectSpecific_UartWrite(ZBC_PORT, "\n\rOTA\n\r", 7);
      //ANALED2_TOGGLE();
      ProjectSpecific_ProcessMTResp( (mtOSALSerialData_t *)MSGpkt );
      break;
  case MT_SYS_UD_CMD:
      //ProjectSpecific_UartWrite(ZBC_PORT, "\n\rOTA-UD\n\r", 10);
      #ifdef DEBUG
      ProjectSpecific_UartWrite(ZBC_PORT, "MTUD Cmd", 8);
      #endif //DEBUG
      AppUDMT_ProcessMTUDCmd( (mtOSALSerialData_t *)MSGpkt );
  default:
      break;
  } 
  return;  
}

/**************************************************************************************************
 * @fn      AppUDMT_SendMTRespWrapper
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void AppUDMT_SendMTRespWrapper(uint8 *mtbuff, uint8 mtbufflen, uint8 respType, bool isOTA)
{
  if (isOTA)
  {        
    BaseED_Send(CORECOMMS_CLUSTER, END_DEVICE_MESSAGE_TYPE_OTA_MT_RESP, 0,
                mtbufflen, mtbuff);
  }
  else
  {
    ProjectSpecific_SendMTResp(mtbuff, mtbufflen, respType);
  }
}

/*********************************************************************
 * @fn      PresenceSensor_HandleNwkStatusCheck
 *
 * @brief   This function will check the network status by querying the
 *          baseED
 *
 *
 * @return  None
 */
 void PresenceSensor_HandleNwkStatusCheck(void)
 {
   uint8 nwk_status = get_nwk_status();
   uint8 status;
   if(nwk_status != NWK_JOINED)
   {
     // If we have surpassed the required number of attempts to find a network we simply go to default settings and reboot.
     // If not, we go to sleep by calling ZDApp_StopJoiningCycle() and then setting a timer to wakeup and call ZDApp_StartJoiningCycle()
#if 0     
/*     
     if (nwkJoinAttemptCount >= PRESENCE_NWK_REJOIN_TRY_COUNT)
     {
       //don't do the reverting business for now
       // Here we simply change the NV items to make them default PAN and default channel list and do a hard reboot.
       // No need to reset the counter nwkJoinAttemptCount because the hard reboot will reset it. But for safety we can do it here.
       // Sometimes being paranoid helps.
       nwkJoinAttemptCount = 0;
       
       // First, set the PAN in NV to wildcard 0xFFFF
       uint16 defPAN = 0xFFFF;
       osal_nv_write(ZCD_NV_PANID, 0, osal_nv_item_len( ZCD_NV_PANID ), &defPAN);
       
       // Second, set the channel list to default channel list, the value with which it was compiled
       uint32 defChanlist = DEFAULT_CHANLIST;
       osal_nv_write(ZCD_NV_CHANLIST, 0, osal_nv_item_len( ZCD_NV_CHANLIST ), &defChanlist);
       
       // Third, just do a hard reboot so that the device will be able to latch onto whatever network it finds after the reboot
       osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_RESET_EVT, PRESENCE_RESET_TIMER);
     }
     else
*/       
#endif
     {  
       // We need to call ZDApp_StopJoiningCycle() here
       uint8 RxOnIdle = FALSE;
       ZMacSetReq( ZMacRxOnIdle, &RxOnIdle );
       status = ZDApp_StopJoiningCycle();
       // Max asks: why this if-else?
       if (status == TRUE)
       {
         // Start another timer now, after which we will call ZDApp_StartJoiningCycle()
         ProjectSpecific_UartWrite(ZBC_PORT, "ZZZ\n\r", 5);
         osal_start_timerEx(PresenceSensor_TaskID, BaseED_NWK_JOIN_RETRY_EVT, BaseED_NWK_JOIN_RETRY_TIMEOUT);  // start timer to wake up and to retry joining the network again
       }
       else
       {
         //SProjectSpecific_UartWrite(ZBC_PORT, (unsigned char *)MSG4, MSG4_LEN);
         osal_start_timerEx(PresenceSensor_TaskID, BaseED_NWK_JOIN_RETRY_EVT, BaseED_NWK_JOIN_RETRY_TIMEOUT);
       }
     }
   }
   else
   {
     ProjectSpecific_UartWrite(ZBC_PORT, (unsigned char *)MSG3, MSG3_LEN);
   }
 }

/*********************************************************************
 * @fn      ProjectSpecific_ProcessCoreMsg
 *
 * @brief  This processes all device specific communication messages 
 *
 * @param  afIncomingMSGPacket_t *pkt - the packet that contains the msg
 */
void ProjectSpecific_ProcessCoreMsg( afIncomingMSGPacket_t *pkt, uint16 shortAddr )
{
  // Here we inspect the incoming packet and act on it. This being an ED
  // Almost all incoming packets will be admin packets to act upon.
  uint8 *mt_buffer = NULL;
  uint8  mt_packet_len = 0;

  if(pkt->cmd.Data[0] == SYNCBYTE_1 && pkt->cmd.Data[1] == SYNCBYTE_2)
  {
    uint16 recvCkSum = BUILD_UINT16(pkt->cmd.Data[pkt->cmd.DataLength - 1],
                                    pkt->cmd.Data[pkt->cmd.DataLength - 2]);
    if (recvCkSum == CalcCkSum(pkt->cmd.Data, pkt->cmd.DataLength - 2)) {
      mt_packet_len = pkt->cmd.Data[13];
      mt_buffer = osal_mem_alloc(mt_packet_len);
      #if DEBUG > 1
      ProjectSpecific_UartWrite(ZBC_PORT, "\r\nCmd msg: ", 11);
      ProjectSpecific_HexDump(&(pkt->cmd.Data[14]), pkt->cmd.DataLength);
      #endif
      #ifdef DEBUG
      ProjectSpecific_UartWrite(ZBC_PORT, "\r\nLQI: ", 7);
      ProjectSpecific_HexDump(&(pkt->LinkQuality), 1);
      #endif //DEBUG
      if (mt_buffer) {
        osal_memcpy(mt_buffer, &pkt->cmd.Data[14],mt_packet_len);       //TODO: change the hardcode for 14
        if(pkt->cmd.Data[3] == END_DEVICE_MESSAGE_TYPE_OTA_MT_REQ)
        {
          if((mt_buffer[2] & MT_RPC_SUBSYSTEM_MASK) < MT_RPC_SYS_MAX)
          {
            // Means we are within the range of well established MT sub systems
            MT_UartProcessOTAZToolData(mt_buffer, mt_packet_len, shortAddr, pkt->srcAddr.panId);
          }
        }
        else
        if (pkt->cmd.Data[3] == END_DEVICE_MESSAGE_TYPE_OTA_MT_RESP)  
        {
          ProjectSpecific_UartWrite(ZBC_PORT, "UPLLLLLLL\n\r", 11);
          if (interPanMsgIndex < nv_num_discovered_nwks) {
            // We got the response packet sooner than expected -- send out the next init packet
            osal_stop_timerEx(PresenceSensor_TaskID, PRESENCE_SEND_INTER_PAN_INIT_EVT);
            osal_set_event(PresenceSensor_TaskID, PRESENCE_SEND_INTER_PAN_INIT_EVT);
          }
          // ProjectSpecific_SendLeaveReq();
          // Means we have received the reply to our "init" packet. So we need to
          // update our PAN info struct in the NV and move on to the next PAN
          ProjectSpecific_UpdatePanInfoArray(mt_buffer, mt_packet_len);
        }
        osal_mem_free(mt_buffer);
      }
    }
  }
}
/**************************************************************************************************
 * @fn      ProjectSpecific_ProcessAppSpecificMTReq
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void ProjectSpecific_ProcessAppSpecificMTReq(uint8 *mt_buffer, uint8 mt_packet_len, uint16 shortAddr)
{
  //uint8 len = mt_buffer[1];
  if (mt_buffer[0] == MT_UART_SOF)
  {

  }
  else
  {
    //TODO: Send error message
  }
}

/*********************************************************************
 * @fn     ProjectSpecific_ProcessDevSpecificMsg
 *
 * @brief  This processes all device specific communication messages 
 *
 * @param  afIncomingMSGPacket_t *pkt - the packet that contains the msg
 */
void ProjectSpecific_ProcessDevSpecificMsg( afIncomingMSGPacket_t *pkt, uint16 shortAddr )
{
  /* INDEV */
  /* Send received OTA message from coordinator across serial to attached
     energy meter for processing.
  */
  unsigned char packet[4] = {1,2,3,4};
  unsigned short recvd_device_id = (unsigned short)(pkt->cmd.Data[4]<<8) + pkt->cmd.Data[5];
  //if (pkt->cmd.Data[2] == DEV_SPECIFIC_SERIAL_CMD)
  if ( (recvd_device_id == DEVICE_ID) || (recvd_device_id == 0) )
  {
    packet[0] = 0x6E;
    packet[1] = pkt->cmd.Data[6];
    packet[2] = pkt->cmd.Data[7];
    packet[3] = 0x6F;
    
    // Send data to energy meter attached to serial port
    ProjectSpecific_UartWrite( ZBC_PORT, (unsigned char *)packet, 4);
  }
}

/**************************************************************************************************
 * @fn      ProjectSpecific_CallBack
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
static void ProjectSpecific_CallBack(uint8 port, uint8 event)
{
  (void)port;
  if (event & (HAL_UART_RX_FULL | HAL_UART_RX_ABOUT_FULL | HAL_UART_RX_TIMEOUT))
  {
    ParseSerialCommand();
  }
}

/**************************************************************************************************
 * @fn      ProjectSpecific_InitAppInstance
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
//Initialize the project specific app structure
void ProjectSpecific_InitAppInstance(uint8 taskid)
{
  appInstance.appTaskid = taskid;
  osal_memcpy(appInstance.extaddr, NLME_GetExtAddr(), Z_EXTADDR_LEN); 
  appInstance.pollRate = zgPollRate;
  appInstance.queuedPollRate = zgQueuedPollRate;
  appInstance.responsePollRate = zgResponsePollRate;
  //Add other variables as and when they get added to appInstance
}

/**************************************************************************************************
 * @fn      ProjectSpecific_TestBitMap
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
//test function for bitmap manipulation
void ProjectSpecific_TestBitMap(void)
{
  /*
  gtotalpackets = 4096;
  gpacketbitmap = BM_ALLOC(gtotalpackets);
  BM_ALL(gpacketbitmap, 0, gtotalpackets);
  
  //BM_SET(gpacketbitmap, 100);
  
  bool r = BM_TEST(gpacketbitmap, 100);
  bool s = BM_TEST(gpacketbitmap, 101);
  uint16 missingidx;
  static bool k =  false;
  k = XNV_GetFirstPacketIndex(0, &missingidx);
  k =  XNV_GetFirstPacketIndex(1, &missingidx);
  
  if(k)
    BM_FREE(gpacketbitmap);
  */
}

/**************************************************************************************************
 * @fn      BaseCoordinator_TestXNVMemory
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
// Test function for SPI_XNV
// We just write and read back a buffer and compare the result.
// to check that XNV memory does work.
// This has to be called twice.
void BaseCoordinator_TestXNVMemory(void)
{
  /*
  static uint8 count = 0;
  static uint8 buf[4] = {0xDE, 0xAD, 0xBA, 0xBE};
  static uint8 readbuf[64] = {0};
  static uint8 ret = 0;
  static uint8 ret2 = 0;
  */
  
  /*
  //HalSPIRead(0x360, readbuf, 64);
  
  // By this time all NV items have been initialized. So we first delete an item and try to initialize it again
  uint16 val;
  ret = osal_nv_item_init(APP_NV_UNIT_TIMER_VALUE, 2, &val);
  ret2 = osal_nv_delete(APP_NV_UNIT_TIMER_VALUE, 2);
  ret = osal_nv_item_init(APP_NV_UNIT_TIMER_VALUE, 2, &val);
  */
  
  
  /*
  if ((count % 2) == 0)
  {
    // we write for even numbers
    HalSPIWrite(0x255, buf, 4);
  }
  else
  {
    // we read for odd numbers
    HalSPIRead(0x255, readbuf, 4);

    if (readbuf[0] == buf[0] &&
        readbuf[1] == buf[1] &&
        readbuf[2] == buf[2] &&
        readbuf[3] == buf[3] 
        )
    {
      ANALED2_TOGGLE();
    }
  }
  
  count++;
  */
}

/**************************************************************************************************
 * @fn      ProjectSpecific_CleanAllNVItems
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
// Calling this function deletes ALL level NV items, including ZigBee specific NV items too
void ProjSpecific_CleanAllNVItems(void)
{
  // First delete all ZigBee level NV items
  zgDeleteItems();
  uint8 i = 0;
  // Now delete the app level NV items
  while(appNVItemTable[i].id != 0x00)
  {
    // Delete the item
    osal_nv_delete(appNVItemTable[i].id, appNVItemTable[i].len);
    // Move on to the next item
    i++;
  }
}


/**************************************************************************************************
 * @fn      ProjectSpecic_InitNvItems
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
// Function top initialize all NV items from the NV memory
// Has to be called to initialized variables which will be later used
// in the program. If this is not called, then variables will continue
// to use default values, which probably you don't want
void ProjSpecific_InitNvItems(void)
{
  uint8  i = 0;
 
  while ( appNVItemTable[i].id != 0x00 )
  {
    // Initialize the item
    APPNVItemInit(appNVItemTable[i].id, appNVItemTable[i].len, appNVItemTable[i].buf, false);

    // Move on to the next item
    i++;
  }
}

/**************************************************************************************************
 * @fn      ProjectSpecific_InitNvItemsToDefault
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void ProjSpecific_InitNvItemsToDefault(void)
{
  uint8  i = 0;
 
  while ( appNVItemTable[i].id != 0x00 )
  {
    // Initialize the item
    APPNVItemInit(appNVItemTable[i].id, appNVItemTable[i].len, (void *)appNVItemDefaultValuesTable[i].defvalue, true);

    // Move on to the next item
    i++;
  }
}

/**************************************************************************************************
 * @fn      APPNVItemInit
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
/* 
The way this function works is as follows:
First we check if the NV item already exists or not. If it does not exist by returning
NV_ITEM_UNINIT, the NV item gets created with the default value and we use that value
in our code.

If it does it returns SUCCESS then there are two possibilities:
      if setDefault is true we overwrite the older value in NV
      with the default value. This is like a restore to default values.

      if setDefault is false, we just read the value present in NV 

*/
uint8 APPNVItemInit( uint16 id, uint16 len, void *buf, uint8 setDefault )
{
  uint8 status;

  // If the item doesn't exist in NV memory, create and initialize
  // it with the value passed in.
  status = osal_nv_item_init( id, len, buf );
  if ( status == ZSUCCESS )
  {
    if ( setDefault )
    {
      // Write the default value back to NV
      status = osal_nv_write( id, 0, len, buf );
    }
    else
    {
      // The item exists in NV memory, read it from NV memory
      status = osal_nv_read( id, 0, len, buf );
    }
  }
  // no else part needed because in both cases (SUCCESS or NV_ITEM_UNINIT) we
  // fall back to the default value

  return status;
}

/**************************************************************************************************
 * @fn      FindNVItemIndex
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
// -1 means NV item is not found
int FindNVItemIndex(uint16 id)
{
  uint8  i = 0;
  int retidx = -1;
  while (appNVItemTable[i].id != 0x00 )
  {
    if (appNVItemTable[i].id == id)
    {
      retidx = i;
      break;
    }
    i++;
  }
  return retidx;
}

/**************************************************************************************************
 * @fn      SetAppNVItem
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
/* Just passing the NV item id and the data to this function enables it to 
 * set the NV item that we want to change. The caller has to make sure that
 * the buf points to a correct sized buffer properly allocated - dynamically or statically
 */
uint8 SetAppNVItem(uint16 id, uint16 offset, void *buf)
{
  uint8 idx = 0;
  uint8 status = FAILURE;
  for(idx = 0; idx < appNVItemTable_size; idx++)
  {
    if(appNVItemTable[idx].id == id)
    {
      // Matching id found
      status = osal_nv_item_init( id, appNVItemTable[idx].len, buf);
      if ( status == ZSUCCESS )
      {
        status = osal_nv_write( id, offset, appNVItemTable[idx].len, buf );
        // Now make sure the copy in RAM is updated
        if ( status == ZSUCCESS )
        {
          osal_memcpy(appNVItemTable[idx].buf, buf, appNVItemTable[idx].len);
        }
      }
      break;
    }
  }
  return status;
}

/**************************************************************************************************
 * @fn      GetAppNVItem
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
/* Just passing the NV item id and the data to this function enables it to 
 * get the NV item that we want to change. The caller has to make sure that
 * the buf points to a correct sized buffer properly allocated -dynamically or statically.
 * The NV item value is returned in buf.
 */
uint8 GetAppNVItem(uint16 id, void *buf)
{
  uint8 idx = 0;
  uint8 status = FAILURE;
  for(idx = 0; idx < appNVItemTable_size; idx++)
  {
    if(appNVItemTable[idx].id == id)
    {
      // Matching id found
      status = osal_nv_read( id, 0, appNVItemTable[idx].len, buf);
      
      break;
    }
  }
  return status;
}

/**************************************************************************************************
 * @fn      ProjectSpecific_ProcessMTResp
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void ProjectSpecific_ProcessMTResp( mtOSALSerialData_t *MSGpkt )
{
  static uint8 len = 0;
  if(MSGpkt->isOTA)
  {
    //5 = MT_MAGIC_LEN(1) + MT_LEN(1) + MT_CMD_LEN(2) + CRC_LEN(1);
    len = ((unsigned char *)MSGpkt->msg)[1] + 5;  
    
    //MTRespPacket = osal_mem_alloc(total_len);   
    BaseED_Send(CORECOMMS_CLUSTER, END_DEVICE_MESSAGE_TYPE_OTA_MT_RESP, 
                0,
                len, 
                (unsigned char *)MSGpkt->msg);
  }
  else
  {
    // Send via serial port, in case the ED is connected via serial
   ProjectSpecific_SendMTResp( MSGpkt->msg, len, SERIAL_MT_CMD_RESPONSE );
  }
}

void ProjectSpecific_TurnDownPolling() {
    // Restore poll rates to previous state
    zgPollRate = appInstance.pollRate;
    zgQueuedPollRate = appInstance.queuedPollRate;
    zgResponsePollRate = appInstance.responsePollRate;
    NLME_SetPollRate(appInstance.pollRate);
    NLME_SetQueuedPollRate(appInstance.queuedPollRate);
    NLME_SetResponseRate(appInstance.responsePollRate);
}

void ProjectSpecific_TurnUpPolling() {
  // Save poll rates and turn polling way up
  appInstance.pollRate = zgPollRate;
  appInstance.queuedPollRate = zgQueuedPollRate;
  appInstance.responsePollRate = zgResponsePollRate;
  NLME_SetPollRate(60);
  NLME_SetQueuedPollRate(60);
  NLME_SetResponseRate(60);
}

/*
 * @fn      ProjectSpecific_PowerUpRadio (uint8 init, uint8 repeat)
 * 
 * @brief   Activates Anaren receiver and puts the device in an INIT state.  Starts a
 *          timer to power down the receiver after BaseED_RF_SHUTDOWN_TIMEOUT x
 *          BaseED_RF_SHUTDOWN_COUNT ms.
 * @param   init - if != 0, will put device into DEV_INIT state and call ZDOInitDevice()
 * @param   repeat - number of 15 second intervals to wait before powering radio back down
 *                  if 0, will use default (BaseED_RF_SHUTDOWN_REPEAT_COUNT)
 */

void ProjectSpecific_PowerUpRadio(uint8 init, uint8 repeat) {
  
  // Power up receiver
  uint8 rxOnIdle = TRUE;
  ZMacSetReq( ZMacRxOnIdle, &rxOnIdle );
  
  // Start the LED flashing
  osal_stop_timerEx(PresenceSensor_TaskID, BaseED_TOGGLE_LED_EVT);
  osal_start_timerEx(PresenceSensor_TaskID, BaseED_TOGGLE_LED_EVT, BaseED_TOGGLE_LED_TIMER);
  
  if (init) {
    static uint8 initComplete = FALSE;
    if (initComplete == FALSE) {
      // Get the device into INIT mode
      initComplete = TRUE;
      devState = DEV_INIT;
      ZDOInitDevice( 0 );
    }
  }
  
  rfShutdownCount = 0;
  if (repeat > 0) {
    BaseED_rf_shutdown_count = repeat;
  }
  // Start the shutdown timer
  osal_stop_timerEx(PresenceSensor_TaskID, BaseED_RF_SHUTDOWN_EVT);
  if (repeat != 0xFFFF) {
    osal_start_timerEx(PresenceSensor_TaskID, BaseED_RF_SHUTDOWN_EVT, BaseED_RF_SHUTDOWN_TIMEOUT);
  }
}

void ProjectSpecific_PowerDownRadio(uint8 hold) {
  osal_stop_timerEx(PresenceSensor_TaskID, BaseED_TOGGLE_LED_EVT); 
  osal_stop_timerEx(PresenceSensor_TaskID, BaseED_RF_SHUTDOWN_EVT);
  
  #if RFD_RCVC_ALWAYS_ON == FALSE
  uint8 rxOnIdle = FALSE;
  ZMacSetReq( ZMacRxOnIdle, &rxOnIdle );
  #endif
  #ifdef HAL_TURN_OFF_LED_PRESENCE
  HAL_TURN_OFF_LED_PRESENCE();
  #endif
  if (hold) {
    SystemReset();
  }
}

/*
 * @fn      ProjectSpecific_PowerDownRadioTimer ()
 * 
 * @brief   Starts a timer for an event that will turn off the receiver
 */
void ProjectSpecific_PowerDownRadioTimer() {

  rfShutdownCount = 0;  
  osal_start_timerEx(PresenceSensor_TaskID, BaseED_RF_SHUTDOWN_EVT, BaseED_RF_SHUTDOWN_TIMEOUT);
}

/**************************************************************************************************
 * @fn      ProjSpecific_ScanforNetworks
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void ProjSpecific_ScanforNetworks(void)
{
  /*
  zAddrType_t destAddr;
  destAddr.addrMode = (afAddrMode_t)Addr16Bit;
  destAddr.addr.shortAddr = 0;

  uint32 scanChannels = 0x00004000;
  byte scanDuration = 1;
  byte startIndex = 0;
  ZDP_MgmtNwkDiscReq( &destAddr, scanChannels, scanDuration, startIndex, 0);
  */
  //uint32 scanChannels = 0x00004000;
  NLME_ScanFields_t scaninfo;
  scaninfo.channels = 0x4000; //MAX_CHANNELS_24GHZ;
  scaninfo.duration = BEACON_ORDER_4_SECONDS;
  scaninfo.scanType = ZMAC_ACTIVE_SCAN;
  scaninfo.scanApp = NLME_DISC_SCAN;
    
  ProjectSpecific_UartWrite(ZBC_PORT, "SC\n\r", 4); 
  NLME_NwkDiscReq2(&scaninfo);
  
  //NLME_NetworkDiscoveryRequest(scanChannels, BEACON_ORDER_1_SECOND);
}

uint8 extPanIdEqual(uint8* devPanID, uint8* testPanID)
{
  int i;
  if (devPanID == NULL || testPanID == NULL) {
    return 0;
  }
  
  for (i = 0; i < Z_EXTADDR_LEN; ++i) {
    if (devPanID[i] != testPanID[i]) {
      return 0;
    }
  }
  return 1;
}

/**************************************************************************************************
 * @fn      ZDO_NwkDiscCB
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void *ZDO_NwkDiscCB(void *pBuff)
{
  byte i = 0;
  byte nwCount = 0;
  networkDesc_t *pList = NULL;
  networkDesc_t *NetworkList = NULL;
  NWInfo_t *foundNWList = NULL;
  uint8* coordExtPanID;

  NetworkList = pList = nwk_getNwkDescList();
  
  ProjectSpecific_UartWrite(ZBC_PORT, "SCCB\n\r", 6); 
  
  // Count the number of PANs found after scanning
  while (pList)
  {
    if (pList->panId != COMMISSIONING_PAN && extPanIdEqual(ZDO_UseExtendedPANID, pList->extendedPANID))
      nwCount++;           // We are only concerned with networks that are NOT commissioning PANs
    pList = pList->nextDesc;
    ProjectSpecific_UartWrite(ZBC_PORT, "NWF\n\r", 5);
#if DEBUG > 2
    ProjectSpecific_HexDump((uint8*)pList, 20);
#endif //DEBUG
  }
  
  if (nwCount == 0)
  {
    //ANALED1_ON();
    //ANALED2_ON();
    ProjectSpecific_UartWrite(ZBC_PORT, "NW=0\n\r", 6);
    ProjectSpecific_PlannedRestart();
    SystemReset();
    //osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_RESET_EVT, PRESENCE_RESET_TIMER);
  }
  else
  {
    ProjectSpecific_UartWrite(ZBC_PORT, "NW>=1\n\r", 7); 

    // Since we have found some networks, we can now transition to IN_PROGRESS    
    uint8 comm_flag = NETWORK_COMMISSIONING_IN_PROGRESS;
    SetAppNVItem(APP_NV_COMMISSIONED_STATUS, 0, &comm_flag);
    
    
    //Allocate our array for storing the found PANs
    foundNWList = osal_mem_alloc(nwCount * sizeof(NWInfo_t));
    osal_memset(foundNWList, 0x00, nwCount * sizeof(NWInfo_t));
    
    i = 0;
    while(NetworkList)
    {
      if(NetworkList->panId != COMMISSIONING_PAN && extPanIdEqual(ZDO_UseExtendedPANID, NetworkList->extendedPANID))
      {
        //TODO Fill foundNWList in with useful information (rssi, lqi, nassoc, etc)
        foundNWList[i].panID = NetworkList->panId;
        foundNWList[i].address = NetworkList->chosenRouter;
        foundNWList[i].lqi = NetworkList->chosenRouterLinkQuality;
        foundNWList[i].channel = (0x00000001 << NetworkList->logicalChannel);
        i++;
      }
      NetworkList = NetworkList->nextDesc;
    }
    
      // Selection Sort the foundNWList array of length nwCount.
    {
      uint8 j = 0;
      uint8 iMax = 0;
      NWInfo_t tmp;
      i = 0;
      
      while ( j < nwCount - 1 )
      {
        iMax = j;
        i = j + 1;
        while ( i < nwCount )
        {
          // compare to see if element is the new min.
          if ( foundNWList[i].lqi > foundNWList[iMax].lqi)
          {
            iMax = i;
          }
          // increment i.
          ++i;
        }
        //if the iMin is not j, swap elements.
        if ( iMax != j )
        {
          tmp = foundNWList[j];
          foundNWList[j] = foundNWList[iMax];
          foundNWList[iMax] = tmp;
        }        
        // increment j.
        ++j;
      }
    }
  
    
    //ANALED1_ON();
    // Set the total number of discovered networks in NV
    SetAppNVItem(APP_NV_NUM_DISCOVERED_NWKS, 0, &nwCount);
    
    
    // Now update the NV array with the parameters of all discovered PANs for upto
    // MAX_PANS_SCANNED number of PANs
    
    // This initializes whole array in NV to zeroes
    osal_nv_write(APP_NV_PANINFO_STRUCT, 0, sizeof(nv_pan_info_default_array), (void *)nv_pan_info_default_array);
    // This writes the discovered networks information to NV
    for(i = 0; i < nwCount; i++)
    {
      // osal_nv_write(APP_NV_PANINFO_STRUCT, i * sizeof(NWInfo_t)+ osal_offsetof(NWInfo_t, panID), sizeof(foundNWList[i].panID), &foundNWList[i].panID);
      osal_nv_write(APP_NV_PANINFO_STRUCT, i * sizeof(NWInfo_t), sizeof(NWInfo_t), &foundNWList[i]);
    }
    
    // Now sync the entire struc array by reading back into the shadow RAM array
    // nv_pan_info_array can now be used like a normal variable to iterate through the found PANs
    osal_nv_read(APP_NV_PANINFO_STRUCT, 0, sizeof(nv_pan_info_array), nv_pan_info_array);
    
    // Start timer which will make us join some network - later this will move to some kind of policy 
    //osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_SEND_COORD_INIT_PACKET_EVT, PRESENCE_SEND_COORD_INIT_PACKET_TIMER);
    //osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_JOIN_A_NETWORK_EVT, PRESENCE_JOIN_A_NETWORK_TIMER);
    ProjectSpecific_UartWrite(ZBC_PORT, "SJT\n\r", 5);
    osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_GATHER_NW_PARMS_EVT, PRESENCE_GATHER_NW_PARMS_TIMER);
  }
  
  // Call this to terminate our scanning endeavor
  NLME_NwkDiscTerm();
    
  // Also de-register because we don't want this cb to be called for future discovery requests
  ZDO_DeregisterForZdoCB(ZDO_NWK_DISCOVERY_CNF_CBID);
  
  // Free foundNWList
  osal_mem_free( foundNWList );
  
  return NULL;
}

#ifdef INTER_PAN
/**************************************************************************************************
 * @fn      BaseED_SendInterPanInitPackets()
 *
 * @brief   Sends init packets to all coordinators in nv_pan_info_array
 *
 * @param   None
 *
 * @return  None
 **************************************************************************************************/
void BaseED_SendInterPanInitPackets() {
  //TODO  Use INTER_PAN transmission
  //TODO  Error checking?
  return;
  StubAPS_RegisterApp((endPointDesc_t *)&BaseED_epDesc);
  int panIdx;
  for (panIdx = 0; panIdx < nv_num_discovered_nwks; ++panIdx) {
    // We store channel using a bit in a 32 bit field, StubAPS requires it as an 8 bit integer
    uint32 channelBit = nv_pan_info_array[panIdx].channel;
    uint8 channel = 0;
    while (channelBit & 0xFFFFFFFE) {
      channelBit = channelBit >> 1;
      ++channel;
    }
    
    uint16 panid = nv_pan_info_array[panIdx].panID;
    uint16 addr = nv_pan_info_array[panIdx].address;
    //ZStatus_t chanResult = StubAPS_SetInterPanChannel(channel);
    //ProjectSpecific_UartWrite(ZBC_PORT, "Chan Change: ", 13);
    //ProjectSpecific_HexDump(&chanResult, 1);
    
    //ANALED1_ON();
    // Here we send out a packet that will ask the coordinator to send their 'info'
    // packet. Which is nothing but a UD-MT packet.
    uint8 initreq[] = {0};            //TODO: remove hardcoding
    
    initreq[0] = MT_SOF;
    initreq[1] = 0x00;
    initreq[2] = 0x2B;
    initreq[3] = 0x08;
    initreq[4] = CalcFCS(initreq+1, MT_UD_HDR_LEN);
    
    #ifdef DEBUG
    ProjectSpecific_UartWrite(ZBC_PORT, "INIT_: ", 7);
    ProjectSpecific_HexDump((uint8*) &(nv_pan_info_array[panIdx].panID), 2);
    #endif //DEBUG
    
    
    //[SAM] this is in the for loop.  immediately after we are done it sets t
    BaseED_SendInterPan(CORECOMMS_CLUSTER, END_DEVICE_MESSAGE_TYPE_OTA_MT_REQ, 
                0,
                panid,
                addr,
                5, 
                initreq);
  }
  StubAPS_SetIntraPanChannel();
}
#endif //INTER_PAN

/**************************************************************************************************
 * @fn      ProjectSpecific_JoinNextNw
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
/*
  This is the function which will go over the found PANs one by one 
  and connect to each of them in sequence.
*/
void ProjectSpecific_JoinNextNw(void)
{
  uint8 idx = 0;
  uint16 nextPanID = nv_pan_info_array[nv_panlist_idx].panID;
  uint32 defChanlist = DEFAULT_CHANLIST;
  
#ifdef DEBUG
  ProjectSpecific_UartWrite(ZBC_PORT, "idx: ", 5);
  ProjectSpecific_HexDump(&nv_panlist_idx, 1);
  ProjectSpecific_UartWrite(ZBC_PORT, "num disc: ", 10);
  ProjectSpecific_HexDump(&nv_num_discovered_nwks, 1);
#endif //DEBUG
  
  if (nv_panlist_idx >= nv_num_discovered_nwks) {
    // Reset panlist_idx to 0
    SetAppNVItem(APP_NV_PANLIST_IDX, 0, &idx);
    
    ProjectSpecific_PlannedRestart();
    /*
    // Put device into DEVICE_COMMISSIONED mode (will initiate network discovery)
    // This will also ensure that APP_NV_GET_COORD_PARMS_FLAG == 1 (in ProjSpecific_InitializePanList())
    uint8 comm_stat = DEVICE_COMMISSIONED;  
    SetAppNVItem(APP_NV_COMMISSIONED_STATUS, 0, &comm_stat);
    
    // Call this a planned restart so it stays in low-power mode for a bit
    uint8 rst = COORD_RESET_PLANNED;
    SetAppNVItem(APP_NV_COORD_RESET, 0, &rst);
    
    // set counter for number of 15 second sleep cycles should be performed.
    uint8 count = 4;
    SetAppNVItem(APP_NV_RADIO_SLEEP_TIMER, 0, &count);
    */

  }
  else {
    //Update the new index in NV first so that on the next reboot, we get the 
    //updated index value
    idx = nv_panlist_idx + 1;
    SetAppNVItem(APP_NV_PANLIST_IDX, 0, &idx);
  
    // Also update the PAN id/Channel so that on reboot we latch onto that network
    osal_nv_write(ZCD_NV_PANID, 0, osal_nv_item_len( ZCD_NV_PANID ), &nextPanID);
    osal_nv_write(ZCD_NV_CHANLIST, 0, osal_nv_item_len( ZCD_NV_CHANLIST ), &defChanlist);
  
    // This will ensure that the entry in the assoc list of the coordinator is cleaned up
    // ProjectSpecific_SendLeaveReq();
  }
  ProjectSpecific_UartWrite(ZBC_PORT, "RST=NNW\n\r", 9);
  //uint8 comm_stat = NETWORK_COMMISSIONING_COMPLETED;  
  //SetAppNVItem(APP_NV_COMMISSIONED_STATUS, 0, &comm_stat);
  osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_RESET_EVT, PRESENCE_RESET_TIMER);
}

/**************************************************************************************************
 * @fn      ZDO_NwkLeaveCB
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void *ZDO_NwkLeaveCB(void *pBuff)
{
  // So at this point we are not connected to any coordinator. So we can just 
  // get the next network in the list of PANs and join with it. We also have
  // an array of PAN ids in the NV which we can read and join one by one.
  
  // de-registering
  ZDO_DeregisterForZdoCB(ZDO_LEAVE_CNF_CBID);
  
    
  // Reset for changes to take effect
  ProjectSpecific_UartWrite(ZBC_PORT, "RST=AP\n\r", 8); 
  osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_RESET_EVT, PRESENCE_RESET_TIMER);
  
  // Starting the sequence of network joins
  //ProjectSpecific_ApplyJoinPolicy();
  
  
/*  
  uint16 nextPAN = foundNWList[nv_panlist_idx].panID;
  nextPAN = 0x1043;
  osal_nv_write(ZCD_NV_PANID, 0, osal_nv_item_len( ZCD_NV_PANID ), &nextPAN);

  // Second, increment the panlist index and store it in NV for the next reset
  nv_panlist_idx++;
  osal_nv_write(APP_NV_PANLIST_IDX, 0, osal_nv_item_len(APP_NV_PANLIST_IDX), &nv_panlist_idx);
  
  // Third, set the channel list to default channel list, the value with which it was compiled
  uint32 defChanlist = DEFAULT_CHANLIST;
  osal_nv_write(ZCD_NV_CHANLIST, 0, osal_nv_item_len( ZCD_NV_CHANLIST ), &defChanlist);
  
  // Fourth, just do a hard reboot so that the device will be able to latch onto 
  // the network we set in the NV after it reboots
  //osal_start_timerEx(PresenceSensor_TaskID, PRESENCE_RESET_EVT, PRESENCE_RESET_TIMER);
  
  // de-registering
  ZDO_DeregisterForZdoCB(ZDO_LEAVE_CNF_CBID);
  
  ZDO_StartDevice( (uint8)ZDO_Config_Node_Descriptor.LogicalType, MODE_REJOIN,
                   BEACON_ORDER_NO_BEACONS, BEACON_ORDER_NO_BEACONS );
  
*/  
  return NULL;
}

void ProjectSpecific_PlannedRestart() {
    // Put device into DEVICE_COMMISSIONED mode (will initiate network discovery)
    // This will also ensure that APP_NV_GET_COORD_PARMS_FLAG == 1 (in ProjSpecific_InitializePanList())
    uint8 comm_stat = DEVICE_COMMISSIONED;  
    SetAppNVItem(APP_NV_COMMISSIONED_STATUS, 0, &comm_stat);
    
    // Call this a planned restart so it stays in low-power mode for a bit
    uint8 rst = COORD_RESET_PLANNED;
    SetAppNVItem(APP_NV_COORD_RESET, 0, &rst);
    
    // set counter for number of 15 second sleep cycles should be performed.
    uint16 count;
    GetAppNVItem(APP_NV_LAST_STARTUP_SLEEP_COUNT, &count);
    count = (count << 1 > MAX_STARTUP_SLEEP_COUNT) ? MAX_STARTUP_SLEEP_COUNT : count << 1;
    SetAppNVItem(APP_NV_LAST_STARTUP_SLEEP_COUNT, 0, &count);
    SetAppNVItem(APP_NV_RADIO_SLEEP_TIMER, 0, &count);
}

/**************************************************************************************************
 * @fn      ProjSpecific_InitializePanList
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void ProjSpecific_InitializePanList(void)
{
  uint8 pflag_on = 1;
  uint8 startidx = 0;
  uint8 paninfobuff[MAX_PANS_SCANNED * sizeof(NWInfo_t)] = {0};
  if (nv_commissioned_status == DEVICE_COMMISSIONED)
  {
    // We set the dirty flag for first power up indication
    SetAppNVItem(APP_NV_GET_COORD_PARMS_FLAG, 0, &pflag_on);
    
    // Initialize the PANinfo array index which we would be using later, with zero
    SetAppNVItem(APP_NV_PANLIST_IDX, 0, &startidx);
    
    // Initialize the PANinfo array itself too, like the array index with all zeroes
    SetAppNVItem(APP_NV_PANINFO_STRUCT, 0, paninfobuff);
  }
  else
  {
     // Do nothing because we need to initialize the PANinfo array only if we are
     // powering up for the first time.
  }
}

/*
typedef struct NWInfo
{
  uint16 panID;
  uint32 channel;
  int16 rssi;
  uint16 lqi;
  uint16 nassoc;
  uint16 drate;
} NWInfo_t;
*/

/**************************************************************************************************
 * @fn      ProjectSpecific_UpdatePanInfoArray
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void ProjectSpecific_UpdatePanInfoArray(uint8 *paninfo_buff, uint8 bufflen)
{
  // uint8 idx = nv_panlist_idx - 1; // We do the -1 because by now, the idx is already incremented for the next iteration
  
  uint16 panid = BUILD_UINT16(paninfo_buff[5], paninfo_buff[4]);
  uint32 channel = BUILD_UINT32(paninfo_buff[9], paninfo_buff[8], paninfo_buff[7], paninfo_buff[6]);
  int16 rssi = BUILD_UINT16(paninfo_buff[11], paninfo_buff[10]);
  uint16 lqi = BUILD_UINT16(paninfo_buff[13], paninfo_buff[12]);
  uint16 numassoc = BUILD_UINT16(paninfo_buff[15], paninfo_buff[14]);
  uint16 drate =   BUILD_UINT16(paninfo_buff[17], paninfo_buff[16]);
  
  #ifdef DEBUG
  ProjectSpecific_UartWrite(ZBC_PORT, "PanInfo: ", 9);
  ProjectSpecific_HexDump((uint8*)&panid, 2);
  #endif //DEBUG
  
  uint8 idx;
  for (idx = 0; idx < nv_num_discovered_nwks; ++idx) {
    if (nv_pan_info_array[idx].panID == panid) {
      break;
    }
  }
  
  if (idx == nv_num_discovered_nwks) {
    return;
  }
  
  // Write all PAN specific information into NV here
  /* Let's make sure all this data is good before writing it
  osal_nv_write(APP_NV_PANINFO_STRUCT, idx * sizeof(NWInfo_t)+ osal_offsetof(NWInfo_t, panID), sizeof(panid), &panid);
  nv_pan_info_array[idx].panID = panid;
    
  osal_nv_write(APP_NV_PANINFO_STRUCT, idx * sizeof(NWInfo_t)+ osal_offsetof(NWInfo_t, channel), sizeof(channel), &channel);
  nv_pan_info_array[idx].channel = channel;
    
  osal_nv_write(APP_NV_PANINFO_STRUCT, idx * sizeof(NWInfo_t)+ osal_offsetof(NWInfo_t, rssi), sizeof(rssi), &rssi);
  nv_pan_info_array[idx].rssi = rssi;
    
  osal_nv_write(APP_NV_PANINFO_STRUCT, idx * sizeof(NWInfo_t)+ osal_offsetof(NWInfo_t, lqi), sizeof(lqi), &lqi);
  nv_pan_info_array[idx].lqi = lqi;
  */
    
  if (numassoc >= 1)
  {
    numassoc--;
  }
  
  osal_nv_write(APP_NV_PANINFO_STRUCT, idx * sizeof(NWInfo_t)+ osal_offsetof(NWInfo_t, nassoc), sizeof(numassoc), &numassoc);
  nv_pan_info_array[idx].nassoc = numassoc;
    
  osal_nv_write(APP_NV_PANINFO_STRUCT, idx * sizeof(NWInfo_t)+ osal_offsetof(NWInfo_t, drate), sizeof(drate), &drate);
  nv_pan_info_array[idx].drate = drate;
  
  // Now sync the entire struc array by reading back into the shadow RAM array
  // nv_pan_info_array can now be used like a normal variable to iterate through the found PANs
  osal_nv_read(APP_NV_PANINFO_STRUCT, 0, sizeof(nv_pan_info_array), nv_pan_info_array);
  
  ++coordInfoResponses;
  
  // We've heard back from all our coordinators; stop the timeout and start the restart event right away
  if (coordInfoResponses >= nv_num_discovered_nwks) {
    osal_stop_timerEx(PresenceSensor_TaskID, PRESENCE_GATHER_NW_PARMS_EVT);
    osal_set_event(PresenceSensor_TaskID, PRESENCE_GATHER_NW_PARMS_EVT);
  }
}

/**************************************************************************************************
 * @fn      ProjectSpecific_ApplyJoinPolicy
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void ProjectSpecific_ApplyJoinPolicy(void)
{
  uint8 comm_stat = NETWORK_COMMISSIONING_COMPLETED;  
  uint16 finalPanID = 0;
  uint32 finalChanlist = DEFAULT_CHANLIST;
  uint16 maxLQI = 0;
  const uint16 assocDevCost = 4;

  uint8 i;  
  for (i = 0; i < nv_num_discovered_nwks; ++i) {
    uint16 lqi = nv_pan_info_array[i].lqi;
    uint16 cost = nv_pan_info_array[i].nassoc * assocDevCost;
    lqi = (lqi > cost) ? lqi - cost : 1;
    
    if (lqi > maxLQI) {
      maxLQI = lqi;
      finalPanID = nv_pan_info_array[i].panID;
      finalChanlist = nv_pan_info_array[i].channel;
    }
  }
  
  /*
  uint8 lqi_level = 0;
  uint8 lqi_idx;
  uint8 chosenLvl = 0;
  uint8 chosenIdx = 0;
  //map of found coordinators.
  NWInfo_t lqi_map[NUM_LQI_PARTITION_LVLS][NUM_DEVS_PER_LQI_PARTITION] = {0};
  uint8 lqi_partition_lens[ NUM_LQI_PARTITION_LVLS ] = {0};
  
  // Test policy: pick PAN with least associated devices. In practice, some kind of
  // parameterized formula which also takes into account the RSSI and LQI should be 
  // applied to figure out which index to use.
  
  //ASSUMES that pan info array is already sorted by lqi
  //sort elements into partitioned levels in map, based on their lqi.
  for (i = 0; i < nv_num_discovered_nwks; i++)
  {
    lqi_level = NUM_LQI_PARTITION_LVLS;
    //Find partition level this element should live on.
    while (lqi_level > 0)
    {
      // Note that max lqi is 0x100.
      if ( (nv_pan_info_array[i].lqi < (lqi_level * (0x100)/(NUM_LQI_PARTITION_LVLS))) 
           && (nv_pan_info_array[i].lqi >= ((lqi_level-1) * (0x100)/(NUM_LQI_PARTITION_LVLS))) )
      {
        // Get the current index for a partition.
        //TODO this might not work
        lqi_idx = lqi_partition_lens[lqi_level - 1];
        
        // boundary check on the row
        if ( lqi_idx < NUM_DEVS_PER_LQI_PARTITION )
        {
          // Store on the next index, as index 0 counts the length
          lqi_map[lqi_level - 1][lqi_idx++] = nv_pan_info_array[i];
          lqi_partition_lens[lqi_level - 1] = lqi_idx;
        }
        else // partition is full.  see if we want this coordinator more than any others.
        {
          
          for ( lqi_idx = 0; lqi_idx < NUM_DEVS_PER_LQI_PARTITION; ++lqi_idx )
          {
            if( nv_pan_info_array[i].nassoc < lqi_map[lqi_level-1][lqi_idx].nassoc)
            {
              lqi_map[lqi_level-1][lqi_idx] = nv_pan_info_array[i];
              break;
            }
          }
        }
        break;
      }
      else
      {
        --lqi_level;
      }
    }
  }
  
  // The devices are now partitioned by lqi
  
  //move through partitions looking for the lowest association count in the highest LQI category
  lqi_level = chosenLvl = NUM_LQI_PARTITION_LVLS - 1;
  lqi_idx = chosenIdx = 0;
  uint8 minFoundFlag = FALSE;
  
  while( TRUE )
  {
    // boundary check and NOT NULL TEST
    if ( lqi_idx < NUM_DEVS_PER_LQI_PARTITION && lqi_map[lqi_level][lqi_idx].panID != 0x0000  )
    {
      minFoundFlag = TRUE; //we found something, so we'll ultimately find the minimum on this level.
      
      //Initially, set the chosenIdx to the first coordinator it can find.
      //NOT NULL TEST
      if ( lqi_map[lqi_level][chosenIdx].panID == 0x0000 )
      {
        chosenIdx = lqi_idx;
      }
      else if ( lqi_map[lqi_level][lqi_idx].nassoc < lqi_map[lqi_level][chosenIdx].nassoc)
      {
         //lqi_map[lqi_level ][lqi_index++];
        chosenIdx = lqi_idx;
      }
      //increment index as we continue search on this level
      ++lqi_idx;
    }
    else //nothing else in the row.
    {
      if(minFoundFlag)
      {
        break;
      }
      else
      {
        //force move to lower level in search
        lqi_idx =  NUM_DEVS_PER_LQI_PARTITION;
      }
    }
    // We have reached the end of the partition without finding a coordinator.
    // Move to next lower layer.
    if ( lqi_idx >= NUM_DEVS_PER_LQI_PARTITION )
    {
      lqi_idx = 0;
      // If we've depleted the entire mapping and found no elements.
      if (lqi_level-- == 0)
      {
        break;
      }
    }
  }
  
    
  // At this point we know which coordinator to join with.
  
  // Choose coordinator info for join.
  finalPanID = lqi_map[lqi_level][chosenIdx].panID;
  finalChanlist = lqi_map[lqi_level][chosenIdx].channel;
  
  */
  
  #ifdef DEBUG
  ProjectSpecific_UartWrite(ZBC_PORT, "AP: ", 4); 
  ProjectSpecific_HexDump((uint8*) &finalPanID, 2);
  #endif
  
  uint16 pan;
  osal_nv_read( ZCD_NV_PANID, 0, sizeof( pan ), &pan );
  
  // If we're already joined to the best network, just return now
  if (pan == finalPanID) {
    #if RFD_RCVC_ALWAYS_ON==FALSE  //TODO Move these to project specific
    ProjectSpecific_TurnDownPolling();
    ProjectSpecific_PowerDownRadio(0);
    #endif
    return;
  }
  
  // Set the final PANid and channel in NV
  osal_nv_write(ZCD_NV_PANID, 0, osal_nv_item_len( ZCD_NV_PANID ), &finalPanID);
  osal_nv_write(ZCD_NV_CHANLIST, 0, osal_nv_item_len( ZCD_NV_CHANLIST ), &finalChanlist);
  
  // Set flag to indicate completion of network commissioning
  // SetAppNVItem(APP_NV_COMMISSIONED_STATUS, 0, &comm_stat);
  
  // This will ensure that the entry in the assoc list of the coordinator is cleaned up
  ZDO_RegisterForZdoCB(ZDO_LEAVE_CNF_CBID, ZDO_NwkLeaveCB);
  ProjectSpecific_SendLeaveReq();
}

/**************************************************************************************************
 * @fn      ProjectSpecific_GetCommissionStatus
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
uint8 ProjectSpecific_GetCommissionStatus(void)
{
  return nv_commissioned_status;
}

/**************************************************************************************************
 * @fn      ProjectSpecific_SendLeaveReq
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void ProjectSpecific_SendLeaveReq(void)
{
  if (get_nwk_status() == NWK_JOINED)
  {
    ProjectSpecific_UartWrite(ZBC_PORT, "LR\n\r", 4); 
    NLME_LeaveReq_t leavingReq;
    leavingReq.extAddr = NULL;  //NULL to remove itself
    leavingReq.removeChildren = true;
    leavingReq.rejoin = true;
    leavingReq.silent = true;
    NLME_LeaveReq(&leavingReq);
  }
}

/**************************************************************************************************
 * @fn      ProjectSpecific_UartWrite
 *
 * @brief   Just a wrapper for writing to serial because while debugging
 *          we can turn off writing to serial just at one place      
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
uint16 ProjectSpecific_UartWrite(uint8 port, uint8 *buf, uint16 len)
{
  return HalUARTWrite(port, buf, len);
  return 0;
}


/*************************************************************************************
 * @fn      ProjectSpecific_HexDump
 *
 * @brief   Converts a memory buffer pointed to by ptr of length len into a string
 *          of hex digits and sends it to output using ProjectSpecific_UartWrite()
 *
 * @param   ptr - pointer to beginning of the memory segment to print
 *          len - number of bytes to convert and print
 *************************************************************************************/

void ProjectSpecific_HexDump(uint8 *ptr, uint16 len)
{
  static const uint8 charMap[] = "0123456789ABCDEF";
  // Length of ptr can be no greater than 256, so 256 x 2 hex per byte = 512 + \r\n = 514
  uint8 buf[256];
  uint8 *bufPtr = buf;
  int i;
  for (i = 0; i < len; ++ptr, ++i) {
    *bufPtr = charMap[((*ptr) >> 4) & 0xF];
    ++bufPtr;
    *bufPtr = charMap[(*ptr) & 0xF];
    ++bufPtr;
  }
  *bufPtr++ = '\r';
  *bufPtr = '\n';
  uint16 bufLen = len * 2 + 2;
  uint16 frame;
  const uint16 frameLen = 32;
  for (frame = 0; frame + frameLen < bufLen; frame = frame + frameLen) {
    ProjectSpecific_UartWrite(ZBC_PORT, buf + frame, frameLen);
  }
  ProjectSpecific_UartWrite(ZBC_PORT, buf + frame, bufLen - frame);
  
}


/**************************************************************************************************
 * @fn      ParseSerialCommand
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
static void ParseSerialCommand( void )
{
   uint8 msgType; 
   int nbytes = 0;   
   uint8 serialCmd[SERIAL_PACKET_HDR_LEN];
  
   uint8 buff[2] = {0};
   uint8 *mptr = NULL;
   uint8 addridx = 0;
   uint16 shortAddr = 0;
   uint16 shortAddr_hi = 0;
   uint16 shortAddr_lo = 0;
   uint8 remaininglen = 0;
   uint16 k = 0;
   uint8 temp = 0;
   uint8 mt_packet_len = 0;
   uint8 *ota_mt = NULL;
   uint8 j = 0;
   
  nbytes = HalUARTRead(ZBC_PORT, serialCmd, SERIAL_PACKET_HDR_LEN);
  
  if (nbytes == 0)
    return;
  
  // Checking Magic Bytes in Header
  if (serialCmd[0] != SERIAL_SYNCBYTE_1 || serialCmd[1] != SERIAL_SYNCBYTE_2)
    return;
  
  //uint8 zbc_id = serialCmd[2];  // ZBC_ID (Not Currently Used?)
  msgType = serialCmd[3];         // Message Type
  uint8 seqNum = serialCmd[4];    // Sequence Number
  uint8 packetlen = serialCmd[5]; // Packet Data Payload Length
  
  switch (msgType)
  {
    case SERIAL_MT_CMD_REQUEST:
      MT_UartProcessZToolData (ZBC_PORT, 0x00);  //0x00 is never referenced in MT_TASK
      break;
     
    /* Take Serial Packet and Passthrough OTA */
    case SERIAL_SENSOR_DATAPACKET:
      nbytes = HalUARTRead( ZBC_PORT, SerialED_RxBuf, packetlen );
      if( nbytes )
      {
        // For E-Meter, last two bytes of payload are checksum, but they are calculated using
        // the seqNum, which isn't sent along.  Do we need to do a checksum check here? --Max
        BaseED_Send( DEVSPECIFIC_CLUSTER, DEV_SPECIFIC_ENERGYMETER_MSG, 0, nbytes, SerialED_RxBuf );
      }
      break;
           
    default:
      break;
  }
}

/**************************************************************************************************
 * @fn      ProjectSpecific_SendMTResp
 *
 * @brief   Relays the response of an MT command via UART in proper packet formatting
 *
 * @param   *mtbuff   - 
 *          mtbufflen - 
 *          respType  - 
 *
 * @return  None
 **************************************************************************************************/
void ProjectSpecific_SendMTResp(uint8 *mtbuff, uint8 mtbufflen, uint8 respType)
{
  static uint8 len = 0;
  static uint8 total_len = 0;
  uint8 *MTRespPacket = NULL;
  uint8 idx = 0;
  static uint16 total_written = 0;

  len = mtbuff[1] + 5;
  total_len = MAGIC_LEN + PREAMBLE_LEN + len + CRC_LEN;
  MTRespPacket = osal_mem_alloc(total_len); 

  if (MTRespPacket != NULL)
  {
    uint16 pan;
    osal_nv_read( ZCD_NV_PANID, 0, sizeof( pan ), &pan ); 
    
    MTRespPacket[idx++] = SERIAL_SYNCBYTE_1;
    MTRespPacket[idx++] = SERIAL_SYNCBYTE_2;            
    MTRespPacket[idx++] = 0;          //This is don't care really              
    MTRespPacket[idx++] = respType;   //Response to an MT command    
    MTRespPacket[idx++] = LO_UINT16( pan );
    MTRespPacket[idx++] = HI_UINT16( pan );
    MTRespPacket[idx++] = 0x00;       //Seq no. which is a don't care for now 
    MTRespPacket[idx++] = len;        //length of the actual MT response
    osal_memcpy(MTRespPacket + idx, mtbuff, len);
    idx += len;
    MTRespPacket[idx++] = 0xDE;
    MTRespPacket[idx++] = 0xAD;
            
    total_written = HalUARTWrite(ZBC_PORT, MTRespPacket, total_len);
    osal_mem_free(MTRespPacket);
  }
  else 
  {
    // Send a hardcoded packet to indicate error: TODO
  }
}

/**************************************************************************************************
 * @fn      ProjectSpecific_ChangeToOTAMode
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void ProjectSpecific_ChangeToOTAMode(void)
{
  
  ProjectSpecific_TurnUpPolling();
  ProjectSpecific_PowerUpRadio(0, 0xFFFF);
  /*
  // Save the polling rates
  appInstance.pollRate = zgPollRate;
  appInstance.queuedPollRate = zgQueuedPollRate;
  appInstance.responsePollRate = zgResponsePollRate;
  
  // Now changes these values
  NLME_SetPollRate(10);
  NLME_SetQueuedPollRate(10);
  NLME_SetResponseRate(10);
  
  // Turn the MAC to always on
  uint8 RxOnIdle = TRUE;
  ZMacSetReq( ZMacRxOnIdle, &RxOnIdle );
  
  */
}

/**************************************************************************************************
 * @fn      ProjectSpecific_RestoreToNormalMode
 *
 * @brief   
 *
 * @param   
 *
 * @return  
 **************************************************************************************************/
void ProjectSpecific_RestoreToNormalMode(void)
{
  ProjectSpecific_TurnDownPolling();
  ProjectSpecific_PowerDownRadio(0);
  /*
  // Restore the values
  NLME_SetPollRate(appInstance.pollRate);
  NLME_SetQueuedPollRate(appInstance.queuedPollRate);
  NLME_SetResponseRate(appInstance.responsePollRate);
  
  // Change the variables too
  zgPollRate = appInstance.pollRate;
  zgQueuedPollRate = appInstance.queuedPollRate;
  zgResponsePollRate = appInstance.responsePollRate;
  
  
  // Change the MAC to NOT always on
  uint8 RxOnIdle = FALSE;
  ZMacSetReq( ZMacRxOnIdle, &RxOnIdle );
  */
}
