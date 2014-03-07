#ifndef ProjectSpecific_H
#define ProjectSpecific_H

#include "SynDefines.h"

// This is the header file for ProjectSpecific.c.

/*********************************************************************
 * MACROS
 */

//#define MAGIC_LEN  0x02   // Length of the magic bytes - 2 bytes at the moment
//#define PREAMBLE_LEN 0x4  // Preamble length excluding the magic bytes
//#define CRC_LEN  0x02     // Length of the last 2 bytes of CRC at the end

#define SERIAL_APP_TX_MAX   80

#define OTA_DL_NOTINPROGRESS   0   //means that end device is NOT in the middle of an OTA upgrade
#define OTA_DL_INPROGRESS      1   //means that end device is in the middle of an OTA upgrade

#define DELAY_OFF   0
#define DELAY_ON    1
#define CYCLE_OFF   0
#define CYCLE_ON    1  

/* This PRESENCE term needs to be less specific, 
   these are not particular to Presence-Occupancy */
/*             Refactor Below                     */
#define PRESENCE_RADIO_ON_EVT                0x0002

#define PRESENCE_SEND_INTER_PAN_INIT_EVT     0x4000
#define PRESENCE_TIMER_DEV_ANNOUNCE_EVT      0x2000
#define PRESENCE_TIMER_OTA_TIMEOUT_EVT       0x1000
#define PRESENCE_RESET_EVT                   0x0800
#define PRESENCE_SEND_COORD_INIT_PACKET_EVT  0x0400
#define PRESENCE_GATHER_NW_PARMS_EVT         0x0200
#define PRESENCE_CHECK_NETWORK_STATUS_EVT    0x0100

#define PRESENCE_SCAN_NETWORKS_EVT           0x0080
#define PRESENCE_DATARATE_CALC_EVT           0x0040


#define PRESENCE_RESET_TIMER                   1000
#define PRESENCE_SEND_COORD_INIT_PACKET_TIMER  500
#define PRESENCE_SEND_INTER_PAN_INIT_TIMER     500
#define PRESENCE_GATHER_NW_PARMS_TIMER         500
#define PRESENCE_SCAN_NETWORKS_TIMER           2000
#define PRESENCE_JOIN_A_NETWORK_TIMER          5000
#define APPLY_JOIN_POLICY_TIMER                10000 //10 seconds
#define OTA_GET_NEXT_PACKET_TIMEOUT_BASE       0x100

#define MAX_STARTUP_SLEEP_COUNT                 64


// Multiply CHECK_NETWORK_STATUS_TIMER times (PLANNED|NORMAL)_RESET_DELAY
// to get number of milliseconds to wait before initiating a search for a new PAN
#define PRESENCE_CHECK_NETWORK_STATUS_TIMER     15000
#define PRESENCE_PLANNED_RESET_DELAY            20        // 20 * 15000 = 5 minutes
#define PRESENCE_NORMAL_RESET_DELAY             4         // 4 * 15000 = 1 minute

     
/*              Refactor Above                    */

// Time between each heartbeat, in minutes
#define PRESENCE_DATARATE_SAMPLE_PERIOD         1
// Since we have to set timer in milliseconds, we need a second count value  
// 1 * 5000ms = 5 seconds
#define PRESENCE_DATARATE_SAMPLE_TIMER          5000 
#define PRESENCE_NWK_REJOIN_TRY_COUNT           3

/* Standard Messages used in ??? */
#define MSG0  "Occ sensor powering up\n\r"
#define MSG0_LEN osal_strlen(MSG0)

#define MSG1  "Check NW Status\n\r"
#define MSG1_LEN osal_strlen(MSG1)

#define MSG2  "Waking up to attempt rejoin\n\r"
#define MSG2_LEN osal_strlen(MSG2)

#define MSG3  "Already joined, nothing to do\n\r"
#define MSG3_LEN osal_strlen(MSG3)

#define MSG4  "ZDApp_StopJoiningCycle() failed\n\r"
#define MSG4_LEN osal_strlen(MSG4)

