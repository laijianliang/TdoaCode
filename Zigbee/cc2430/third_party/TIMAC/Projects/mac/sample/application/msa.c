/**************************************************************************************************
  Filename:       msa.c
  Revised:        $Date: 2010/10/30 23:40:22 $
  Revision:       $Revision: 2.0 $

  Description:    This file contains the sample application that can be use to test
                  the functionality of the MAC, HAL and low level.


  Copyright 2006-2009 Texas Instruments Incorporated. All rights reserved.

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
  PROVIDED �AS IS� WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
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

/**************************************************************************************************

    Description:

                                KEY UP (or S1)
                              - Non Beacon
                              - First board in the network will be setup as the coordinator
                              - Any board after the first one will be setup as the device
                                 |
          KEY LEFT               |      KEY RIGHT(or S2)
                             ----+----- - Start transmitting
                                 |
                                 |
                                KEY DOWN



**************************************************************************************************/


/**************************************************************************************************
 *                                           Includes
 **************************************************************************************************/

/* Hal Driver includes */
#include "hal_types.h"
#include "hal_key.h"
#include "hal_timer.h"
#include "hal_drivers.h"
#include "hal_led.h"
#include "hal_adc.h"
#include "hal_lcd.h"

/* OS includes */
#include "OSAL.h"
#include "OSAL_Tasks.h"
#include "OSAL_PwrMgr.h"

/* Application Includes */
#include "OnBoard.h"

/* MAC Application Interface */
#include "mac_api.h"
#include "mac_main.h"

/* Application */
#include "msa.h"

/**************************************************************************************************
 *                                           Constant
 **************************************************************************************************/

#define MSA_DEV_SHORT_ADDR        0x0000        /* Device initial short address - This will change after association */

#define MSA_HEADER_LENGTH         4             /* Header includes DataLength + DeviceShortAddr + Sequence */
#define MSA_ECHO_LENGTH           8             /* Echo packet */

#define MSA_MAC_MAX_RESULTS       5             /* Maximun number of scan result that will be accepted */
#define MSA_MAX_DEVICE_NUM        7             /* Maximun number of devices can associate with the coordinator */

#if defined (HAL_BOARD_CC2420DB)
  #define MSA_HAL_ADC_CHANNEL     HAL_ADC_CHANNEL_0             /* AVR - Channel 0 and Resolution 10 */
  #define MSA_HAL_ADC_RESOLUTION  HAL_ADC_RESOLUTION_10
#elif defined (HAL_BOARD_DZ1611) || defined (HAL_BOARD_DZ1612) || defined (HAL_BOARD_DRFG4618) || defined (HAL_BOARD_F2618)
  #define MSA_HAL_ADC_CHANNEL     HAL_ADC_CHANNEL_0             /* DZ1611 and DZ1612 - Channel 0 and Resolution 12 */
  #define MSA_HAL_ADC_RESOLUTION  HAL_ADC_RESOLUTION_12
#else
  #define MSA_HAL_ADC_CHANNEL     HAL_ADC_CHANNEL_7             /* CC2430 EB & DB - Channel 7 and Resolution 14 */
  #define MSA_HAL_ADC_RESOLUTION  HAL_ADC_RESOLUTION_14
#endif

/* Size table for MAC structures */
const CODE uint8 msa_cbackSizeTable [] =
{
  0,                                   /* unused */
  sizeof(macMlmeAssociateInd_t),       /* MAC_MLME_ASSOCIATE_IND */
  sizeof(macMlmeAssociateCnf_t),       /* MAC_MLME_ASSOCIATE_CNF */
  sizeof(macMlmeDisassociateInd_t),    /* MAC_MLME_DISASSOCIATE_IND */
  sizeof(macMlmeDisassociateCnf_t),    /* MAC_MLME_DISASSOCIATE_CNF */
  sizeof(macMlmeBeaconNotifyInd_t),    /* MAC_MLME_BEACON_NOTIFY_IND */
  sizeof(macMlmeOrphanInd_t),          /* MAC_MLME_ORPHAN_IND */
  sizeof(macMlmeScanCnf_t),            /* MAC_MLME_SCAN_CNF */
  sizeof(macMlmeStartCnf_t),           /* MAC_MLME_START_CNF */
  sizeof(macMlmeSyncLossInd_t),        /* MAC_MLME_SYNC_LOSS_IND */
  sizeof(macMlmePollCnf_t),            /* MAC_MLME_POLL_CNF */
  sizeof(macMlmeCommStatusInd_t),      /* MAC_MLME_COMM_STATUS_IND */
  sizeof(macMcpsDataCnf_t),            /* MAC_MCPS_DATA_CNF */
  sizeof(macMcpsDataInd_t),            /* MAC_MCPS_DATA_IND */
  sizeof(macMcpsPurgeCnf_t),           /* MAC_MCPS_PURGE_CNF */
  sizeof(macEventHdr_t)                /* MAC_PWR_ON_CNF */
};

