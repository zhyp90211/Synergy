/**************************************************************************************************
  Filename:       GenericApp.h
  Revised:        $Date: 2007-10-27 17:22:23 -0700 (Sat, 27 Oct 2007) $
  Revision:       $Revision: 15795 $

  Description:    This file is the header file for the BaseED codebase


  Copyright 2004-2007 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED “AS IS” WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE, 
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com. 
**************************************************************************************************/
#ifndef BASEED_H
#define BASEED_H

/*********************************************************************
 * INCLUDES
 */
#include "ZComDef.h"

/*********************************************************************
 * CONSTANTS
 */

#define NWK_JOINED 1
#define NWK_ORPHAN 2

#define BaseED_ENDPOINT           11

#define BaseED_PROFID             0x0F05
#define BaseED_DEVICEID           0x9111
#define BaseED_DEVICE_VERSION       1
#define BaseED_DEVICE_VERSION_MINOR 1
#define BaseED_FLAGS                0
  
#define BaseED_SEND_EVT            0x0001
#define BaseED_RESP_EVT            0x0002
#define BaseED_NWK_JOIN_STATUS_EVT 0x0004     // Event which will prompt us to check network status
#define BaseED_NWK_JOIN_RETRY_EVT  0x0008     // Event which will prompt us to retry joining the network in the hope that this time we will be able to join with it

#define BaseED_RF_SHUTDOWN_EVT     0x0010
// If this is a testbed device, it will be connected to USB, so we don't mind
// clobbering the RF_SHUTDOWN_EVT
#ifdef TESTBED_ANAREN_DEVICE
  #define BaseED_TESTBED_PERIODIC_PACKET_EVT 0x0010
  #define BaseED_TESTBED_PERIODIC_PACKET_TIMER 250
  #define RFD_RCVC_ALWAYS_ON TRUE
#endif

#define BaseED_TOGGLE_LED_EVT      0x0020

// Timer to check the network status, 15 seconds of scanning
#define BaseED_NWK_JOIN_STATUS_TIMEOUT   15000

// Timer to retry network joining
// 15000ms * 19 = 285 seconds of sleep
// 15000ms * 3 = 45 seconds of sleep
#define BaseED_NWK_JOIN_RETRY_REPEAT_COUNT 7//19
#define BaseED_NWK_JOIN_RETRY_TIMEOUT      15000

// Timer to determine how long to search for Comissioning Coordinator
#define BaseED_RF_SHUTDOWN_TIMEOUT    15000
#define BaseED_RF_SHUTDOWN_REPEAT_COUNT 8

#define BaseED_TOGGLE_LED_TIMER  800


#define BIT0              0x01
#define BIT1              0x02
#define BIT2              0x04
#define BIT3              0x08
#define BIT4              0x10
#define BIT5              0x20
#define BIT6              0x40
#define BIT7              0x80

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * Public FUNCTIONS
 */

// Task Initialization 
void BaseED_Init( byte task_id );

// Task Event Processor 
UINT16 BaseED_ProcessEvent( byte task_id, UINT16 events );

// Send out message
void BaseED_Send(unsigned char clusterID, unsigned char msgTypeID, unsigned char msgFlag, 
                 unsigned char bufSize, unsigned char *buffer);

uint16 CalcCkSum(uint8* dataBuffer, uint8 len);

// Send out message to coordinator on different PAN
#ifdef INTER_PAN
void BaseED_SendInterPan(unsigned char clusterID, unsigned char msgTypeID, unsigned char msgFlag,
                 uint16 dstPanId, uint16 dstAddr, unsigned char bufSize, unsigned char *buffer);
#endif //INTER_PAN
uint8 get_nwk_status(void);

/*********************************************************************
*********************************************************************/

#endif 