#define MSG5  "Sent sensor data for occupancy node.\n\r"
#define MSG5_LEN osal_strlen(MSG5)

#define MAX_PANS_SCANNED  10


/* Legacy, Not Generic, Needs to be weeded out */
#ifndef ANALED

#define LED_PIN1 P2_4     //D2 - Blue LED

#define ANALED1_OFF()      (0) //st( LED_PIN1 = 0; );  
#define ANALED1_ON()       (0) //st( LED_PIN1 = 1; );  
#define ANALED1_TOGGLE()   (0) //st( if (LED_PIN1) { LED_PIN1 = 0; } else { LED_PIN1 = 1;} );

#define LED_PIN2 P2_3     //D1 - Red LED

#define ANALED2_OFF()      (0) //st( LED_PIN2 = 0; );  
#define ANALED2_ON()       (0) //st( LED_PIN2 = 1; );  
#define ANALED2_TOGGLE()   (0) //st( if (LED_PIN2) { LED_PIN2 = 0; } else { LED_PIN2 = 1;} );
#else

#define ANALED1_OFF()      
#define ANALED1_ON()       
#define ANALED1_TOGGLE()   

#define ANALED2_OFF()      
#define ANALED2_ON()       
#define ANALED2_TOGGLE()   

#endif  //for leds

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */
extern uint8 get_nwk_status(void);

/*********************************************************************
 * TYPEDEFS
 */

// This structure will contain instance information for the application that
// needs to be accessed from across files and states.
typedef struct
{
  OtaStatus_t otaStatus;
  uint8 appTaskid;
  uint8 imgarea;
  uint8 imgtype;
  uint16 totalpackets;
  uint8 extaddr[Z_EXTADDR_LEN];
  uint16 pollRate;
  uint16 queuedPollRate;
  uint16 responsePollRate;
  uint16 panid;
} appInstance_t;

// Enum for device states
typedef enum
{
  // Device has no commissioning parameters
  NON_COMMISSIONED,     
  // Device has only device specific commissioning parameters
  DEVICE_COMMISSIONED,  
  // Device is in the process of getting network commissioning parameters
  NETWORK_COMMISSIONING_IN_PROGRESS, 
  // Device is done with gathering all network commissioning parameters
  NETWORK_COMMISSIONING_COMPLETED,   
  NETWORK_COMMISSIONED,
  // Network commissioning policy is applied and device is fully commissioned, ready to fly.
  DEVICE_ACTIVE        
} deviceState_t;

typedef enum
{
  COORD_RESET_NORMAL = 0,
  COORD_RESET_PLANNED = 1,
  COORD_RESET_IMMEDIATE = 2
} coordReset_t;

//Structure storing details of networks found after scanning
typedef struct NWInfo
{
  uint16 panID;
  uint16 address;
  uint32 channel;
  int16 rssi;
  uint16 lqi;
  uint16 nassoc;
  uint16 drate;
} NWInfo_t;

// Structure for storing an NV item
typedef struct appNVItemTab
{
  uint16 id;
  uint16 len;
  void *buf;
} appNVItemTab_t;

// Struct for ... *?*
typedef struct appNVItemDefaultValues
{
  uint16 id;
  uint16 len;
  const void *defvalue;
} appNVItemDefaultValues_t;

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

void ProjSpecific_InitDevice( uint8 task_id );
UINT16 ProjectSpecific_ProcessEvent( UINT16 events );
void ProjectSpecific_ProcessSystemEvent(  afIncomingMSGPacket_t *MSGpkt );
void ProjSpecific_ZDO_state_change( void );
void ProjectSpecific_ProcessDevSpecificMsg( afIncomingMSGPacket_t *pkt, uint16 shortAddr );
void ProjectSpecific_ProcessCoreMsg( afIncomingMSGPacket_t *pkt, uint16 shortAddr );
uint8 SetAppNVItem(uint16 id, uint16 offset, void *buf);

#endif