/**************************************************************************************************
 *                                        Local Variables
 **************************************************************************************************/

/*
  Extended address of the device, for coordinator, it will be msa_ExtAddr1
  For any device, it will be msa_ExtAddr2 with the last 2 bytes from a ADC read
*/
sAddrExt_t    msa_ExtAddr;
sAddrExt_t    msa_ExtAddr1 = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
sAddrExt_t    msa_ExtAddr2 = {0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x00, 0x00};

/* Coordinator and Device information */
uint16        msa_PanId = MSA_PAN_ID;
uint16        msa_CoordShortAddr = MSA_COORD_SHORT_ADDR;
uint16        msa_DevShortAddr   = MSA_DEV_SHORT_ADDR;

/*
  List of short addresses that the coordinator will use
  to assign to each the device that associates to it
*/
uint16        msa_DevShortAddrList[] = {0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007};

/* Current number of devices associated to the coordinator */
uint8         msa_NumOfDevices = 0;

/* Predefined packet that will be sent out by the coordinator */
uint8         msa_Data1[MSA_PACKET_LENGTH];

/* Predefined packet that will be sent back to the coordinator by the device */
uint8         msa_Data2[MSA_ECHO_LENGTH];

/* TRUE and FALSE value */
bool          msa_MACTrue = TRUE;
bool          msa_MACFalse = FALSE;

/* Beacon payload, this is used to determine if the device is not zigbee device */
uint8         msa_BeaconPayload[] = {0x22, 0x33, 0x44};
uint8         msa_BeaconPayloadLen = 3;

/* Contains pan descriptor results from scan */
macPanDesc_t  msa_PanDesc[MSA_MAC_MAX_RESULTS];

/* flags used in the application */
bool          msa_IsCoordinator  = FALSE;   /* True if the device is started as a Pan Coordinate */
bool          msa_IsStarted      = FALSE;   /* True if the device started, either as Pan Coordinator or device */
bool          msa_IsSampleBeacon = FALSE;   /* True if the beacon payload match with the predefined */
bool          msa_IsDirectMsg    = FALSE;   /* True if the messages will be sent as direct messages */
uint8         msa_State = MSA_IDLE_STATE;   /* Either IDLE state or SEND state */

/* Structure that used for association request */
macMlmeAssociateReq_t msa_AssociateReq;

/* Structure that used for association response */
macMlmeAssociateRsp_t msa_AssociateRsp;

/* Structure that contains information about the device that associates with the coordinator */
typedef struct
{
  uint16 devShortAddr;
  uint8  isDirectMsg;
}msa_DeviceInfo_t;

/* Array contains the information of the devices */
static msa_DeviceInfo_t msa_DeviceRecord[MSA_MAX_DEVICE_NUM];

uint8 msa_SuperFrameOrder;
uint8 msa_BeaconOrder;

/* Task ID */
uint8 MSA_TaskId;


/**************************************************************************************************
 *                                     Local Function Prototypes
 **************************************************************************************************/
/* Setup routines */
void MSA_CoordinatorStartup(void);
void MSA_DeviceStartup(void);

/* MAC related routines */
void MSA_AssociateReq(void);
void MSA_AssociateRsp(macCbackEvent_t* pMsg);
void MSA_McpsDataReq(uint8* data, uint8 dataLength, bool directMsg, uint16 dstShortAddr);
void MSA_McpsPollReq(void);
void MSA_ScanReq(uint8 scanType, uint8 scanDuration);
void MSA_SyncReq(void);

/* Support */
bool MSA_BeaconPayLoadCheck(uint8* pSdu);
bool MSA_DataCheck(uint8* data, uint8 dataLength);


/**************************************************************************************************
 *
 * @fn          MSA_Init
 *
 * @brief       Initialize the application
 *
 * @param       taskId - taskId of the task after it was added in the OSAL task queue
 *
 * @return      none
 *
 **************************************************************************************************/
