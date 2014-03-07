/******************************************************************************
  Filename:       BaseED.c
  Revised:        $Date: 2010-12-21 10:27:34 -0800 (Tue, 21 Dec 2010) $
  Revision:       $Revision: 24670 $

  Description:    BaseED.c (no Profile).

******************************************************************************/

/**************************************************************************************************
 *                                            INCLUDES
 **************************************************************************************************/
#include "OSAL.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"

#include "BaseED.h"
#include "BaseED_support.h"
#include "BaseED_supportsettings.h"
#include "SynDefines.h"
#include "BaseComms.h"

#include "DebugTrace.h"

#if !defined( WIN32 )
  #include "OnBoard.h"
#endif

#ifdef INTER_PAN
  #include "stub_aps.h"
#endif

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_uart.h"


extern uint16 numbytes;
extern uint16 totalLQI;
extern int16 totalRSSI;

/**************************************************************************************************
 * LOCAL VARIABLES
 **************************************************************************************************/

static uint8 BaseED_TaskID;    // Task ID for internal task/event processing.
static uint8 transID;
static afAddrType_t Coord_Addr;
static uint32 TxSeqNum = 0;
//static uint8 TxSeqNum;
static uint8 nwk_status = NWK_ORPHAN;

uint8  nwkJoinAttemptCount = 0;
/**************************************************************************************************
 * CONSTANTS
 **************************************************************************************************/

// This list should be filled with Application specific Cluster IDs.
const cId_t BaseED_ClusterList[MAX_CLUSTERS] =
{
  CORECOMMS_CLUSTER,
  DEVSPECIFIC_CLUSTER
};

// the description for this app
const SimpleDescriptionFormat_t BaseED_SimpleDesc =
{
  BaseED_ENDPOINT,              //  int   Endpoint;
  BaseED_PROFID,                //  uint16 AppProfId[2];
  BaseED_DEVICEID,              //  uint16 AppDeviceId[2];
  BaseED_DEVICE_VERSION,        //  int   AppDevVer:4;
  BaseED_FLAGS,                 //  int   AppFlags:4;
  MAX_CLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)BaseED_ClusterList,  //  byte *pAppInClusterList;
  MAX_CLUSTERS,          //  byte  AppNumOutClusters;
  (cId_t *)BaseED_ClusterList   //  byte *pAppOutClusterList;
};

// the end point description for this app
const endPointDesc_t BaseED_epDesc =
{
  BaseED_ENDPOINT,
  &BaseED_TaskID,
  (SimpleDescriptionFormat_t *)&BaseED_SimpleDesc,
  noLatencyReqs
};

/**************************************************************************************************
 * TYPEDEFS
 **************************************************************************************************/


/**************************************************************************************************
 * EXTERNAL VARIABLES
 **************************************************************************************************/
extern uint8 nv_commissioned_status;
extern DeviceInfo_t nv_device_info;

/**************************************************************************************************
 * EXTERNAL FUNCTIONS
 **************************************************************************************************/
uint8 get_nwk_status(void);
#ifdef DEBUG
uint16 ProjectSpecific_UartWrite(uint8 port, uint8 *buf, uint16 len);
void ProjectSpecific_HexDump(uint8 *ptr, uint16 len);
#endif //DEBUG

/*********************************************************************
 * LOCAL FUNCTIONS
 **************************************************************************************************/
static void BaseED_ProcessMSGCmd( afIncomingMSGPacket_t *pkt );


/**************************************************************************************************
 * PUBLIC FUNCTIONS
 **************************************************************************************************/

/**************************************************************************************************
 * @fn      BaseED_Init
 *
 * @brief   This is called during OSAL tasks' initialization.
 *
 * @param   task_id - the Task ID assigned by OSAL.
 *
 * @return  none
 **************************************************************************************************/
void BaseED_Init( uint8 task_id )
{
  Coord_Addr.addrMode = (afAddrMode_t)Addr16Bit;
  Coord_Addr.addr.shortAddr = 0;
  Coord_Addr.endPoint = BaseED_ENDPOINT;
  BaseED_TaskID = task_id;
  afRegister( (endPointDesc_t *)&BaseED_epDesc );
  RegisterForKeys( task_id );
  ProjSpecific_InitDevice( task_id );  //This will initialize the sensor device
  
  
  HalLedBlink (HAL_LED_2, 1, HAL_LED_DEFAULT_DUTY_CYCLE, HAL_LED_DEFAULT_FLASH_TIME);
  // Max asks: why only if DEVICE_ACTIVE?
  
  nwkJoinAttemptCount = 0;
}

