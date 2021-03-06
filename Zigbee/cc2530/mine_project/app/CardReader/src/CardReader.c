/**************************************************************************************************
Filename:       CR.c
Revised:        $Date: 2011/08/01 20:41:20 $
Revision:       $Revision: 1.17 $

Description:    This file contains the application that can be use to set a device as Location
node from MAC directly which will communicate with a FFD with full z-stack.

**************************************************************************************************/

/**************************************************************************************************
*                                           Includes
**************************************************************************************************/
#include <stdio.h>
#include <string.h>

/* Hal Driver includes */
#include "hal_types.h"
#include "hal_key.h"
#include "hal_timer.h"
#include "hal_drivers.h"
#include "hal_led.h"
#include "hal_assert.h"
#include "hal_uart.h"

/* OS includes */
#include "OSAL.h"
#include "OSAL_Tasks.h"
//#include "OSAL_Nv.h"

/* App Protocol*/
#include "App_cfg.h"

/* Application Includes */
#include "OnBoard.h"

/* MAC Application Interface */
#include "mac_api.h"
#include "mac_main.h"
#include "mac_pib.h"
#include "ZComDef.h"
/* Application */
#include "CardReader.h"
//#include "AppProtocol.h"
//#include "AppProtocolWrapper.h"

/* watchdog util */
#include "watchdogutil.h"
#include "MacUtil.h"
#include "crc.h"

/* FLASH */
#include "Hal_flash.h"

#include "MBusProto.h"
#include "AppProtocolWrapper.h"


/*********************************************************************
* TYPEDEFS
*/
typedef enum
{
    CR_REPORT_DATA,
    CR_RESPOND,
    CR_EXCEPTION,
    CR_REPORT_VERSION,
} eUartSendType;

typedef struct
{
    /* device information */
    uint16 PanId;
    uint16 ShortAddr;
    uint8 Channel;
    sAddrExt_t extAddr;

    /* retreat */
    uint8 retreat_flag;
    uint8 retreat_cnt;
} CR_DevInfo_t;

/* Size table for MAC structures */
const CODE uint_8 CR_cbackSizeTable[] =
{
    0,                                               /* unused */
    sizeof(macMlmeAssociateInd_t),       /* MAC_MLME_ASSOCIATE_IND */
    sizeof(macMlmeAssociateCnf_t),       /* MAC_MLME_ASSOCIATE_CNF */
    sizeof(macMlmeDisassociateInd_t),    /* MAC_MLME_DISASSOCIATE_IND */
    sizeof(macMlmeDisassociateCnf_t),    /* MAC_MLME_DISASSOCIATE_CNF */
    sizeof(macMlmeBeaconNotifyInd_t),    /* MAC_MLME_BEACON_NOTIFY_IND */
    sizeof(macMlmeOrphanInd_t),           /* MAC_MLME_ORPHAN_IND */
    sizeof(macMlmeScanCnf_t),              /* MAC_MLME_SCAN_CNF */
    sizeof(macMlmeStartCnf_t),             /* MAC_MLME_START_CNF */
    sizeof(macMlmeSyncLossInd_t),        /* MAC_MLME_SYNC_LOSS_IND */
    sizeof(macMlmePollCnf_t),                /* MAC_MLME_POLL_CNF */
    sizeof(macMlmeCommStatusInd_t),      /* MAC_MLME_COMM_STATUS_IND */
    sizeof(macMcpsDataCnf_t),            /* MAC_MCPS_DATA_CNF */
    sizeof(macMcpsDataInd_t),            /* MAC_MCPS_DATA_IND */
    sizeof(macMcpsPurgeCnf_t),           /* MAC_MCPS_PURGE_CNF */
    sizeof(macEventHdr_t)                  /* MAC_PWR_ON_CNF */
};

/**************************************************************************************************
*                                           Macros
**************************************************************************************************/
#if (MBUS_BAUDRATE == 115200)
#define CR_APP_BAUD  HAL_UART_BR_115200
#endif

#if (MBUS_BAUDRATE == 9600)
#define CR_APP_BAUD  HAL_UART_BR_9600
#endif

#if !defined( CR_APP_RX_MAX )
/* The generic safe Rx minimum is 48, but if you know your PC App will not
* continue to send more than a byte after receiving the ~CTS, lower max
* here and safe min in _hal_uart.c to just 8.
*/
#define CR_APP_RX_MAX  50
#endif

// When the Rx buf space is less than this threshold, invoke the Rx callback.
#if !defined( CR_APP_THRESH )
#define CR_APP_THRESH  48
#endif

#if !defined( CR_APP_TX_MAX )
#define CR_APP_TX_MAX  250
#endif

#if !defined(CR_CARD_BUF_SIZE)
#define CR_CARD_BUF_SIZE   400
#endif

// Millisecs of idle time after a byte is received before invoking Rx callback.
#if !defined( CR_APP_IDLE )
#define CR_APP_IDLE  8
#endif

#define CR_MAX_CARD_IN_PACKET ((MBUS_FRAME_MAX_LEN-sizeof(mbus_hdr_slv_t) - sizeof(mbus_tlv_t) - 2)/sizeof(mbus_card_t))

#define CR_LED_UART0      HAL_LED_1
#define CR_LED_UART1      HAL_LED_3
#define CR_LED_AIR         HAL_LED_2

/**************************************************************************************************
*                                        Local Variables
**************************************************************************************************/
/* Coordinator and Device information */
static uint16        CR_PanId = 0xFFFF;
static uint8         CR_DeviceID = MBUS_IDMAX;