void MSA_Init(uint8 taskId)
{
  uint8 i;

  /* Initialize the task id */
  MSA_TaskId = taskId;

  /* initialize MAC features */
  MAC_InitDevice();
  MAC_InitCoord();

  /* Initialize MAC beacon */
  MAC_InitBeaconDevice();
  MAC_InitBeaconCoord();

  /* Reset the MAC */
  MAC_MlmeResetReq(TRUE);

  /* Initialize the data packet */
  for (i=MSA_HEADER_LENGTH; i<MSA_PACKET_LENGTH; i++)
  {
    msa_Data1[i] = i-MSA_HEADER_LENGTH;
  }

  /* Initialize the echo packet */
  for (i=0; i<MSA_ECHO_LENGTH; i++)
  {
    msa_Data2[i] = 0xEE;
  }

  msa_BeaconOrder = MSA_MAC_BEACON_ORDER;
  msa_SuperFrameOrder = MSA_MAC_SUPERFRAME_ORDER;

}

/**************************************************************************************************
 *
 * @fn          MSA_ProcessEvent
 *
 * @brief       This routine handles events
 *
 * @param       taskId - ID of the application task when it registered with the OSAL
 *              events - Events for this task
 *
 * @return      16bit - Unprocessed events
 *
 **************************************************************************************************/