/**************************************************************************************************
 * @fn      BaseED_ProcessEvent
 *
 * @brief   Generic Application Task event processor.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events   - Bit map of events to process.
 *
 * @return  Event flags of all unprocessed events.
 **************************************************************************************************/
UINT16 BaseED_ProcessEvent( uint8 task_id, UINT16 events )
{
  (void)task_id;  // Intentionally unreferenced parameter
  //afDataConfirm_t *afDataConfirm;
  //ZStatus_t sentStatus;
  devStates_t BaseED_NwkState;
   
  if ( events & SYS_EVENT_MSG )
  {
    afIncomingMSGPacket_t *MSGpkt;
    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( BaseED_TaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
      case ZDO_STATE_CHANGE:
        BaseED_NwkState = (devStates_t)(MSGpkt->hdr.status);   
        if (BaseED_NwkState == DEV_END_DEVICE)
        { 
            HalLedBlink ( HAL_LED_2, 2, 50, 1000 );
            ProjSpecific_ZDO_state_change( );
            nwk_status = NWK_JOINED;
        }       
        
        break;         
        
      case AF_DATA_CONFIRM_CMD:
         //afDataConfirm = (afDataConfirm_t *)MSGpkt;
         //sentStatus = afDataConfirm->hdr.status;
         //BaseED_HandleMsgConfirmation(sentStatus);
         //ANALED2_TOGGLE();
         if(nv_commissioned_status == DEVICE_ACTIVE)
         {
           // blink only in case of active device
           ANALED1_TOGGLE();
           //ANALED2_TOGGLE();
         }
         break;
         
      case AF_INCOMING_MSG_CMD:
        BaseED_ProcessMSGCmd( MSGpkt );
        //ANALED2_TOGGLE();
        break;

      default:
        break;
      }
      
      // call project specific event handler
      ProjectSpecific_ProcessSystemEvent(MSGpkt);
      
      osal_msg_deallocate( (uint8 *)MSGpkt );
    }
    return ( events ^ SYS_EVENT_MSG );
  }
  
  // any project specific events? 
  return ProjectSpecific_ProcessEvent(events); 
}

/**************************************************************************************************
 * @fn      BaseED_Send
 *
 * @brief   Send data OTA.
 *
 * @param   message type, message flag, payload size, and payload
 *
 * @return  none
 **************************************************************************************************/