/* TRUE and FALSE value */
static bool CR_MACTrue = TRUE;
static bool CR_MACFalse = FALSE;

/* Task ID */
uint8 CR_TaskId;

/* Device Info from flash */
static CR_DevInfo_t CR_DevInfo;


/* UART*/
static uint_8 uartPort = HAL_UART_PORT_0;
static uint_8 uartPortTemp = HAL_UART_PORT_0;

static uint_8 uartReadBufLen0 = 20;
static uint_8 uartReadBuf0[20];
static uint_8 uartReadLen0;

static uint_8 uartReadBufLen1 = 20;
static uint_8 uartReadBuf1[20];
static uint_8 uartReadLen1;

static eUartSendType MBUS_send_type;
static UrgentType urgentType = URGENT_NONE;
static uint_8 urgentCount = 0;

static mbus_card_t cardMsgBuf[CR_CARD_BUF_SIZE];
static uint_16 cardBufHead;
static uint_16 cardBufTail;
static uint_16 cardBufSendIndex = 0;
static bool fSendIndexValid = FALSE;

static uint_8 sendSeqNum = 0;

static bool fSending = FALSE;

static uint_8 sendData[MBUS_FRAME_MAX_LEN];
static uint_16 recIndex;

static bool fClearAndReset = FALSE;

static uint_8 versionBuf[128];
/**************************************************************************************************
*                                     Local Function Prototypes
**************************************************************************************************/
/* Setup routines */
static  void    init_uart(uint8  uartPort , uint8 baudRate);
static void     CR_Startup(void);
static void     CR_ReadDevInfo(void);
static void     CR_start_timerEx(uint8 taskID,
                                 uint16 event_id,
                                 uint16 timeout_value);
static void CR_ParseAirMessage(macCbackEvent_t *p);
static void CR_ParseUartCmd(uint_8* data, uint_16 len);
/**************************************************************************************************
*
* @fn          CR_Init
*
* @brief       Initialize the application
*
* @param       taskId - taskId of the task after it was added in the OSAL task queue
*
* @return      none
*
**************************************************************************************************/
void CR_Init(uint_8 taskId)
{
    /* Initialize the task id */
    CR_TaskId = taskId;

    /* initialize MAC features */
    MAC_Init();
    MAC_InitCoord();

    /* Reset the MAC */
    MAC_MlmeResetReq(TRUE);

#if(defined WATCHDOG) && (WATCHDOG==TRUE)
    StartWatchDog(DOGTIMER_INTERVAL_1S);
    osal_set_event(CR_TaskId, CR_FEEDWATCHDOG_EVENT);
#endif

    /*init devinfo */
    CR_ReadDevInfo();

    /* turn off leds */
    HalLedSet(CR_LED_UART0, HAL_LED_MODE_OFF);
    HalLedSet(CR_LED_UART1, HAL_LED_MODE_OFF);
    HalLedSet(CR_LED_AIR, HAL_LED_MODE_OFF);

    init_uart(HAL_UART_PORT_0,CR_APP_BAUD);

    /* initial macutil */
    MacUtil_t MacUtil;
    MacUtil.panID = 0xFFFF;  /* default is card nwk id */
    MacUtil.dst_endpoint = MINEAPP_ENDPOINT;
    MacUtil.src_endpoint = 0x20;//MINEAPP_ENDPOINT;
    MacUtil.profile_id = 0x0100;//MINEAPP_PROFID;
    MacUtil.cluster_id = CARD_CLUSTERID;
    MacUtil.NodeType = NODETYPE_COORDINATOR;
    MAC_UTIL_INIT(&MacUtil);

    /* start up*/
    CR_Startup();

    CR_start_timerEx(CR_TaskId, CR_UART_READ0_EVENT, 1000);
    CR_start_timerEx(CR_TaskId, CR_LED_CONTROL_EVENT, 2000);
}

/**************************************************************************************************
*
* @fn          init_uart
*
* @brief       init uart
*
* @param       UART_PORT - number of  uart port
*              baudRate -
*
* @return      void
*
**************************************************************************************************/
void init_uart(uint8  uartPort , uint8 baudRate)
{
    halUARTCfg_t uartConfig;

    uartConfig.configured           = TRUE;
    uartConfig.baudRate             = baudRate;
    uartConfig.flowControl          = FALSE;
    uartConfig.flowControlThreshold = CR_APP_THRESH;
    uartConfig.rx.maxBufSize        = CR_APP_RX_MAX;
    uartConfig.tx.maxBufSize        = CR_APP_TX_MAX;
    uartConfig.idleTimeout          = CR_APP_IDLE;
    uartConfig.intEnable            = TRUE;
    uartConfig.callBackFunc         = NULL;

    /*two ports. one for backup*/
    HalUARTOpen(uartPort, &uartConfig);

}