uint16 MSA_ProcessEvent(uint8 taskId, uint16 events)
{
  uint8* pMsg;
  macCbackEvent_t* pData;
  static uint8 index;
  static uint8 sequence;

  if (events & SYS_EVENT_MSG)
  {
    while ((pMsg = osal_msg_receive(MSA_TaskId)) != NULL)
    {
      switch ( *pMsg )
      {
        case MAC_MLME_ASSOCIATE_IND:
          MSA_AssociateRsp((macCbackEvent_t*)pMsg);
          break;

        case MAC_MLME_ASSOCIATE_CNF:
          /* Retrieve the message */
          pData = (macCbackEvent_t *) pMsg;

          if ((!msa_IsStarted) && (pData->associateCnf.hdr.status == MAC_SUCCESS))
          {
            msa_IsStarted = TRUE;

            /* Retrieve MAC_SHORT_ADDRESS */

            msa_DevShortAddr = pData->associateCnf.assocShortAddress;

            /* Setup MAC_SHORT_ADDRESS - obtained from Association */
            MAC_MlmeSetReq(MAC_SHORT_ADDRESS, &msa_DevShortAddr);

            HalLedBlink(HAL_LED_4, 0, 90, 1000);

            /* Poll for data if it's not setup for direct messaging */
            if (!msa_IsDirectMsg)
            {
              osal_start_timerEx(MSA_TaskId, MSA_POLL_EVENT, MSA_WAIT_PERIOD);
            }
          }
          break;

        case MAC_MLME_COMM_STATUS_IND:
          break;

        case MAC_MLME_BEACON_NOTIFY_IND:
          /* Retrieve the message */
          pData = (macCbackEvent_t *) pMsg;

          /* Check for correct beacon payload */
          if (!msa_IsStarted)
          {
            msa_IsSampleBeacon = MSA_BeaconPayLoadCheck(pData->beaconNotifyInd.pSdu);

            /* If it's the correct beacon payload, retrieve the data for association req */
            if (msa_IsSampleBeacon)
            {
              msa_AssociateReq.logicalChannel = MSA_MAC_CHANNEL;
              msa_AssociateReq.coordAddress.addrMode = SADDR_MODE_SHORT;
              msa_AssociateReq.coordAddress.addr.shortAddr = pData->beaconNotifyInd.pPanDesc->coordAddress.addr.shortAddr;
              msa_AssociateReq.coordPanId = pData->beaconNotifyInd.pPanDesc->coordPanId;
              if (msa_IsDirectMsg)
                msa_AssociateReq.capabilityInformation = MAC_CAPABLE_ALLOC_ADDR | MAC_CAPABLE_RX_ON_IDLE;
              else
                msa_AssociateReq.capabilityInformation = MAC_CAPABLE_ALLOC_ADDR;
              msa_AssociateReq.sec.securityLevel = MAC_SEC_LEVEL_NONE;

              /* Retrieve beacon order and superframe order from the beacon */
              msa_BeaconOrder = MAC_SFS_BEACON_ORDER(pData->beaconNotifyInd.pPanDesc->superframeSpec);
              msa_SuperFrameOrder = MAC_SFS_SUPERFRAME_ORDER(pData->beaconNotifyInd.pPanDesc->superframeSpec);
            }
          }

          break;

        case MAC_MLME_START_CNF:
          /* Retrieve the message */
          pData = (macCbackEvent_t *) pMsg;
          /* Set some indicator for the Coordinator */
          if ((!msa_IsStarted) && (pData->startCnf.hdr.status == MAC_SUCCESS))
          {
            msa_IsStarted = TRUE;
            msa_IsCoordinator = TRUE;
            HalLedSet (HAL_LED_4, HAL_LED_MODE_ON);
          }
          break;

        case MAC_MLME_SCAN_CNF:
          /* Check if there is any Coordinator out there */
          pData = (macCbackEvent_t *) pMsg;

          /* If there is no other on the channel or no other with sampleBeacon start as coordinator */
          if (((pData->scanCnf.resultListSize == 0) && (pData->scanCnf.hdr.status == MAC_NO_BEACON)) || (!msa_IsSampleBeacon))
          {
            MSA_CoordinatorStartup();
          }
          else if ((msa_IsSampleBeacon) && pData->scanCnf.hdr.status == MAC_SUCCESS)
          {
            /* Start the devive up as beacon enabled or not */
            MSA_DeviceStartup();

            /* Call Associate Req */
            MSA_AssociateReq();
          }
          break;

        case MAC_MCPS_DATA_CNF:
          pData = (macCbackEvent_t *) pMsg;

          /* If last transmission completed, ready to send the next one */
          if ((pData->dataCnf.hdr.status == MAC_SUCCESS) ||
              (pData->dataCnf.hdr.status == MAC_CHANNEL_ACCESS_FAILURE) ||
              (pData->dataCnf.hdr.status == MAC_NO_ACK))
          {
            osal_start_timerEx(MSA_TaskId, MSA_SEND_EVENT, MSA_WAIT_PERIOD);
          }

          mac_msg_deallocate((uint8**)&pData->dataCnf.pDataReq);
          break;

        case MAC_MCPS_DATA_IND:
          pData = (macCbackEvent_t*)pMsg;

          if (MSA_DataCheck ( pData->dataInd.msdu.p, pData->dataInd.msdu.len ))
          {
            HalLedSet (HAL_LED_3, HAL_LED_MODE_TOGGLE);

            /* Only send the echo back if the received */
            if (!msa_IsCoordinator)
            {
              if (MSA_ECHO_LENGTH >= 4)
              {
                /* Return the first 4 bytes and the last byte of the received packet */
                msa_Data2[0] = pData->dataInd.msdu.p[0];
                msa_Data2[1] = pData->dataInd.msdu.p[1];
                msa_Data2[2] = pData->dataInd.msdu.p[2];
                msa_Data2[3] = pData->dataInd.msdu.p[3];
              }

              MSA_McpsDataReq(msa_Data2,
                              MSA_ECHO_LENGTH,
                              TRUE,
                              msa_CoordShortAddr );
            }
          }
          break;
      }

      /* Deallocate */
      mac_msg_deallocate((uint8 **)&pMsg);
    }

    return events ^ SYS_EVENT_MSG;
  }

  /* This event handles polling for in-direct message device */
  if (events & MSA_POLL_EVENT)
  {
    MSA_McpsPollReq();
    osal_start_timerEx(MSA_TaskId, MSA_POLL_EVENT, MSA_WAIT_PERIOD);

    return events ^ MSA_POLL_EVENT;
  }

  /* This event will blink a LED then send out a DataReq and continue to do so every WAIT_PERIOD */
  if (events & MSA_SEND_EVENT)
  {

    /* Do it again */
    if (msa_State == MSA_SEND_STATE)
    {
      /* Start sending */
      if (msa_IsCoordinator)
      {
        if (MSA_PACKET_LENGTH >= MSA_HEADER_LENGTH)
        {
          /* Coordinator sending to devices. Use the associated list of device to send out msg */
          msa_Data1[0] = MSA_PACKET_LENGTH;
          msa_Data1[1] = HI_UINT16(msa_DeviceRecord[index].devShortAddr);
          msa_Data1[2] = LO_UINT16(msa_DeviceRecord[index].devShortAddr);
          msa_Data1[3] = sequence;
        }

        MSA_McpsDataReq((uint8*)msa_Data1,
                        MSA_PACKET_LENGTH,
                        msa_DeviceRecord[index].isDirectMsg,
                        msa_DeviceRecord[index].devShortAddr );

        /* Reset the index if it reaches the current number of associated devices */
        if (++index == msa_NumOfDevices)
        {
          index = 0;
        }
      }
      else
      {
        if (MSA_PACKET_LENGTH >= MSA_HEADER_LENGTH)
        {
          /* Device sending to coordinator */
          msa_Data1[0] = MSA_PACKET_LENGTH;
          msa_Data1[1] = HI_UINT16(msa_CoordShortAddr);
          msa_Data1[2] = LO_UINT16(msa_CoordShortAddr);
          msa_Data1[3] = sequence;
        }
        MSA_McpsDataReq((uint8*)msa_Data1,
                        MSA_PACKET_LENGTH,
                        TRUE,
                        msa_CoordShortAddr );

      }

      if (sequence++ == 0xFF)
      {
        sequence = 0;
      }

      HalLedSet (HAL_LED_1, HAL_LED_MODE_BLINK);
    }

    return events ^ MSA_SEND_EVENT;
  }
  return 0;
}