void BaseED_Send(unsigned char clusterID, unsigned char msgTypeID, unsigned char msgFlag,
                 unsigned char bufSize, unsigned char *buffer)
{
  int total_len = 0;
  afStatus_t  nret = afStatus_FAILED;
  uint8 *pTxMsg = NULL;
  
#ifdef DEBUG
  ProjectSpecific_UartWrite(ZBC_PORT, "Sending message: ", 17);
  ProjectSpecific_HexDump(buffer, bufSize);
#endif //DEBUG
  
  #define PREAMBLE_LENGTH 14  // 14 bytes include sync, preamble
  #define CRC_LENGTH 2 
  
  total_len = PREAMBLE_LENGTH + bufSize + CRC_LENGTH;  // calculate total length of packet
  pTxMsg = osal_mem_alloc(total_len); // allocate buffer space for this
  
  if (pTxMsg) {
    // Populate the sync bytes
    pTxMsg[0] = SYNCBYTE_1;
    pTxMsg[1] = SYNCBYTE_2;
      
    // Populate the preamble bytes  
    pTxMsg[2] = nv_device_info.deviceType;  // device type
    pTxMsg[3] = msgTypeID;  // message type 
    
    uint16 saddr = NLME_GetShortAddr(); // get the network address  
    pTxMsg[4] = HI_UINT16(saddr);  // the network address 
    pTxMsg[5] = LO_UINT16(saddr);
    
    pTxMsg[6] = HI_UINT16(nv_device_info.deviceId);  // the device ID
    pTxMsg[7] = LO_UINT16(nv_device_info.deviceId);
  //  pTxMsg[6] = HI_UINT16(DEVICE_ID);  // the device ID
  //  pTxMsg[7] = LO_UINT16(DEVICE_ID);
  
    pTxMsg[8] = BREAK_UINT32(TxSeqNum, 0);  //low
    pTxMsg[9] = BREAK_UINT32(TxSeqNum, 1);
    pTxMsg[10] = BREAK_UINT32(TxSeqNum, 2);
    pTxMsg[11] = BREAK_UINT32(TxSeqNum, 3); //high
    
    
    pTxMsg[12] = msgFlag;
    pTxMsg[13] = bufSize;  
  
    if (bufSize > 0)  // copy the incomming data payload to the tx buffer
    {
      osal_memcpy(pTxMsg + PREAMBLE_LENGTH, buffer, bufSize); 
    }
    
    // now put in the CRC bytes
    uint16 cksum = CalcCkSum(pTxMsg, total_len-2);
    pTxMsg[PREAMBLE_LENGTH + bufSize] = HI_UINT16(cksum);
    pTxMsg[PREAMBLE_LENGTH + bufSize + 1] = LO_UINT16(cksum);
                   
    // send out the message!
    nret = AF_DataRequest(&Coord_Addr,
                          (endPointDesc_t *)&BaseED_epDesc,
                           clusterID,
                           total_len, pTxMsg,
                           &transID, AF_SKIP_ROUTING, AF_DEFAULT_RADIUS);
  
    if (nret != afStatus_SUCCESS)   
    {
      asm("nop");
      //osal_set_event(BaseED_TaskID, BaseEDEM_SEND_EVT);      
      #ifdef DEBUG
      ProjectSpecific_UartWrite(ZBC_PORT, "Failed! ", 8);
      ProjectSpecific_HexDump(&nret, 1);
      #endif //DEBUG
    }
    else 
    {
      if (msgTypeID != END_DEVICE_MESSAGE_TYPE_OTA_MT_RESP)
      {
        // We increment seq number only for non MT responses, else all MT responses
        // will come up as a packet lost because the seq number will be bumped by 2 
        // when the next data packet is transmitted.      
        TxSeqNum++;
        // we increment the total number of bytes sent
        numbytes += total_len;
      }
    }
  
    // Free the memory now
    osal_mem_free(pTxMsg);
  }
  else {
    asm("nop");
    #if DEBUG > 1
    ProjectSpecific_UartWrite(ZBC_PORT, "Can't send: NO MEM\r\n", 20);
    #endif
  }
}
#ifdef INTER_PAN
/**************************************************************************************************
 * @fn      BaseED_SendInterPan
 *
 * @brief   Send data OTA to a coordinator on a different PAN.
 *
 * @param   TODO
 *
 * @return  none
 **************************************************************************************************/
void BaseED_SendInterPan(unsigned char clusterID, unsigned char msgTypeID, unsigned char msgFlag,
                 uint16 dstPanId, uint16 dstAddr, unsigned char bufSize, unsigned char *buffer)
{
  int total_len = 0;
  afStatus_t  nret = afStatus_FAILED;
  uint8 *pTxMsg = NULL;
  
#ifdef DEBUG
  ProjectSpecific_UartWrite(ZBC_PORT, "Sending interpan: ", 18);
  ProjectSpecific_HexDump(buffer, bufSize);
#endif //DEBUG
  
  #define PREAMBLE_LENGTH 14  // 14 bytes include sync, preamble
  #define CRC_LENGTH 2 
  
  total_len = PREAMBLE_LENGTH + bufSize + CRC_LENGTH;  // calculate total length of packet
  pTxMsg = osal_mem_alloc(total_len); // allocate buffer space for this
  
  // Populate the sync bytes
  pTxMsg[0] = SYNCBYTE_1;
  pTxMsg[1] = SYNCBYTE_2;
    
  // Populate the preamble bytes  
  pTxMsg[2] = nv_device_info.deviceType;  // device type
  pTxMsg[3] = msgTypeID;  // message type 
  
  uint16 saddr = NLME_GetShortAddr(); // get the network address  
  pTxMsg[4] = HI_UINT16(saddr);  // the network address 
  pTxMsg[5] = LO_UINT16(saddr);
  
  pTxMsg[6] = HI_UINT16(nv_device_info.deviceId);  // the device ID
  pTxMsg[7] = LO_UINT16(nv_device_info.deviceId);
//  pTxMsg[6] = HI_UINT16(DEVICE_ID);  // the device ID
//  pTxMsg[7] = LO_UINT16(DEVICE_ID);

  pTxMsg[8] = BREAK_UINT32(TxSeqNum, 0);  //low
  pTxMsg[9] = BREAK_UINT32(TxSeqNum, 1);
  pTxMsg[10] = BREAK_UINT32(TxSeqNum, 2);
  pTxMsg[11] = BREAK_UINT32(TxSeqNum, 3); //high

  pTxMsg[12] = msgFlag;
  pTxMsg[13] = bufSize;  

  if (bufSize > 0)  // copy the incomming data payload to the tx buffer
  {
    osal_memcpy(pTxMsg + PREAMBLE_LENGTH, buffer, bufSize); 
  }
  
  // now put in the CRC bytes
  uint16 cksum = CalcCkSum(pTxMsg, total_len-2);
  pTxMsg[PREAMBLE_LENGTH + bufSize] = HI_UINT16(cksum);
  pTxMsg[PREAMBLE_LENGTH + bufSize + 1] = LO_UINT16(cksum);
  
  afAddrType_t Interpan_Addr;
  Interpan_Addr.addrMode = (afAddrMode_t)Addr16Bit;
  Interpan_Addr.addr.shortAddr = dstAddr;
  Interpan_Addr.endPoint = STUBAPS_INTER_PAN_EP;
  Interpan_Addr.panId = dstPanId;
                 
  // send out the message!
  nret = AF_DataRequest(&Interpan_Addr,
                        (endPointDesc_t *)&BaseED_epDesc,
                         clusterID,
                         total_len, pTxMsg,
                         &transID, AF_SKIP_ROUTING | AF_EN_SECURITY, AF_DEFAULT_RADIUS);

  if (nret != afStatus_SUCCESS)   
  {
    //osal_set_event(BaseED_TaskID, BaseEDEM_SEND_EVT);      
    #ifdef DEBUG
    ProjectSpecific_UartWrite(ZBC_PORT, "Failed! ", 8);
    ProjectSpecific_HexDump(&nret, 1);
    #endif //DEBUG
  }
  else 
  {
    if (msgTypeID != END_DEVICE_MESSAGE_TYPE_OTA_MT_RESP)
    {
      // We increment seq number only for non MT responses, else all MT responses
      // will come up as a packet lost because the seq number will be bumped by 2 
      // when the next data packet is transmitted.      
      TxSeqNum++;
      // we increment the total number of bytes sent
      numbytes += total_len;
    }
  }
  
  // Free the memory now
  osal_mem_free(pTxMsg);
}
#endif//INTER_PAN
/**************************************************************************************************
 * @fn      BaseED_ProcessMSGCmd
 *
 * @brief   Data message processor callback. This function processes
 *          any incoming data from the coordinator.
 *
 * @param   pkt - pointer to the incoming message packet
 *
 * @return  TRUE if the 'pkt' parameter is being used and will be freed later,
 *          FALSE otherwise.
 **************************************************************************************************/