/**************************************************************************************************
*
* @fn          CR_ProcessEvent
*
* @brief       This routine handles events
*
* @param       taskId - ID of the application task when it registered with the OSAL
*              events - Events for this task
*
* @return      16bit - Unprocessed events
*
**************************************************************************************************/
uint16 CR_ProcessEvent(uint_8 taskId, uint16 events)
{
    uint_8* pMsg;
    macCbackEvent_t* pData;

#if(defined WATCHDOG) && (WATCHDOG==TRUE)
    if(events & CR_FEEDWATCHDOG_EVENT)
    {
        osal_start_timerEx(CR_TaskId, CR_FEEDWATCHDOG_EVENT, 300);
        FeedWatchDog();
        return events ^ CR_FEEDWATCHDOG_EVENT;
    }
#endif

    if(events & SYS_EVENT_MSG)
    {
        while((pMsg = osal_msg_receive(CR_TaskId)) != NULL)
        {
            switch(*pMsg)
            {
            case MAC_MLME_START_CNF:
            {
                /* if start fail, restart */
                pData = (macCbackEvent_t *) pMsg;
                if(pData->startCnf.hdr.status != ZSUCCESS)
                {
                    SystemReset();
                }
                break;
            }
            case MAC_MCPS_DATA_CNF:
            {
                pData = (macCbackEvent_t *) pMsg;
                osal_msg_deallocate((uint_8 *) pData->dataCnf.pDataReq);
                break;
            }

            case MAC_MCPS_DATA_IND:
            {
                pData = (macCbackEvent_t *) pMsg;

                CR_ParseAirMessage(pData);
                break;
            }

            }

            /* Deallocate */
            osal_msg_deallocate((uint_8 *) pMsg);
        }

        return events ^ SYS_EVENT_MSG;
    }

    if(events & CR_LED_CONTROL_EVENT)
    {
        HalLedSet(CR_LED_AIR, HAL_LED_MODE_OFF);
        HalLedSet(CR_LED_UART0, HAL_LED_MODE_OFF);
        HalLedSet(CR_LED_UART1, HAL_LED_MODE_OFF);
        osal_start_timerEx(taskId, CR_LED_CONTROL_EVENT, 1000);
        return events ^ CR_LED_CONTROL_EVENT;
    }

    if(events & CR_UART_READ0_EVENT)
    {
        uint_8 len;
        static bool fIgnore = FALSE;

        if(fSending)
        {
            osal_start_timerEx(taskId, CR_UART_READ0_EVENT, MBUS_FRAME_TIMEOUT);
            return events ^ CR_UART_READ0_EVENT;
        }

        len = HalUARTRead(HAL_UART_PORT_0, uartReadBuf0 + uartReadLen0, uartReadBufLen0 - uartReadLen0);

        uartReadLen0 += len;

        // too long to be a possible cmd from master
        if(uartReadLen0 > sizeof(mbus_hdr_mstr_t) + 2 && len > 0)
        {
            fIgnore = TRUE;
        }

        //continue ignoring till len == 0
        if(fIgnore)
        {
            uartReadLen0 = 0;
        }

        if(len == 0)
        {
            fIgnore = FALSE;
        }


        uint_8 sizet = sizeof(mbus_hdr_mstr_t) + 2;
        if(uartReadLen0 == sizet && len == 0)
        {
            uartPortTemp = HAL_UART_PORT_0;
            CR_ParseUartCmd(uartReadBuf0, uartReadLen0);
            uartReadLen0 = 0;
        }
        else if(uartReadLen0 > 0 && len == 0)
        {
            uartReadLen0 = 0;
        }

        osal_start_timerEx(taskId, CR_UART_READ0_EVENT, MBUS_FRAME_TIMEOUT);
        return events ^ CR_UART_READ0_EVENT;
    }

    if(events & CR_UART_READ1_EVENT)
    {
        uint_8 len;
        static bool fIgnore = FALSE;

        if(fSending)
        {
            osal_start_timerEx(taskId, CR_UART_READ1_EVENT, MBUS_FRAME_TIMEOUT);
            return events ^ CR_UART_READ0_EVENT;
        }

        len = HalUARTRead(HAL_UART_PORT_1, uartReadBuf1 + uartReadLen1, uartReadBufLen1 - uartReadLen1);

        uartReadLen1 += len;

        // too long to be a possible cmd from master
        if(uartReadLen1 > sizeof(mbus_hdr_mstr_t) + 2 && len > 0)
        {
            fIgnore = TRUE;
        }

        //continue ignoring till len == 0
        if(fIgnore)
        {
            uartReadLen1 = 0;
        }

        if(len == 0)
        {
            fIgnore = FALSE;
        }

        if(uartReadLen1 == sizeof(mbus_hdr_mstr_t) + 2 && len == 0)
        {
            uartPortTemp = HAL_UART_PORT_1;
            CR_ParseUartCmd(uartReadBuf1, uartReadLen1);
            uartReadLen1 = 0;
        }
        else if(uartReadLen1 > 0 && len == 0)
        {
            uartReadLen1 = 0;
        }

        osal_start_timerEx(taskId, CR_UART_READ1_EVENT, MBUS_FRAME_TIMEOUT);
        return events ^ CR_UART_READ1_EVENT;
    }

    if(events & CR_UART_WRITE_EVENT)
    {
        if(MBUS_send_type == CR_REPORT_DATA)
        {
            uint_16 txAvail = Hal_UART_TxBufLen(uartPort);

            static bool fInit = FALSE;
            static uint_16 cardNum = 0;

            //first time
            if(!fInit)
            {

                fInit = TRUE;
                fSending = TRUE;
                //HAL_ASSERT(txAvail == CR_APP_TX_MAX-1 );

                if(txAvail != CR_APP_TX_MAX - 1)
                {
                    HalUARTFlushTxBuf(uartPort);
                }

                mbus_hdr_slv_t mbusHead;
                mbus_tlv_t mbusTlv;

                cardNum = (cardBufHead >= cardBufTail) ?
                          (cardBufHead - cardBufTail) :
                          (CR_CARD_BUF_SIZE + cardBufHead - cardBufTail);

                if(cardNum > CR_MAX_CARD_IN_PACKET)
                {
                    cardNum = CR_MAX_CARD_IN_PACKET;
                }

                osal_memcpy(mbusHead.sync, MBUS_SYNC, sizeof(mbusHead.sync));
                mbusHead.cmd = MBUS_CMD_RSP;
                MBUS_SET_SLAVE_VERSION(mbusHead.frame_control, MBUS_PROTO_VERSION);
                mbusHead.slv_id = CR_DeviceID;
                MBUS_SET_SLAVE_SEQ(mbusHead.frame_control, sendSeqNum);

                if(cardNum > 0)
                    mbusHead.data_len = sizeof(mbus_tlv_t) + cardNum * sizeof(mbus_card_t);
                else
                    mbusHead.data_len = sizeof(mbus_tlv_t);

                mbusTlv.len = cardNum * sizeof(mbus_card_t);
                mbusTlv.type = MBUS_TLV_CARD_READER;

                HalUARTWrite(uartPort, (uint_8*)(&mbusHead), sizeof(mbusHead));
                osal_memcpy(sendData + recIndex, (uint_8*)(&mbusHead), sizeof(mbusHead));
                recIndex += sizeof(mbusHead);

                txAvail -= sizeof(mbusHead);

                /* if no card , also send mbusTlv to ARM */
                // if(cardNum > 0)
                // {
                HalUARTWrite(uartPort, (uint_8*)(&mbusTlv), sizeof(mbusTlv));
                osal_memcpy(sendData + recIndex, (uint_8*)(&mbusTlv), sizeof(mbusTlv));
                recIndex += sizeof(mbusTlv);

                txAvail -= sizeof(mbusTlv);
                //   }

            }


            uint_8 count = txAvail / sizeof(mbus_card_t);
            uint_8 left = 0;

            /* in case the uart writing speed is slower than card coming speed
            */
            if(!fSendIndexValid)
            {
                cardBufSendIndex = cardBufTail;
            }


            if(count > cardNum) count = cardNum;
            if(count > 0)
            {
                uint_8 tmpCount = count;
                if(tmpCount + cardBufSendIndex > CR_CARD_BUF_SIZE)
                {
                    left = tmpCount - (CR_CARD_BUF_SIZE - cardBufSendIndex);
                    tmpCount = (CR_CARD_BUF_SIZE - cardBufSendIndex);
                }
                HalUARTWrite(uartPort, (uint_8*)(&cardMsgBuf[cardBufSendIndex]), tmpCount * sizeof(mbus_card_t));
                osal_memcpy(sendData + recIndex, (uint_8*)(&cardMsgBuf[cardBufSendIndex]), tmpCount * sizeof(mbus_card_t));
                recIndex += tmpCount * sizeof(mbus_card_t);

                if(left)
                {
                    HalUARTWrite(uartPort, (uint_8*)(&cardMsgBuf[0]), left * sizeof(mbus_card_t));
                    osal_memcpy(sendData + recIndex, (uint_8*)(&cardMsgBuf[0]), left * sizeof(mbus_card_t));
                    recIndex += left * sizeof(mbus_card_t);
                }

                /*sendIndex moved, then send to true*/
                fSendIndexValid = TRUE;
            }
            /*something wrong with uartTX*/
            else if(cardNum != 0)
            {
                recIndex = 0;
                fInit = FALSE;
                fSending = FALSE;
                return events ^ CR_UART_WRITE_EVENT;
            }

            cardNum -= count;
            cardBufSendIndex += count;
            if(cardBufSendIndex >= CR_CARD_BUF_SIZE)
            {
                cardBufSendIndex -= CR_CARD_BUF_SIZE;
            }
            txAvail -= count * sizeof(mbus_card_t);

            //fill CRC
            if(cardNum == 0 && txAvail > 2)
            {
                uint_16 recCRC = CRC16(sendData, recIndex, 0xFFFF);
                HalUARTWrite(uartPort, (uint_8*)(&recCRC), sizeof(recCRC));

                recIndex = 0;
                fInit = FALSE;
                fSending = FALSE;
            }
            else
            {
                /*txbuf full, wait txbuf to be empty*/
                uint_32 bits = CR_APP_TX_MAX;
                bits *= 8 * 1000;
                osal_start_timerEx(taskId, CR_UART_WRITE_EVENT, bits / MBUS_BAUDRATE);
            }
        }

        if(MBUS_send_type == CR_RESPOND)
        {
            uint_16 txAvail = Hal_UART_TxBufLen(uartPort);

            uint_16 crc = 0xFFFF;

            if(txAvail != CR_APP_TX_MAX - 1)
            {
                HalUARTFlushTxBuf(uartPort);
            }

            mbus_hdr_slv_t mbusHead;

            osal_memcpy(mbusHead.sync, MBUS_SYNC, sizeof(mbusHead.sync));
            mbusHead.cmd = MBUS_CMD_RSP;
            MBUS_SET_SLAVE_VERSION(mbusHead.frame_control, MBUS_PROTO_VERSION);
            mbusHead.slv_id = CR_DeviceID;
            mbusHead.data_len = 0;

            /*seq is always 0 in this mode*/
            MBUS_SET_SLAVE_SEQ(mbusHead.frame_control, 0);

            crc = CRC16((uint_8*)(&mbusHead), sizeof(mbusHead), crc);
            HalUARTWrite(uartPort, (uint_8*)(&mbusHead), sizeof(mbusHead));
            HalUARTWrite(uartPort, (uint_8*)(&crc), sizeof(crc));

            if(fClearAndReset)
            {
                txAvail = Hal_UART_TxBufLen(uartPort);
                while(txAvail != CR_APP_TX_MAX - 1)
                {
                    txAvail = Hal_UART_TxBufLen(uartPort);
                }
                SystemReset();
            }


        }

        if(MBUS_send_type == CR_EXCEPTION)
        {
            uint_16 crc;
            uint_16 txAvail = Hal_UART_TxBufLen(uartPort);

            HAL_ASSERT(txAvail == CR_APP_TX_MAX - 1);

            mbus_hdr_slv_t mbusHead;
            mbusHead.cmd = MBUS_CMD_EXCEPCTION;
            mbusHead.data_len = 0;
            mbusHead.slv_id = CR_DeviceID;
            MBUS_SET_SLAVE_VERSION(mbusHead.frame_control, MBUS_PROTO_VERSION);
            /*seq is always 0 in this mode*/
            MBUS_SET_SLAVE_SEQ(mbusHead.frame_control, 0);

            HalUARTWrite(uartPort, (uint_8*)(&mbusHead), sizeof(mbusHead));
            txAvail -= sizeof(mbusHead);
            crc = CRC16((uint_8*)(&mbusHead), sizeof(mbusHead), 0xFFFF);

            HalUARTWrite(uartPort, (uint_8*)(&crc), sizeof(crc));

        }

        if(MBUS_send_type == CR_REPORT_VERSION)
        {
            // send  mbus_hdr_slv_t + mbus_tlv_t + "hardversion" + "version"

#define CR_HARDWARE_VERSION "2530CardReader "

            uint_16 crc;
            uint_16 txAvail = Hal_UART_TxBufLen(uartPort);

            uint16 version_len = strlen(CR_HARDWARE_VERSION) + strlen(VERSION) + 1;   // with one '\0'

            mbus_hdr_slv_t  mbusHead;
            mbus_tlv_t      mbusTlv;

            osal_memcpy(mbusHead.sync, MBUS_SYNC, sizeof(mbusHead.sync));

            mbusHead.cmd = MBUS_CMD_VERSION_RSP;
            mbusHead.data_len = sizeof(mbus_tlv_t) + version_len;
            mbusHead.slv_id = CR_DeviceID;

            mbusHead.frame_control = 0;
            MBUS_SET_SLAVE_VERSION(mbusHead.frame_control, MBUS_PROTO_VERSION);
            /*seq is always 0 in this mode*/
            MBUS_SET_SLAVE_SEQ(mbusHead.frame_control, 0);

            mbusTlv.type = MBUS_TLV_VERSION_CARD_READER;
            mbusTlv.len = version_len;

            uint16 idx = 0;
            osal_memcpy(versionBuf, (void*)&mbusHead, sizeof(mbusHead));
            idx += sizeof(mbusHead);
            osal_memcpy(versionBuf+idx, (void*)&mbusTlv, sizeof(mbusTlv));
            idx += sizeof(mbusTlv);
            strcpy(versionBuf+idx, CR_HARDWARE_VERSION);
            idx += strlen(CR_HARDWARE_VERSION);
            strcpy(versionBuf+idx, VERSION);
            idx += strlen(VERSION) + 1;


            HalUARTWrite(uartPort, versionBuf, idx);
            txAvail -= idx;
            crc = CRC16(versionBuf, idx, 0xFFFF);

            HalUARTWrite(uartPort, (uint_8*)(&crc), sizeof(crc));

        }


        return events ^ CR_UART_WRITE_EVENT;
    }

    if(events & CR_BACKTO_NORMAL_EVENT)
    {
        if(urgentCount < 10)
        {
            urgentCount++;
            osal_start_timerEx(taskId, CR_BACKTO_NORMAL_EVENT, 60000);
        }
        else
        {
            urgentType = URGENT_NONE;
        }

        return events ^ CR_BACKTO_NORMAL_EVENT;
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
void MAC_CbackEvent(macCbackEvent_t* pData)
{
    macCbackEvent_t* pMsg = NULL;

    uint_8 len = CR_cbackSizeTable[pData->hdr.event];

    switch (pData->hdr.event)
    {
    case MAC_MLME_BEACON_NOTIFY_IND:
        len += sizeof(macPanDesc_t)
               + pData->beaconNotifyInd.sduLength
               + MAC_PEND_FIELDS_LEN(pData->beaconNotifyInd.pendAddrSpec);
        if ((pMsg = (macCbackEvent_t *) osal_msg_allocate(len)) != NULL)
        {
            /* Copy data over and pass them up */
            osal_memcpy(pMsg, pData, sizeof(macMlmeBeaconNotifyInd_t));
            pMsg->beaconNotifyInd.pPanDesc = (macPanDesc_t *)
                                             ((uint_8 *) pMsg
                                              + sizeof(macMlmeBeaconNotifyInd_t));
            osal_memcpy(pMsg->beaconNotifyInd.pPanDesc,
                        pData->beaconNotifyInd.pPanDesc,
                        sizeof(macPanDesc_t));
            pMsg->beaconNotifyInd.pSdu = (uint_8 *)
                                         (pMsg->beaconNotifyInd.pPanDesc + 1);
            osal_memcpy(pMsg->beaconNotifyInd.pSdu,
                        pData->beaconNotifyInd.pSdu,
                        pData->beaconNotifyInd.sduLength);
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
        if (SUCCESS != osal_msg_send(CR_TaskId, (uint8 *) pMsg))
        {
            SystemReset();
        }
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
uint_8 MAC_CbackCheckPending(void)
{
    return (0);
}


/**************************************************************************************************
*
* @fn      CR_Startup()
*
* @brief   Update the timer per tick
*
* @param   beaconEnable: TRUE/FALSE
*
* @return  None
*
**************************************************************************************************/
void CR_Startup()
{
    macMlmeStartReq_t startReq;

    /* Setup MAC_EXTENDED_ADDRESS */
    MAC_MlmeSetReq(MAC_EXTENDED_ADDRESS, &CR_DevInfo.extAddr);

    /* Setup MAC_SHORT_ADDRESS */
    CR_DevInfo.ShortAddr = 0;
    MAC_MlmeSetReq(MAC_SHORT_ADDRESS, &CR_DevInfo.ShortAddr);

    /* Setup MAC_BEACON_PAYLOAD_LENGTH */
    uint_8 tmp8 = 0;
    MAC_MlmeSetReq(MAC_BEACON_PAYLOAD_LENGTH, &tmp8);

    /* Enable RX */
    MAC_MlmeSetReq(MAC_RX_ON_WHEN_IDLE, &CR_MACTrue);

    /* Setup MAC_ASSOCIATION_PERMIT */
    MAC_MlmeSetReq(MAC_ASSOCIATION_PERMIT, &CR_MACFalse);

    /* change CCA param */
    uint8 maxFrameRetries = 4;
    MAC_MlmeSetReq(MAC_MAX_FRAME_RETRIES, &maxFrameRetries);

    uint8 maxCsmaBackoff = 5;
    MAC_MlmeSetReq(MAC_MAX_CSMA_BACKOFFS, &maxCsmaBackoff);

    uint8 minBe = 4;
    MAC_MlmeSetReq(MAC_MIN_BE, &minBe);

    uint8 maxBe = 6;
    MAC_MlmeSetReq(MAC_MAX_BE, &maxBe);

    /* Fill in the information for the start request structure */
    startReq.startTime = 0;
    startReq.panId = CR_DevInfo.PanId;
    startReq.logicalChannel = CR_DevInfo.Channel;
    startReq.beaconOrder = 0xF;
    startReq.superframeOrder = 0xF;
    startReq.panCoordinator = TRUE;
    startReq.batteryLifeExt = FALSE;
    startReq.coordRealignment = FALSE;
    startReq.realignSec.securityLevel = FALSE;
    startReq.beaconSec.securityLevel = FALSE;

    /* Call start request to start the device as a coordinator */
    MAC_MlmeStartReq(&startReq);
}

void CR_ReadDevInfo(void)
{

    HalFlashRead(HAL_FLASH_IEEE_PAGE, HAL_FLASH_IEEE_OSET,
                 (uint8 *)&CR_DevInfo.extAddr, HAL_FLASH_IEEE_SIZE);

    /* DeviceID */
    CR_DevInfo.PanId = BUILD_UINT16(CR_DevInfo.extAddr[EXT_MACADDR_DEVID_LBYTE], CR_DevInfo.extAddr[EXT_MACADDR_DEVID_HBYTE]);
    HAL_ASSERT( CR_DevInfo.PanId  > 30000 &&  CR_DevInfo.PanId  < 30255);
    CR_DeviceID =  CR_DevInfo.PanId - 30000;

    HAL_ASSERT(CR_DevInfo.extAddr[EXT_MACADDR_TYPE] == EXT_MACADDR_TYPE_CARDREADER);


    /* Channel */
    uint8 channel = CR_DevInfo.extAddr[EXT_MACADDR_CHANNEL];

    if(!(channel>=11 && channel <=26))
    {
        channel = 0x0B;   //use default channel 11;
    }

    CR_DevInfo.Channel =  channel;

    CR_DevInfo.ShortAddr = 0x00;
}

static void CR_start_timerEx(uint8 taskID,
                             uint16 event_id,
                             uint16 timeout_value)
{
    if (ZSuccess != osal_start_timerEx(taskID, event_id, timeout_value))
    {
        SystemReset();
    }
}

void CR_ParseAirMessage(macCbackEvent_t *p)
{
    uint_8* data = p->dataInd.msdu.p;
    uint_8 len = p->dataInd.msdu.len;
    uint_16 srcAddr = p->dataInd.mac.srcAddr.addr.shortAddr;
    int_8 rssi = p->dataInd.mac.rssi;

    sData_t sData;
    sData.p = data;
    sData.len = len;

    uint_8 clusterID = MAC_UTIL_GetClusterID(sData);

    sData_t ApsData = MAC_UTIL_RemoveHeader(sData);

    bool flag = FALSE;

    HAL_TOGGLE_LED3();

    if(clusterID == CHARGEED_CLUSTERID)
    {
        APPWrapper_t* AppPkt = (APPWrapper_t*)(ApsData.p);
        uint_8 appflag = AppPkt->app_flag;
        switch(appflag)
        {
        case CHARGEED_SSREQ:
        {
            uint8 reqtype = AppPkt->app_chargeed_ssReq.reqtype;

            flag = TRUE;
            if(reqtype == SSREQ_POLL)
            {
                app_chargeed_ssRsp_t app_chargeed_ssRsp;
                app_chargeed_ssRsp.msgtype = CHARGEED_SSRSP;
                app_chargeed_ssRsp.srcPan = CR_PanId;
                app_chargeed_ssRsp.locnode_num = CR_DeviceID;
                app_chargeed_ssRsp.seqnum = AppPkt->app_chargeed_ssReq.seqnum;

                app_chargeed_ssRsp.urgent_type = urgentType;
                app_chargeed_ssRsp.urgent_value = 0;

                MacParam_t param;
                param.cluster_id = CHARGEED_CLUSTERID;
                param.panID = CARD_NWK_ADDR;
                param.radius = 1;

                if (URGENT_NONE != app_chargeed_ssRsp.urgent_type)
                {
                    MAC_UTIL_BuildandSendDataPAN(&param, (uint8 *) &app_chargeed_ssRsp,
                                                 sizeof(app_chargeed_ssRsp_t),
                                                 MAC_UTIL_UNICAST, srcAddr,
                                                 MAC_TXOPTION_NO_CNF);
                }
            }

            /* If received a SSREQ_OUT or SSREQ_POLL , Send SSIND to SPI*/
            if(reqtype == SSREQ_OUT || reqtype == SSREQ_POLL)
            {
                cardMsgBuf[cardBufHead].type = MBUS_TYPE_RSSI;

                cardMsgBuf[cardBufHead].data.cardrssi.card_num = srcAddr;
                cardMsgBuf[cardBufHead].data.cardrssi.seq = AppPkt->app_chargeed_ssReq.seqnum;
                cardMsgBuf[cardBufHead].data.cardrssi.rssi = rssi;

            }
            else
            {
                flag = FALSE;
            }
            break;
        }
        case URGENT:
        {
            uint8 chargeed_urgenttype = AppPkt->app_Urgent.urgenttype;

            flag = TRUE;
            if(chargeed_urgenttype == ALERT)
            {
                app_Urgent_t app_Urgent;
                app_Urgent.msgtype = URGENT;
                app_Urgent.urgenttype = ALERTACK;
                app_Urgent.value = 0;

                MacParam_t param;
                param.cluster_id = CHARGEED_CLUSTERID;
                param.panID = CARD_NWK_ADDR;
                param.radius = 1;
                MAC_UTIL_BuildandSendDataPAN(&param, (uint8 *) &app_Urgent, sizeof(app_Urgent_t),
                                             MAC_UTIL_UNICAST, srcAddr,
                                             MAC_TXOPTION_NO_CNF);

                cardMsgBuf[cardBufHead].data.alm.value = AppPkt->app_Urgent.value;
                cardMsgBuf[cardBufHead].data.alm.card_num = srcAddr;
                cardMsgBuf[cardBufHead].type = MBUS_TYPE_ALARM;
            }
            else if(chargeed_urgenttype == NOPWR)
            {
                cardMsgBuf[cardBufHead].data.nopower.value = AppPkt->app_Urgent.value;
                cardMsgBuf[cardBufHead].data.nopower.card_num = srcAddr;
                cardMsgBuf[cardBufHead].type = MBUS_TYPE_NOPOWER;
            }
            else
            {
                flag = FALSE;
            }

            break;
        }
        default:
            break;
        }

    }

    if(clusterID == CARD_CLUSTERID)
    {
        APPWrapper_t* AppPkt = (APPWrapper_t*)(ApsData.p);
        uint_8 appflag = AppPkt->app_flag;
        switch(appflag)
        {
        case SSREQ:
        {
            uint8 reqtype = AppPkt->app_ssReq.reqtype;

            flag = TRUE;
            if(reqtype == SSREQ_POLL)
            {
                if(URGENT_NONE ==urgentType)
                {
                    app_ssReq_t app_ssReq;
                    app_ssReq.msgtype = SSREQ;
                    app_ssReq.reqtype = SSREQ_NODATA;
                    app_ssReq.NWK_ADDR = CARD_NWK_ADDR;
                    app_ssReq.seqnum = AppPkt->app_ssReq.seqnum;

                    MacParam_t param;
                    param.cluster_id = CARD_CLUSTERID;
                    param.panID = CARD_NWK_ADDR;
                    param.radius = 1;

                    MAC_UTIL_BuildandSendDataPAN(&param, (uint8 *) &app_ssReq, sizeof(app_ssReq_t),
                                                 MAC_UTIL_UNICAST, srcAddr,
                                                 MAC_TXOPTION_NO_CNF);
                }
                else if(RETREAT ==urgentType ||CANCELRETREAT ==urgentType )
                {
                    app_Urgent_t app_Urgent;
                    app_Urgent.msgtype = URGENT;
                    app_Urgent.urgenttype = urgentType;
                    app_Urgent.value = 0;

                    MacParam_t param;
                    param.cluster_id = CARD_CLUSTERID;
                    param.panID = CARD_NWK_ADDR;
                    param.radius = 1;

                    //FIXME, why not retransmit when urgent???
                    MAC_UTIL_BuildandSendDataPAN(&param, (uint8 *) &app_Urgent, sizeof(app_Urgent),
                                                 MAC_UTIL_UNICAST, srcAddr,   MAC_TXOPTION_NO_CNF);
                }

            }

            /* If received a SSREQ_OUT or SSREQ_POLL , Send SSIND to SPI*/
            if(reqtype == SSREQ_OUT || reqtype == SSREQ_POLL)
            {
                cardMsgBuf[cardBufHead].type = MBUS_TYPE_RSSI;

                cardMsgBuf[cardBufHead].data.cardrssi.card_num = srcAddr;
                cardMsgBuf[cardBufHead].data.cardrssi.seq = AppPkt->app_ssReq.seqnum;
                cardMsgBuf[cardBufHead].data.cardrssi.rssi = rssi;

            }
            break;
        }

        case URGENT:
        {
            uint8 urType = AppPkt->app_Urgent.urgenttype;

            flag = TRUE;
            if(urType == ALERT)
            {
                app_Urgent_t app_Urgent;
                app_Urgent.msgtype = URGENT;
                app_Urgent.urgenttype = ALERTACK;
                app_Urgent.value = 0;

                MacParam_t param;
                param.cluster_id = CARD_CLUSTERID;
                param.panID = CARD_NWK_ADDR;
                param.radius = 1;
                MAC_UTIL_BuildandSendDataPAN(&param, (uint8 *) &app_Urgent, sizeof(app_Urgent_t),
                                             MAC_UTIL_UNICAST, srcAddr,
                                             MAC_TXOPTION_NO_CNF);

                cardMsgBuf[cardBufHead].data.alm.value = AppPkt->app_Urgent.value;
                cardMsgBuf[cardBufHead].data.alm.card_num = srcAddr;
                cardMsgBuf[cardBufHead].type = MBUS_TYPE_ALARM;
            }
            else if(urType == NOPWR)
            {
                cardMsgBuf[cardBufHead].data.nopower.value = AppPkt->app_Urgent.value;
                cardMsgBuf[cardBufHead].data.nopower.card_num = srcAddr;
                cardMsgBuf[cardBufHead].type = MBUS_TYPE_NOPOWER;
            }
            break;
        }
        default:
            break;
        }

    }

    if(flag)
    {

#ifdef DEBUG_CR
        totalCards++;
#endif

        cardBufHead++;
        if(cardBufHead >= CR_CARD_BUF_SIZE)
        {
            cardBufHead = 0;
        }

        if(cardBufHead == cardBufTail)
        {
            cardBufTail++;
            if(cardBufTail == CR_CARD_BUF_SIZE)
                cardBufTail = 0;

            /*the tail catch up the sendIndex*/
            if(fSendIndexValid && cardBufTail == cardBufSendIndex)
                fSendIndexValid  = FALSE;
        }
    }

    //HAL_TOGGLE_LED3();

}


/******************************************************************************
********************
*
* @fn      CR_ParseUartCmd
*
* @brief   parse the uart coming data
*
*******************************************************************************
*******************/
static void CR_ParseUartCmd(uint_8* data, uint_16 len)
{
    /*CRC doesn't include the sync unit*/
    uint_16 crc =  CRC16(data, len - 2, 0xFFFF);
    bool f_CRC = TRUE;
    uint_8 cmdType;
    uint_8 querySeq;

    if(crc != *((uint_16*)(data + len - 2)))
        f_CRC = FALSE;

    if(!f_CRC)
    {
        //MBUS_send_type = CR_EXCEPTION;
        //osal_start_timerEx(CR_TaskId, CR_UART_WRITE_EVENT, MBUS_FRAME_INTERVAL);
        return;
    }

    if(uartPortTemp == HAL_UART_PORT_0)
        HAL_TOGGLE_LED1();
    else
        HAL_TOGGLE_LED2();

    if(((mbus_hdr_mstr_t*)data)->slv_id != CR_DeviceID)
    {
        return;
    }
    if(MBUS_GET_MASTER_VERSION(((mbus_hdr_mstr_t*)data)->frame_control) != MBUS_PROTO_VERSION) return;

    /*the port works well, then set uartPort*/
    uartPort = uartPortTemp;

    cmdType = ((mbus_hdr_mstr_t*)data)->cmd;
    querySeq = MBUS_GET_MASTER_SEQ(((mbus_hdr_mstr_t*)data)->frame_control);

    switch(cmdType)
    {
    case(MBUS_CMD_CLR):
    {
        fClearAndReset = TRUE;
        cardBufHead = 0;
        cardBufTail = 0;
        cardBufSendIndex = 0;
        sendSeqNum = 0;
        MBUS_send_type = CR_RESPOND;
        osal_start_timerEx(CR_TaskId, CR_UART_WRITE_EVENT, MBUS_FRAME_INTERVAL);
        break;
    }

    case(MBUS_CMD_QRY):
    {
        /*resend*/
        if(querySeq == sendSeqNum)
        {
            fSendIndexValid = FALSE;
        }
        /*request new data*/
        else
        {
            sendSeqNum = querySeq;
        }

        if(fSendIndexValid)
        {
            /*cardBufTail catches fSendIndexValid*/
            fSendIndexValid = FALSE;
            cardBufTail = cardBufSendIndex;
        }

        MBUS_send_type = CR_REPORT_DATA;
        osal_start_timerEx(CR_TaskId, CR_UART_WRITE_EVENT, MBUS_FRAME_INTERVAL);
        break;
    }

    case(MBUS_CMD_VERSION_QRY):
    {
        MBUS_send_type = CR_REPORT_VERSION;
        osal_start_timerEx(CR_TaskId, CR_UART_WRITE_EVENT, MBUS_FRAME_INTERVAL);
        break;
    }

    case(MBUS_CMD_RETREAT):
    {
        urgentCount = 0;
        urgentType = RETREAT;
        MBUS_send_type = CR_RESPOND;
        osal_start_timerEx(CR_TaskId, CR_UART_WRITE_EVENT, MBUS_FRAME_INTERVAL);
        osal_start_timerEx(CR_TaskId, CR_BACKTO_NORMAL_EVENT, 60000);
        break;
    }

    case(MBUS_CMD_CANCEL_RETREAT):
    {
        urgentCount = 0;
        urgentType = CANCELRETREAT;
        MBUS_send_type = CR_RESPOND;
        osal_start_timerEx(CR_TaskId, CR_UART_WRITE_EVENT, MBUS_FRAME_INTERVAL);
        osal_start_timerEx(CR_TaskId, CR_BACKTO_NORMAL_EVENT, 60000);
        break;
    }

    default:
        break;
    }

}

/**************************************************************************************************
**************************************************************************************************/