/**************************************************************************************************
 *
 * @fn          MAC_CbackEvent
 *
 * @brief       This callback function sends MAC events to the application.
 *              The application must implement this function.  A typical
 *              implementation of this function would allocate an OSAL message,
 *              copy the event parameters to the message, and send the message
 *              to the application's OSAL event handler.  This function may be
 *              executed from task or interrupt context and therefore must
 *              be reentrant.
 *
 * @param       pData - Pointer to parameters structure.
 *
 * @return      None.
 *
 **************************************************************************************************/
void MAC_CbackEvent(macCbackEvent_t *pData)
{

  macCbackEvent_t *pMsg = NULL;

  uint8 len = msa_cbackSizeTable[pData->hdr.event];

  switch (pData->hdr.event)
  {
      case MAC_MLME_BEACON_NOTIFY_IND:

      len += sizeof(macPanDesc_t) + pData->beaconNotifyInd.sduLength +
             MAC_PEND_FIELDS_LEN(pData->beaconNotifyInd.pendAddrSpec);
      if ((pMsg = (macCbackEvent_t *) osal_msg_allocate(len)) != NULL)
      {
        /* Copy data over and pass them up */
        osal_memcpy(pMsg, pData, sizeof(macMlmeBeaconNotifyInd_t));
        pMsg->beaconNotifyInd.pPanDesc = (macPanDesc_t *) ((uint8 *) pMsg + sizeof(macMlmeBeaconNotifyInd_t));
        osal_memcpy(pMsg->beaconNotifyInd.pPanDesc, pData->beaconNotifyInd.pPanDesc, sizeof(macPanDesc_t));
        pMsg->beaconNotifyInd.pSdu = (uint8 *) (pMsg->beaconNotifyInd.pPanDesc + 1);
        osal_memcpy(pMsg->beaconNotifyInd.pSdu, pData->beaconNotifyInd.pSdu, pData->beaconNotifyInd.sduLength);
      }
      break;

    case MAC_MCPS_DATA_IND:
      pMsg = pData;
      break;

    default:
      if ((pMsg = (macCbackEvent_t *) osal_msg_allocate(len)) != NULL)
      {
        osal_memcpy(pMsg, pData, len);
      }
      break;
  }

  if (pMsg != NULL)
  {
    osal_msg_send(MSA_TaskId, (uint8 *) pMsg);
  }
}

/**************************************************************************************************
 *
 * @fn      MAC_CbackCheckPending
 *
 * @brief   Returns the number of indirect messages pending in the application
 *
 * @param   None
 *
 * @return  Number of indirect messages in the application
 *
 **************************************************************************************************/
uint8 MAC_CbackCheckPending(void)
{
  return (0);
}

/**************************************************************************************************
 *
 * @fn      MSA_CoordinatorStartup()
 *
 * @brief   Update the timer per tick
 *
 * @param   beaconEnable: TRUE/FALSE
 *
 * @return  None
 *
 **************************************************************************************************/