void BaseED_ProcessMSGCmd( afIncomingMSGPacket_t *pkt )
{
  uint16 recvd_short_addr = (unsigned short)(pkt->cmd.Data[4]<<8) + pkt->cmd.Data[5];
  if (recvd_short_addr == NLME_GetShortAddr() || recvd_short_addr == 0xFFFF)
  {
    totalLQI += pkt->LinkQuality;
    totalRSSI += pkt->rssi;
    switch ( pkt->clusterId )
    {
     
      // CORECOMMS_CLUSTER is for core messages.
      case CORECOMMS_CLUSTER:     
        //BaseComms_ProcessCoreMsg(pkt);
        //numbytes += pkt->cmd.DataLength;
        ProjectSpecific_ProcessCoreMsg(pkt, recvd_short_addr);
        break;
  
      // DEVSPECIFIC_CLUSTER is for project device specific messages.
      case DEVSPECIFIC_CLUSTER:
        //numbytes += pkt->cmd.DataLength;
        ProjectSpecific_ProcessDevSpecificMsg(pkt, recvd_short_addr);
        break;              
      default:
          break;
    }
  }
}

/**************************************************************************************************
 * @fn      CalcCkSum
 *
 * @brief   This calculates the checksum of the passed in buffer
 *
 * @param   *dataBuffer -
 *          len - 
 *          
 * @return  cksum
 **************************************************************************************************/
uint16 CalcCkSum(uint8 *dataBuffer, uint8 len)
{
    uint16 cksum = 0;
    uint8 CK_A = 0;
    uint8 CK_B = 0;
    uint8 i = 0;
    for (i = 0; i < len; i++)
    {
        CK_A = CK_A + dataBuffer[i];
        CK_B = CK_B + CK_A;
    }
    
    cksum = (CK_A * 256) + CK_B;
    return cksum;  
}

/**************************************************************************************************
 * @fn      get_nwk_status
 *
 * @brief   This returns the current status of the network.  If associated with coordinator, 
 *          returns NWK_JOINED.  returns NWK_ORPHAN otherwise.
 *
 * @param   None
 *          
 * @return  nwk_status
 **************************************************************************************************/
uint8 get_nwk_status(void)
{
  return nwk_status;
}
