/*******************************************************************************
  Filename:       BaseComms.c
  Revised:        $Date: 2009-03-29 10:51:47 -0700 (Sun, 29 Mar 2009) $
  Revision:       $Revision: 19585 $

  Description -   Base code for basic communication messages
*******************************************************************************/

#include "AF.h"
#include "OnBoard.h"
#include "OSAL_Tasks.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"
#include <string.h>
#include "hal_drivers.h"
#include "hal_key.h"
#include "hal_led.h"
#include "hal_uart.h"

#include "SynDefines.h"
#include "BaseED.h"
#include "BaseComms.h"


/*********************************************************************
 * LOCAL FUNCTIONS - Modify these for the particular sensor
 */

/*********************************************************************
 * LOCAL VARIABLES
 */


/*********************************************************************
 * EXTERNAL VARIABLES - these are defined within the skeleton implementation.
 */

extern afAddrType_t Coord_Addr;
extern uint8 BaseEDEM_MsgID;
extern const endPointDesc_t BaseEDEM_epDesc;


/*********************************************************************
 * PUBLIC FUNCTIONS
 */



/*********************************************************************
 * @fn      BaseComms_ProcessCoreMsg
 *
 * @brief  This processes all core communication messages 
 *
 * @param  afIncomingMSGPacket_t *pkt - the packet that contains the msg
 */
void BaseComms_ProcessCoreMsg( afIncomingMSGPacket_t *pkt )
{  
    // Handle all OTA_MT packets here
     if(pkt->cmd.Data[0] == SYNCBYTE_1 && pkt->cmd.Data[1] == SYNCBYTE_2)
    {
        //OTA SYNCBYTES (0xA%, 0xCC) match.
        
       
    }
}