void MSA_CoordinatorStartup()
{
  macMlmeStartReq_t   startReq;

  /* Setup MAC_EXTENDED_ADDRESS */
  sAddrExtCpy(msa_ExtAddr, msa_ExtAddr1);
  MAC_MlmeSetReq(MAC_EXTENDED_ADDRESS, &msa_ExtAddr);

  /* Setup MAC_SHORT_ADDRESS */
  MAC_MlmeSetReq(MAC_SHORT_ADDRESS, &msa_CoordShortAddr);

  /* Setup MAC_BEACON_PAYLOAD_LENGTH */
  MAC_MlmeSetReq(MAC_BEACON_PAYLOAD_LENGTH, &msa_BeaconPayloadLen);

  /* Setup MAC_BEACON_PAYLOAD */
  MAC_MlmeSetReq(MAC_BEACON_PAYLOAD, &msa_BeaconPayload);

  /* Enable RX */
  MAC_MlmeSetReq(MAC_RX_ON_WHEN_IDLE, &msa_MACTrue);

  /* Setup MAC_ASSOCIATION_PERMIT */
  MAC_MlmeSetReq(MAC_ASSOCIATION_PERMIT, &msa_MACTrue);

  /* Fill in the information for the start request structure */
  startReq.startTime = 0;
  startReq.panId = msa_PanId;
  startReq.logicalChannel = MSA_MAC_CHANNEL;
  startReq.beaconOrder = msa_BeaconOrder;
  startReq.superframeOrder = msa_SuperFrameOrder;
  startReq.panCoordinator = TRUE;
  startReq.batteryLifeExt = FALSE;
  startReq.coordRealignment = FALSE;
  startReq.realignSec.securityLevel = FALSE;
  startReq.beaconSec.securityLevel = FALSE;

  /* Call start request to start the device as a coordinator */
  MAC_MlmeStartReq(&startReq);

}

/**************************************************************************************************
 *
 * @fn      MSA_DeviceStartup()
 *
 * @brief   Update the timer per tick
 *
 * @param   beaconEnable: TRUE/FALSE
 *
 * @return  None
 *
 **************************************************************************************************/
void MSA_DeviceStartup()
{
  uint16 AtoD = 0;

#if (defined HAL_ADC) && (HAL_ADC == TRUE)
  AtoD = HalAdcRead (MSA_HAL_ADC_CHANNEL, MSA_HAL_ADC_RESOLUTION);
#endif

  /*
    Setup MAC_EXTENDED_ADDRESS
  */
#if defined (HAL_BOARD_CC2420DB) || defined (HAL_BOARD_DZ1611) || defined (HAL_BOARD_DZ1612) || defined (HAL_BOARD_DRFG4618) || defined (HAL_BOARD_F2618)
  /* Use HI and LO of AtoD on CC2420 and MSP430 */
  msa_ExtAddr2[6] = HI_UINT16( AtoD );
  msa_ExtAddr2[7] = LO_UINT16( AtoD );
#else
  /* On 2430 Use the MSB to ensure the stability */
  msa_ExtAddr2[6] = HI_UINT16( AtoD ) >> 4;
  msa_ExtAddr2[7] = HI_UINT16( AtoD ) >> 3;
#endif

  sAddrExtCpy(msa_ExtAddr, msa_ExtAddr2);
  MAC_MlmeSetReq(MAC_EXTENDED_ADDRESS, &msa_ExtAddr);

  /* Setup MAC_BEACON_PAYLOAD_LENGTH */
  MAC_MlmeSetReq(MAC_BEACON_PAYLOAD_LENGTH, &msa_BeaconPayloadLen);

  /* Setup MAC_BEACON_PAYLOAD */
  MAC_MlmeSetReq(MAC_BEACON_PAYLOAD, &msa_BeaconPayload);

  /* Setup PAN ID */
  MAC_MlmeSetReq(MAC_PAN_ID, &msa_PanId);

  /* This device is setup for Direct Message */
  if (msa_IsDirectMsg)
    MAC_MlmeSetReq(MAC_RX_ON_WHEN_IDLE, &msa_MACTrue);
  else
    MAC_MlmeSetReq(MAC_RX_ON_WHEN_IDLE, &msa_MACFalse);

  /* Setup Coordinator short address */
  MAC_MlmeSetReq(MAC_COORD_SHORT_ADDRESS, &msa_AssociateReq.coordAddress.addr.shortAddr);

  if (msa_BeaconOrder != 15)
  {
    /* Setup Beacon Order */
    MAC_MlmeSetReq(MAC_BEACON_ORDER, &msa_BeaconOrder);

    /* Setup Super Frame Order */
    MAC_MlmeSetReq(MAC_SUPERFRAME_ORDER, &msa_SuperFrameOrder);

    /* Sync request */
    MSA_SyncReq();
  }

  /* Power saving */
  MSA_PowerMgr (MSA_PWR_MGMT_ENABLED);

}

/**************************************************************************************************
 *
 * @fn      MSA_AssociateReq()
 *
 * @brief
 *
 * @param    None
 *
 * @return  None
 *
 **************************************************************************************************/
void MSA_AssociateReq(void)
{
  MAC_MlmeAssociateReq(&msa_AssociateReq);
}

/**************************************************************************************************
 *
 * @fn      MSA_AssociateRsp()
 *
 * @brief   This routine is called by Associate_Ind inorder to return the response to the device
 *
 * @param   pMsg - pointer to the structure recieved by MAC_MLME_ASSOCIATE_IND
 *
 * @return  None
 *
 **************************************************************************************************/
void MSA_AssociateRsp(macCbackEvent_t* pMsg)
{
  /* Assign the short address  for the Device, from pool */
  uint16 assocShortAddress = msa_DevShortAddrList[msa_NumOfDevices];

  /* Build the record for this device */
  msa_DeviceRecord[msa_NumOfDevices].devShortAddr = msa_DevShortAddrList[msa_NumOfDevices];
  msa_DeviceRecord[msa_NumOfDevices].isDirectMsg = pMsg->associateInd.capabilityInformation & MAC_CAPABLE_RX_ON_IDLE;
  msa_NumOfDevices++;

  /* If the number of devices are more than MAX_DEVICE_NUM, turn off the association permit */
  if (msa_NumOfDevices == MSA_MAX_DEVICE_NUM)
    MAC_MlmeSetReq(MAC_ASSOCIATION_PERMIT, &msa_MACFalse);

  /* Fill in association respond message */
  sAddrExtCpy(msa_AssociateRsp.deviceAddress, pMsg->associateInd.deviceAddress);
  msa_AssociateRsp.assocShortAddress = assocShortAddress;
  msa_AssociateRsp.status = MAC_SUCCESS;
  msa_AssociateRsp.sec.securityLevel = MAC_SEC_LEVEL_NONE;

  /* Call Associate Response */
  MAC_MlmeAssociateRsp(&msa_AssociateRsp);
}

/**************************************************************************************************
 *
 * @fn      MSA_McpsDataReq()
 *
 * @brief   This routine calls the Data Request
 *
 * @param   data       - contains the data that would be sent
 *          dataLength - length of the data that will be sent
 *
 * @return  None
 *
 **************************************************************************************************/
void MSA_McpsDataReq(uint8* data, uint8 dataLength, bool directMsg, uint16 dstShortAddr)
{
  macMcpsDataReq_t  *pData;
  static uint8      handle = 0;

  if ((pData = MAC_McpsDataAlloc(dataLength, MAC_SEC_LEVEL_NONE, MAC_KEY_ID_MODE_NONE)) != NULL)
  {
    pData->mac.srcAddrMode = SADDR_MODE_SHORT;
    pData->mac.dstAddr.addrMode = SADDR_MODE_SHORT;
    pData->mac.dstAddr.addr.shortAddr = dstShortAddr;
    pData->mac.dstPanId = msa_PanId;
    pData->mac.msduHandle = handle++;
    pData->mac.txOptions = MAC_TXOPTION_ACK;

    /* If it's the coordinator and the device is in-direct message */
    if (msa_IsCoordinator)
    {
      if (!directMsg)
      {
        pData->mac.txOptions |= MAC_TXOPTION_INDIRECT;
      }
    }

    /* Copy data */
    osal_memcpy (pData->msdu.p, data, dataLength);

    /* Send out data request */
    MAC_McpsDataReq(pData);
  }

}

/**************************************************************************************************
 *
 * @fn      MSA_McpsPollReq()
 *
 * @brief   Performs a poll request on the coordinator
 *
 * @param   None
 *
 * @return  None
 *
 **************************************************************************************************/
void MSA_McpsPollReq(void)
{
  macMlmePollReq_t  pollReq;

  /* Fill in information for poll request */
  pollReq.coordAddress.addrMode = SADDR_MODE_SHORT;
  pollReq.coordAddress.addr.shortAddr = msa_CoordShortAddr;
  pollReq.coordPanId = msa_PanId;
  pollReq.sec.securityLevel = MAC_SEC_LEVEL_NONE;

  /* Call poll reuqest */
  MAC_MlmePollReq(&pollReq);
}

/**************************************************************************************************
 *
 * @fn      MacSampelApp_ScanReq()
 *
 * @brief   Performs active scan on specified channel
 *
 * @param   None
 *
 * @return  None
 *
 **************************************************************************************************/
void MSA_ScanReq(uint8 scanType, uint8 scanDuration)
{
  macMlmeScanReq_t scanReq;

  /* Fill in information for scan request structure */
  scanReq.scanChannels = (uint32) 1 << MSA_MAC_CHANNEL;
  scanReq.scanType = scanType;
  scanReq.scanDuration = scanDuration;
  scanReq.maxResults = MSA_MAC_MAX_RESULTS;
  scanReq.result.pPanDescriptor = msa_PanDesc;

  /* Call scan request */
  MAC_MlmeScanReq(&scanReq);
}

/**************************************************************************************************
 *
 * @fn      MSA_SyncReq()
 *
 * @brief   Sync Request
 *
 * @param   None
 *
 * @return  None
 *
 **************************************************************************************************/
void MSA_SyncReq(void)
{
  macMlmeSyncReq_t syncReq;

  /* Fill in information for sync request structure */
  syncReq.logicalChannel = MSA_MAC_CHANNEL;
  syncReq.channelPage    = MAC_CHANNEL_PAGE_0;
  syncReq.trackBeacon    = TRUE;

  /* Call sync request */
  MAC_MlmeSyncReq(&syncReq);
}

/**************************************************************************************************
 *
 * @fn      MSA_BeaconPayLoadCheck()
 *
 * @brief   Check if the beacon comes from MSA but not zigbee
 *
 * @param   pSdu - pointer to the buffer that contains the data
 *
 * @return  TRUE or FALSE
 *
 **************************************************************************************************/
bool MSA_BeaconPayLoadCheck(uint8* pSdu)
{
  uint8 i = 0;
  for (i=0; i<msa_BeaconPayloadLen; i++)
  {
    if (pSdu[i] != msa_BeaconPayload[i])
    {
      return FALSE;
    }
  }

  return TRUE;
}

/**************************************************************************************************
 *
 * @fn      MSA_DataCheck()
 *
 * @brief   Check if the data match with the predefined data
 *
 * @param    data - pointer to the buffer where the data will be checked against the predefined data
 *           dataLength - length of the data
 *
 * @return  TRUE if the data matched else it's the response / echo packet
 *
 **************************************************************************************************/
bool MSA_DataCheck(uint8* data, uint8 dataLength)
{
  uint8 i = 0;

  if (data[0] == dataLength)
  {
    for (i=MSA_HEADER_LENGTH; i<(data[0] - 1); i++)
    {
       if (data[i] != msa_Data1[i])
         return FALSE;
    }
  }
  else
  {
    return FALSE;
  }
  return TRUE;
}

/**************************************************************************************************
 *
 * @fn      MSA_HandleKeys
 *
 * @brief   Callback service for keys
 *
 * @param   keys  - keys that were pressed
 *          state - shifted
 *
 * @return  void
 *
 **************************************************************************************************/
void MSA_HandleKeys(uint8 keys, uint8 shift)
{
  if ( keys & HAL_KEY_SW_1 )
  {
    /* Start the device as a direct message device and beacon disabled*/
    if (!msa_IsStarted)
    {
      /* Decide if direct or indirect messaging is used */
      msa_IsDirectMsg = MSA_DIRECT_MSG_ENABLED;

      if (msa_IsDirectMsg)
      {
        /* Start the device as an DIRECT messaging device */
        if (msa_BeaconOrder != 15)
          MSA_ScanReq(MAC_SCAN_PASSIVE, MSA_MAC_BEACON_ORDER + 1);
        else
          MSA_ScanReq(MAC_SCAN_ACTIVE, 3);
      }
      else
      {
         /* Start the device as an INDIRECT messaging device and beacon disabled */
         /* Beacon network doesn't work with polling */
         if (!msa_IsStarted)
         {
           msa_IsDirectMsg = FALSE;
           MSA_ScanReq(MAC_SCAN_ACTIVE, 3);
         }
      }
    }
  }

  if ( keys & HAL_KEY_SW_2 )
  {
    /* Start sending message */
    if (msa_IsStarted)
    {
      /* Set App's state to SEND or IDLE */
      if (msa_State == MSA_IDLE_STATE)
        msa_State = MSA_SEND_STATE;
      else
        msa_State = MSA_IDLE_STATE;

      /* Send Event to the App to carry out the changes */
      osal_start_timerEx(MSA_TaskId, MSA_SEND_EVENT, 100);
    }
  }
}

/**************************************************************************************************
 **************************************************************************************************/
