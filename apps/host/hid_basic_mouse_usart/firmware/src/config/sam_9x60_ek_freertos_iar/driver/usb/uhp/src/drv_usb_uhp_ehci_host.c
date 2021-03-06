/*******************************************************************************
   USB EHCI Host Controller Driver

   Company:
    Microchip Technology Inc.

   File Name:
    drv_usb_uhp_ehci_host.c

   Summary:
    USB EHCI Host Driver Implementation.

   Description:
    This file implements the USB Host port EHCI driver implementation.
    This file should be included in the application if USB Host mode operation
    is desired.
*******************************************************************************/

//DOM-IGNORE-BEGIN
/*******************************************************************************
* Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/
//DOM-IGNORE-END

#include <stdint.h>
#include <string.h>
#include "configuration.h"
#include "definitions.h"
#include "driver/usb/drv_usb.h"
#include "driver/usb/uhp/src/drv_usb_uhp_local.h"
#include "driver/usb/uhp/src/drv_usb_uhp_ehci_host.h"


/**********************************************************
 * This structure is a set of pointer to the USB EHCI driver
 * functions. It is provided to the host and device layer
 * as the interface to the driver.
 * *******************************************************/

DRV_USB_HOST_INTERFACE gDrvUSBUHPHostInterfaceEhci =
{
    .open              = DRV_USB_UHP_Open,
    .close             = DRV_USB_UHP_Close,
    .eventHandlerSet   = DRV_USB_UHP_ClientEventCallBackSet,
    .hostIRPSubmit     = DRV_USB_UHP_EHCI_HOST_IRPSubmit,
    .hostIRPCancel     = DRV_USB_UHP_IRPCancel,
    .hostPipeSetup     = DRV_USB_UHP_EHCI_PipeSetup,
    .hostPipeClose     = DRV_USB_UHP_EHCI_PipeClose,
    .hostEventsDisable = DRV_USB_UHP_EventsDisable,
    .endpointToggleClear = DRV_USB_UHP_EndpointToggleClear,
    .hostEventsEnable  = DRV_USB_UHP_EventsEnable,
    .rootHubInterface.rootHubPortInterface.hubPortReset           = DRV_USB_UHP_ROOT_HUB_PortReset,
    .rootHubInterface.rootHubPortInterface.hubPortSpeedGet        = DRV_USB_UHP_ROOT_HUB_PortSpeedGet,
    .rootHubInterface.rootHubPortInterface.hubPortResetIsComplete = DRV_USB_UHP_ROOT_HUB_PortResetIsComplete,
    .rootHubInterface.rootHubPortInterface.hubPortSuspend         = DRV_USB_UHP_ROOT_HUB_PortSuspend,
    .rootHubInterface.rootHubPortInterface.hubPortResume          = DRV_USB_UHP_ROOT_HUB_PortResume,
    .rootHubInterface.rootHubMaxCurrentGet      = DRV_USB_UHP_ROOT_HUB_MaximumCurrentGet,
    .rootHubInterface.rootHubPortNumbersGet     = DRV_USB_UHP_ROOT_HUB_PortNumbersGet,
    .rootHubInterface.rootHubSpeedGet           = DRV_USB_UHP_ROOT_HUB_BusSpeedGet,
    .rootHubInterface.rootHubInitialize         = DRV_USB_UHP_ROOT_HUB_Initialize,
    .rootHubInterface.rootHubOperationEnable    = DRV_USB_UHP_EHCI_HOST_ROOT_HUB_OperationEnable,
    .rootHubInterface.rootHubOperationIsEnabled = DRV_USB_UHP_ROOT_HUB_OperationIsEnabled
};

typedef union __attribute__ ((packed))
{
    struct
    {
        volatile uint32_t PingState    : 1;
        volatile uint32_t SplitXstate  : 1;
        volatile uint32_t MissedMiFrame: 1;
        volatile uint32_t XactErr      : 1;
        volatile uint32_t BabbleDetect : 1;
        volatile uint32_t DataBuffErr  : 1;
        volatile uint32_t Halted       : 1;
        volatile uint32_t Active       : 1;
        volatile uint32_t PIDCode      : 2;
        volatile uint32_t ErrorCounter : 2;
        volatile uint32_t C_Page       : 3;
        volatile uint32_t IOC          : 1;
        volatile uint32_t TotalBytesTF :15;
        volatile uint32_t DataToggle   : 1;
    };
    volatile uint32_t qtdtoken;
} QTDToken;

typedef struct
{
    volatile uint32_t Next_qTD_Pointer;             /* DWord 0 */
    volatile uint32_t Alternate_Next_qTD_Pointer;   /* DWord 1 */
    volatile QTDToken qTD_Token;                    /* DWord 2 */
    volatile uint32_t qTD_Buffer_Pointer[5];        /* DWord 3 to 7 */
} EHCIQueueTDDescriptor;

typedef struct
{
    EHCIQueueTDDescriptor qTD;
    USB_HOST_IRP * irp;
    volatile bool inUse;
    uint8_t padding[24];
} DRV_USB_UHP_HOST_QTD_IRP_OBJ;

typedef union __attribute__ ((packed))
{
    struct
    {
        volatile uint32_t DeviceAdd  : 7;
        volatile uint32_t I          : 1;
        volatile uint32_t EndPt      : 4;
        volatile uint32_t EPS        : 2;
        volatile uint32_t dtc        : 1;
        volatile uint32_t H          : 1;
        volatile uint32_t MaxPLength :11;
        volatile uint32_t C          : 1;
        volatile uint32_t RL         : 4;
    };
    volatile uint32_t EptCharac;
} EHCIQueueHEptCharac;

typedef union __attribute__ ((packed))
{
    struct
    {
        volatile uint32_t uFrameSmask : 8;
        volatile uint32_t uFrameCmask : 8;
        volatile uint32_t Hubddr      : 7;
        volatile uint32_t PortNumber  : 7;
        volatile uint32_t Mult        : 2;
    };
    volatile uint32_t EptCapa;
} EHCIQueueHEptCapa;

typedef struct
{
    volatile uint32_t            Horizontal_Link_Pointer;  /* DWord 0 */
    volatile EHCIQueueHEptCharac Endpoint_Characteristics; /* DWord 1 */
    volatile EHCIQueueHEptCapa   Endpoint_Capabilities;    /* DWord 2 */
    volatile uint32_t            Current_qTD_Pointer;      /* DWord 3: processed by the host */
    volatile uint32_t            Next_qTD_Pointer;         /* DWord 4 */
    volatile uint32_t            Alt_Next_qTD_Pointer;     /* DWord 5 */
    volatile QTDToken            qTD_Token;                /* DWord 6 */
    volatile uint32_t            BufferPointer[5];         /* DWord 7 to 11 */
    volatile uint8_t             dummy[0xD0];              /* Aligned 208 */
} EHCIQueueHeadDescriptor;

#define DRV_USB_UHP_MAX_TRANSACTION   20
#define NOT_CACHED __attribute__((__section__(".region_nocache")))

__ALIGNED(32) NOT_CACHED EHCIQueueHeadDescriptor EHCI_QueueHead[DRV_USB_UHP_PIPES_NUMBER]; /* Queue Head: 0x30=48 length */
/* pQueueHeadCurrent is a pointer to a queue head that is already in the active list */
EHCIQueueHeadDescriptor* pQueueHeadCurrent;

__ALIGNED(64) NOT_CACHED DRV_USB_UHP_HOST_QTD_IRP_OBJ EHCI_IRP_QueueTD[DRV_USB_UHP_PIPES_NUMBER][DRV_USB_UHP_MAX_TRANSACTION];

__ALIGNED(32) NOT_CACHED EHCIQueueTDDescriptor * QueueTD;

__ALIGNED(4096) NOT_CACHED uint32_t PeriodicFrameList[1024];

static uint8_t OnlyOneTime = 0;

// ****************************************************************************
// ****************************************************************************
// Local Functions
// ****************************************************************************
// ****************************************************************************

// *****************************************************************************
/* Function:
    static void _DRV_USB_UHP_EHCI_HOST_CreateQTD( EHCIQueueTDDescriptor *qTD_base_addr,
                                EHCIQueueTDDescriptor *next_qTD_base_addr,
                                uint32_t PID,
                                uint32_t data_toggle,
                                uint32_t nb_bytes,
                                uint32_t int_on_complete,
                                uint32_t *buffer_base_addr )

   Summary:
    Create Transfer Descriptor

   Description:
    Fill Transfer Descriptor

   Remarks:
    Refer to .h for usage information.
 */
static void _DRV_USB_UHP_EHCI_HOST_CreateQTD( EHCIQueueTDDescriptor *qTD_base_addr,
                            EHCIQueueTDDescriptor *next_qTD_base_addr,
                            uint32_t PID,
                            uint32_t data_toggle,
                            uint32_t nb_bytes,
                            uint32_t int_on_complete,
                            uint32_t *buffer_base_addr )
{
    EHCIQueueTDDescriptor *qTD = (EHCIQueueTDDescriptor *)qTD_base_addr;

    /* 3.5.1 Next qTD Pointer */
    /* Table 3-14. qTD Next Element Transfer Pointer (DWord 0) */
    qTD->Next_qTD_Pointer = (uint32_t)next_qTD_base_addr |  /* Next qTD Pointer */
                            0;                              /* Terminate */

    /* 3.5.2 Alternate Next qTD Pointer */
    /* Table 3-15. qTD Alternate Next Element Transfer Pointer (DWord 1) */
    qTD->Alternate_Next_qTD_Pointer = 0x1; /* No Alternate, Terminate */

    /* 3.5.4 qTD Buffer Page Pointer List */
    /* Table 3-17. qTD Buffer Pointer(s) (DWords 3-7) */
    qTD->qTD_Buffer_Pointer[0] = (uint32_t)buffer_base_addr;

    if( ((uint32_t)buffer_base_addr + nb_bytes) >= (((uint32_t)buffer_base_addr & 0xFFFFF000) + 4096) )
    {
        qTD->qTD_Buffer_Pointer[1] = (((uint32_t)buffer_base_addr & 0xFFFFF000) + 4096);
    }
    else
    {
        qTD->qTD_Buffer_Pointer[1] = 0;
    }

    if( ((uint32_t)buffer_base_addr + nb_bytes) >= (((uint32_t)buffer_base_addr & 0xFFFFF000) + (4096*2)) )
    {
        qTD->qTD_Buffer_Pointer[2] = (((uint32_t)buffer_base_addr & 0xFFFFF000) + (4096*2));
    }
    else
    {
        qTD->qTD_Buffer_Pointer[2] = 0;
    }

    if( ((uint32_t)buffer_base_addr + nb_bytes) >= (((uint32_t)buffer_base_addr & 0xFFFFF000) + (4096*3)) )
    {
        qTD->qTD_Buffer_Pointer[3] = (((uint32_t)buffer_base_addr & 0xFFFFF000) + (4096*3));
    }
    else
    {
        qTD->qTD_Buffer_Pointer[3] = 0;
    }

    if( ((uint32_t)buffer_base_addr + nb_bytes) >= (((uint32_t)buffer_base_addr & 0xFFFFF000) + (4096*4)) )
    {
        qTD->qTD_Buffer_Pointer[4] = (((uint32_t)buffer_base_addr & 0xFFFFF000) + (4096*4));
    }
    else
    {
        qTD->qTD_Buffer_Pointer[4] = 0;
    }

    /* 3.5.3 qTD Token */
    /* Table 3-16. qTD Token (DWord 2) */
    qTD->qTD_Token.qtdtoken = (data_toggle << 31) |     /* Data Toggle */
                              (nb_bytes << 16) |        /* Total Bytes to Transfer */
                              (int_on_complete << 15) | /* Interrupt On Complete (IOC) */
                              (0 << 12) |               /* Current Page (C_Page) */
                              (3 << 10) |               /* Error Counter (CERR): Allow 3 retry */
                              (PID <<  8) |             /* PID Code (0: OUT, 1: IN 2: SETUP) */
                              (1 << 7);                 /* Status Field Description: Active. */
}


// ****************************************************************************
// ****************************************************************************
// External Functions
// ****************************************************************************
// ****************************************************************************

// *****************************************************************************
/* Function:
   void DRV_USB_UHP_EHCI_HOST_ResetEnable(DRV_USB_UHP_OBJ *hDriver)

   Summary:
    Reset the current port number

   Description:
    Reset the current port number to default value

   Remarks:
    Refer to .h for usage information.
 */
void DRV_USB_UHP_EHCI_HOST_ResetEnable(DRV_USB_UHP_OBJ *hDriver)
{
    volatile uhphs_registers_t *usbIDEHCI = hDriver->usbIDEHCI;

    usbIDEHCI->UHPHS_USBCMD |= UHPHS_USBCMD_RS_Msk; /* RUN */

    /* Test if the Host Controller is halted */
    if((usbIDEHCI->UHPHS_USBSTS & UHPHS_USBSTS_HCHLT_Msk) != 0)
    {
        SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\033[31m\n\rehci_send_reset error\033[0m");
    }

    /* Port Disabled */
    *((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + hDriver->portNumber) &= ~UHPHS_PORTSC_PED_Msk;

    /* Set HcInterruptDisable to have all interrupt disabled */
    hDriver->usbIDOHCI->UHP_OHCI_HCINTERRUPTDISABLE = UHP_OHCI_UHP_0HCI_HCINTERRUPTENABLE_SO   | /*     SchedulingOverrun */
                                                      UHP_OHCI_UHP_0HCI_HCINTERRUPTENABLE_WDH  | /*     WritebackDoneHead */
                                                      UHP_OHCI_UHP_0HCI_HCINTERRUPTENABLE_SF   | /*          StartofFrame */
                                                      UHP_OHCI_UHP_0HCI_HCINTERRUPTENABLE_RD   | /*        ResumeDetected */
                                                      UHP_OHCI_UHP_0HCI_HCINTERRUPTENABLE_UE   | /*    UnrecoverableError */
                                                      UHP_OHCI_UHP_0HCI_HCINTERRUPTENABLE_FNO  | /*   FrameNumberOverflow */
                                                      UHP_OHCI_UHP_0HCI_HCINTERRUPTENABLE_RHSC | /*   RootHubStatusChange */
                                                      UHP_OHCI_UHP_0HCI_HCINTERRUPTENABLE_OC   | /*       OwnershipChange */
                                                      UHP_OHCI_UHP_0HCI_HCINTERRUPTENABLE_MIE;   /* MasterInterruptEnable */

    /* Port routing control logic default-routes all ports to this host controller. */
    usbIDEHCI->UHPHS_CONFIGFLAG = UHPHS_CONFIGFLAG_CF_Msk;
}


// *****************************************************************************
/* Function:
    void DRV_USB_UHP_EHCI_HOST_ReceivedSize( uint32_t * BuffSize )

   Summary:
    Change the received size if needed

   Description:
    Change the received size if the requesting data number have not been sent

   Remarks:
    Refer to .h for usage information.
 */
void DRV_USB_UHP_EHCI_HOST_ReceivedSize( uint32_t * BuffSize )
{
    /* If Total Bytes to Transfer == 0, all data has been receive */
    if( EHCI_IRP_QueueTD[0][1].qTD.qTD_Token.TotalBytesTF != 0 )
    {
        *BuffSize -= EHCI_IRP_QueueTD[0][1].qTD.qTD_Token.TotalBytesTF;
    }
}


// *****************************************************************************
/* Function:
    void DRV_USB_UHP_EHCI_HOST_Init(DRV_USB_UHP_OBJ *hDriver)

   Summary:
    EHCI initialization

   Description:


   Remarks:
    Refer to .h for usage information.
 */
void DRV_USB_UHP_EHCI_HOST_Init(DRV_USB_UHP_OBJ *hDriver)
{
    volatile uhphs_registers_t *usbIDEHCI = hDriver->usbIDEHCI;
    uint32_t i;

    /* Initialize FrameList with a dummy queueHead */
    /* Create Queue Head for the command: */
    for( i=0; i<(sizeof(PeriodicFrameList)/4); i++ )
    {
        PeriodicFrameList[i] = 0x01;  /* T-Bit = 1: host controller will never use the value of the frame list pointer */
    }

    /* For the very first use of a queue head, software may zero-out the queue head transfer overlay, then set
     * the Next qTD Pointer field value to reference a valid qTD. */
    memset(EHCI_QueueHead, 0, sizeof(EHCI_QueueHead));
    memset(EHCI_IRP_QueueTD, 0, sizeof(EHCI_IRP_QueueTD));

    /* Host Controller Reset (HCRESET) */
    /* When software writes a one to this bit, the Host Controller resets its internal pipelines,
     * timers, counters, state machines, etc. to their initial value. Any transaction currently in
     * progress on USB is immediately terminated. A USB reset is not driven on downstream ports. */
    usbIDEHCI->UHPHS_USBCMD |= UHPHS_USBCMD_HCRESET_Msk;

    while( (usbIDEHCI->UHPHS_USBCMD & UHPHS_USBCMD_HCRESET_Msk) == UHPHS_USBCMD_HCRESET_Msk )
    {
    }

    /* Write the USBCMD register to set the desired interrupt threshold, frame list size (if applicable) */
    usbIDEHCI->UHPHS_USBCMD &= ~UHPHS_USBCMD_ITC_Msk;
    /* The default value is eight micro-frames.
     * This means that the host controller will not generate interrupts any more frequently than
     * once every eight micro-frames. */
    usbIDEHCI->UHPHS_USBCMD |= UHPHS_USBCMD_ITC(8);

    /* Frame List Size: 00b = 1024 elements (4096 bytes) */
    usbIDEHCI->UHPHS_USBCMD |= (((0x00) & UHPHS_USBCMD_FLS_Msk)<<UHPHS_USBCMD_FLS_Pos);
}


// *****************************************************************************
/* Function:
    DRV_USB_UHP_HOST_PIPE_HANDLE DRV_USB_UHP_EHCI_PipeSetup
    (
        DRV_HANDLE handle,
        uint8_t deviceAddress,
        USB_ENDPOINT endpointAndDirection,
        uint8_t hubAddress,
        uint8_t hubPort,
        USB_TRANSFER_TYPE pipeType,
        uint8_t bInterval,
        uint16_t wMaxPacketSize,
        USB_SPEED speed
    )

  Summary:
    Open a pipe with the specified attributes.

  Description:
    This function opens a communication pipe between the Host and the device
    endpoint. The transfer type and other attributes are specified through the
    function parameters. The driver does not check for available bus bandwidth,
    which should be done by the application (the USB Host Layer in this case)

  Remarks:
    See drv_xxx.h for usage information.
*/
DRV_USB_UHP_HOST_PIPE_HANDLE DRV_USB_UHP_EHCI_PipeSetup
(
    DRV_HANDLE        client,
    uint8_t           deviceAddress,
    USB_ENDPOINT      endpointAndDirection,
    uint8_t           hubAddress,
    uint8_t           hubPort,
    USB_TRANSFER_TYPE pipeType,
    uint8_t           bInterval,
    uint16_t          wMaxPacketSize,
    USB_SPEED         speed
)
{
    volatile uhphs_registers_t *usbIDEHCI;
    DRV_USB_UHP_OBJ * hDriver;
    DRV_USB_UHP_HOST_PIPE_OBJ * pipe = NULL;
    DRV_USB_UHP_HOST_PIPE_HANDLE pipeHandle = DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID;
    EHCIQueueHeadDescriptor* pQueueHeadNew;  /* pQueueHeadNew is a pointer to the queue head to be added */
    EHCIQueueTDDescriptor *qTD;
    uint8_t pipeIter = 0;

    if((client == DRV_HANDLE_INVALID) || (((DRV_USB_UHP_OBJ *)client) == NULL))
    {
        SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Invalid client handle");
    }
    else if(((DRV_USB_UHP_OBJ *)client)->inUse)
    {
        if((speed == USB_SPEED_LOW) || (speed == USB_SPEED_FULL) || (speed == USB_SPEED_HIGH))
        {
            /* wMaxPacketSize can be less than 8 for Non control endpoints. E.g.
             * Interrupt IN endpoints can be 4 bytes for HID Mouse device. This
             * USB module requires minimum 2 to the power 3 i.e. 8 bytes as the
             * wMaxPacketSize.
             *
             * For Control endpoints the minimum has to be 8 bytes.  */
            if(pipeType != USB_TRANSFER_TYPE_CONTROL)
            {
                if(wMaxPacketSize < 8)
                {
                    wMaxPacketSize = 8;
                }
            }
            if((wMaxPacketSize < 8) || (wMaxPacketSize > DRV_USB_UHP_NO_CACHE_BUFFER_LENGTH))
            {
                SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Invalid pipe endpoint size");
            }
            else
            {
                hDriver = (DRV_USB_UHP_OBJ *)client;
                /* There are two things that we need to check before we allocate
                 * a pipe. We check if have a free pipe object and check if we
                 * have a free endpoint that we can use */

                /* All control transfer pipe are grouped together as a linked
                 * list of pipes.  Non control transfer pipes are organized
                 * individually */

                /* We start by searching for a free pipe */
                if(OSAL_MUTEX_Lock(&hDriver->mutexID, OSAL_WAIT_FOREVER) != OSAL_RESULT_TRUE)
                {
                    SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Mutex lock failed");
                }
                else
                {
                    for(pipeIter = 0; pipeIter < USB_HOST_PIPES_NUMBER; pipeIter ++)
                    {
                        pipe = &gDrvUSBHostPipeObj[pipeIter];

                        if(false == pipe->inUse)
                        {
                            /* This means we have found a free pipe object */

                            /* Setup the pipe object */
                            pipe->inUse = true;
                            pipe->deviceAddress = deviceAddress;
                            pipe->irpQueueHead = NULL;
                            pipe->bInterval = bInterval;
                            pipe->speed = speed;
                            pipe->hubAddress = hubAddress;
                            pipe->hubPort = hubPort;
                            pipe->pipeType = pipeType;
                            pipe->hClient = client;
                            pipe->endpointSize = wMaxPacketSize;
                            pipe->intervalCounter = bInterval;
                            pipe->hostEndpoint = pipeIter;
                            pipe->endpointAndDirection = endpointAndDirection;

                            usbIDEHCI = hDriver->usbIDEHCI;

                            SYS_DEBUG_PRINT(SYS_ERROR_DEBUG, "\r\n\033[31mPSt = %d @%d \033[0m", pipeIter, pipe->deviceAddress);

                            /* 4.8.1 Adding Queue Heads to Asynchronous Schedule */
                            if( OnlyOneTime == 0 )
                            {
                                pQueueHeadCurrent = &EHCI_QueueHead[0];
                            }

                            pQueueHeadNew = &EHCI_QueueHead[pipeIter];

                            if( (EHCI_QueueHead[pipeIter].Horizontal_Link_Pointer == (uint32_t)&EHCI_QueueHead[pipeIter])
                               ||(EHCI_QueueHead[pipeIter].Horizontal_Link_Pointer == 0 ) )
                            {
                                pQueueHeadNew->Horizontal_Link_Pointer = pQueueHeadCurrent->Horizontal_Link_Pointer;
                                if( pQueueHeadCurrent->Horizontal_Link_Pointer == 0 )
                                {
                                    pQueueHeadNew->Horizontal_Link_Pointer = (uint32_t)&EHCI_QueueHead[0];
                                }
                            }

                            /* 4.10 Managing Control/Bulk/Interrupt Transfers via Queue Heads */
                            /* For the very first use of a queue head, software may zero-out the queue head transfer overlay,
                             * then set the Next qTD Pointer field value to reference a valid qTD. */

                            /* Queue Head Horizontal Link Pointer
                             * see: 3.6 Queue Head
                             * Table 3-18. Queue Head DWord 0 */
                            EHCI_QueueHead[pipeIter].Horizontal_Link_Pointer |= (1 << 1) | /* Typ: QH/(s)iTD/FSTN: Data structure type = 01b QH (queue head) */
                                                                                (0 << 0);  /* T: Terminate = 1 : last transfer */

                            if( pipe->pipeType == USB_TRANSFER_TYPE_CONTROL )
                            {
                                /* Table 3-19. Endpoint Characteristics: Queue Head DWord 1 */
                                EHCI_QueueHead[pipeIter].Endpoint_Characteristics.EptCharac =
                                    (0 << 28) |                      /* RL: Nak Count Reload */
                                    (0 << 27) |                      /* C: Control Endpoint Flag */
                                    (pipe->endpointSize << 16) |     /* Maximum Packet Length */
                                    /* This bit is set to mark the QH as the head of the asynchronous schedule.
                                     * This bit is cleared since this bit is not used for the periodic schedule. */
                                    (0 << 15) |                      /* H: Head of Reclamation List Flag */
                                    (1 << 14) |                      /* DTC: Data Toggle Control: 1: from qTD, 0: from QH (automatic) */
                                    (2 << 12) |                      /* EPS: Endpoint Speed = 10b High-Speed (480 Mb/s) */
                                    ((pipe->endpointAndDirection & 0xF) <<  8) | /* Endpt: Endpoint Number */
                                    (0 << 7) |                       /* I: Inactivate on Next Transaction (Periodic Schedule Full or Low-speed)*/
                                    (pipe->deviceAddress <<  0);     /* Device Address */
                            }
                            else
                            {
                                EHCI_QueueHead[pipeIter].Endpoint_Characteristics.EptCharac =
                                    (0 << 28) |                      /* RL: Nak Count Reload */
                                    (0 << 27) |                      /* C: Control Endpoint Flag */
                                    (pipe->endpointSize << 16) |     /* Maximum Packet Length */
                                    /* This bit is set to mark the QH as the head of the asynchronous schedule.
                                     * This bit is cleared since this bit is not used for the periodic schedule. */
                                    (0 << 15) |                      /* H: Head of Reclamation List Flag */
                                    (0 << 14) |                      /* DTC: Data Toggle Control: 1: from qTD, 0: from QH (automatic) */
                                    (2 << 12) |                      /* EPS: Endpoint Speed = 10b High-Speed (480 Mb/s) */
                                    ((pipe->endpointAndDirection & 0xF) <<  8) | /* Endpt: Endpoint Number */
                                    (0 << 7) |                       /* I: Inactivate on Next Transaction (Periodic Schedule Full or Low-speed)*/
                                    (pipe->deviceAddress <<  0);     /* Device Address */
                            }
                            if( pipeIter == 0 )
                            {
                                EHCI_QueueHead[pipeIter].Endpoint_Characteristics.EptCharac |=
                                (1 << 15);                      /* H: Head of Reclamation List Flag */
                            }

                            /* Table 3-20. Endpoint Capabilities: Queue Head DWord 2 */
                            EHCI_QueueHead[pipeIter].Endpoint_Capabilities.EptCapa =
                                (1 << 30) |                   /* Mult: High-Bandwidth Pipe Multiplier = 3 transactions to
                                                               * be issued for this endpoint per micro-frame */
                                (pipe->hubPort << 23) |       /* Port Number */
                                (pipe->hubAddress << 16) |    /* Hub Addr (full or low-speed device) */
                                (0 <<  8);                    /* �Frame C-Mask: Split Completion Mask (low- or full-speed device) */

                            /* then set the Next qTD Pointer field value to reference a valid qTD. */
                            qTD = &EHCI_IRP_QueueTD[pipeIter][0].qTD;
                            /* Don't touch DTC (|=) */
                            qTD->qTD_Token.qtdtoken |= (1 << 6) | /* Halted */
                                                       (3 <<10);  /* CErr set to 3 */
                            qTD->Alternate_Next_qTD_Pointer = 0x1;
                            qTD->Next_qTD_Pointer = 0;
                            qTD->qTD_Buffer_Pointer[0] = 0;
                            qTD->qTD_Buffer_Pointer[1] = 0;
                            qTD->qTD_Buffer_Pointer[2] = 0;
                            qTD->qTD_Buffer_Pointer[3] = 0;
                            qTD->qTD_Buffer_Pointer[4] = 0;

                            /* Link to the next one, in all case */
                            if( EHCI_QueueHead[pipeIter].Current_qTD_Pointer != 0 )
                            {
                                if(  (EHCI_QueueHead[pipeIter].Current_qTD_Pointer+0x40) >= (uint32_t)&EHCI_IRP_QueueTD[pipeIter][DRV_USB_UHP_MAX_TRANSACTION-1].qTD )
                                {
                                    EHCI_QueueHead[pipeIter].Next_qTD_Pointer = (uint32_t)&EHCI_IRP_QueueTD[pipeIter][0].qTD;
                                }
                                else
                                {
                                    EHCI_QueueHead[pipeIter].Next_qTD_Pointer = EHCI_QueueHead[pipeIter].Current_qTD_Pointer+0x40;
                                }
                            }
                            else
                            {
                                EHCI_QueueHead[pipeIter].Next_qTD_Pointer = (uint32_t)&EHCI_IRP_QueueTD[pipeIter][0].qTD;
                            }
                            /* Note that if the I-bit (Inactivate on Next Transaction) is a one and the Active bit (qTD Token (DWord 2)) is a zero,
                             * the host controller immediately skips processing of this queue head, exits this state and uses the horizontal
                             * pointer to the next schedule data structure. */

                            if( OnlyOneTime == 0 )
                            {
                                OnlyOneTime = 1;
                                /* Activation of the list */
                                /* Current Asynchronous List Address */
                                usbIDEHCI->UHPHS_ASYNCLISTADDR = (uint32_t)&EHCI_QueueHead[0];
                                /* Enable Asynchronous list */
                                usbIDEHCI->UHPHS_USBCMD |= UHPHS_USBCMD_ASE_Msk;
                            }

                            if((EHCI_QueueHead[pipeIter].qTD_Token.XactErr != 0) ||
                               (EHCI_QueueHead[pipeIter].qTD_Token.MissedMiFrame != 0) ||
                               (EHCI_QueueHead[pipeIter].qTD_Token.BabbleDetect != 0) ||
                               (EHCI_QueueHead[pipeIter].qTD_Token.DataBuffErr != 0))
                            {
                                EHCI_QueueHead[pipeIter].qTD_Token.qtdtoken = (1 << 6); /* Halted */
                                EHCI_QueueHead[pipeIter].qTD_Token.qtdtoken &= ~(1 << 6); /* Non Halted */
                            }

                            /* Finish the new chained list */
                            if( (pQueueHeadCurrent->Horizontal_Link_Pointer&0x7) == 0 )
                            {
                                pQueueHeadCurrent->Horizontal_Link_Pointer = (pQueueHeadCurrent->Horizontal_Link_Pointer & (0x6))
                                                                           | (uint32_t)&EHCI_QueueHead[pipeIter];
                            }

                            if( (uint32_t)pQueueHeadCurrent < (uint32_t)pQueueHeadNew )
                            {
                                pQueueHeadCurrent->Horizontal_Link_Pointer = (pQueueHeadCurrent->Horizontal_Link_Pointer & (0x6))
                                                                           | (uint32_t)&EHCI_QueueHead[pipeIter];
                                pQueueHeadCurrent = pQueueHeadNew;
                            }
                            if(OSAL_MUTEX_Unlock(&hDriver->mutexID) != OSAL_RESULT_TRUE)
                            {
                                SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP Driver: Mutex unlock failed in DRV USB_UHP_HOST_PipeSetup()");
                            }
                            else
                            {
                                pipeHandle = (DRV_USB_UHP_HOST_PIPE_HANDLE)pipe;
                                break;
                            }
                        }
                    }
                    if(pipeHandle == DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID)
                    {
                        /* This means that we could not find a free pipe object */
                        if(OSAL_MUTEX_Unlock(&hDriver->mutexID) != OSAL_RESULT_TRUE)
                        {
                            SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP Driver: Mutex unlock failed in DRV USB_UHP_HOST_PipeSetup()");
                        }
                    }
                }
            }
        }
    }
    else
    {
        SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP Driver: Invalid client in DRV USB_UHP_HOST_PipeSetup()");
    }
    return (pipeHandle);
}/* end of DRV_USB_UHP_EHCI_PipeSetup() */


// *****************************************************************************
/* Function:
     void DRV_USB_UHP_EHCI_PipeClose(DRV_USB_UHP_HOST_PIPE_HANDLE pipeHandle)

  Summary:
    Closes an open pipe.

  Description:
    This function closes an open pipe. Any IRPs scheduled on the pipe will be
    aborted and IRP callback functions will be called with the status as
    DRV_USB_HOST_IRP_STATE_ABORTED. The pipe handle will become invalid and the
    pipe will not accept IRPs.

  Remarks:
    See .h for usage information.
*/
void DRV_USB_UHP_EHCI_PipeClose
(
    DRV_USB_UHP_HOST_PIPE_HANDLE pipeHandle
)
{
    DRV_USB_UHP_OBJ * hDriver = NULL;
    USB_HOST_IRP_LOCAL  * irp = NULL;
    DRV_USB_UHP_HOST_PIPE_OBJ * pipe = NULL;
    EHCIQueueHeadDescriptor* pQueueHeadToUnlink; /* pQHeadToUnlink is a pointer to the queue head to be removed */
    EHCIQueueHeadDescriptor* pQueueHeadPrevious = NULL; /* pQHeadPrevious is a pointer to a queue head that references the queue head to remove */
    volatile uint32_t watchdog = 0;
    uint8_t pipeIter = 0;
    bool interruptWasEnabled = false;

    /* Make sure we have a valid pipe */
    if ((pipeHandle == 0) || (pipeHandle == DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID))
    {
        SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Invalid pipe handle");
    }
    else
    {
        pipe = (DRV_USB_UHP_HOST_PIPE_OBJ *)pipeHandle;

        /* Make sure that we are working with a pipe in use */
        if(pipe->inUse != true)
        {
            SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Pipe is not in use");
        }
        else
        {
            hDriver = (DRV_USB_UHP_OBJ *)pipe->hClient;
            /* Disable the interrupt */
            if(!hDriver->isInInterruptContext)
            {
                if(OSAL_MUTEX_Lock(&hDriver->mutexID, OSAL_WAIT_FOREVER) != OSAL_RESULT_TRUE)
                {
                    SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Mutex lock failed");
                }
                interruptWasEnabled = _DRV_USB_UHP_InterruptSourceDisable(hDriver->interruptSource);
            }

            pipe->inUse = false;

            /* Now we invoke the call back for each IRP in this pipe and say that it is
             * aborted.  If the IRP is in progress, then that IRP will be actually
             * aborted on the next SOF This will allow the USB module to complete any
             * transaction that was in progress. */

            irp = (USB_HOST_IRP_LOCAL *)pipe->irpQueueHead;
            while(irp != NULL)
            {
                irp->pipe = DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID;

                if(irp->status == USB_HOST_IRP_STATUS_IN_PROGRESS)
                {
                    /* If the IRP is in progress, then we set the temp IRP state. This
                     * will be caught in the DRV_USB_UHP_TransferProcess() function */

                    irp->status = USB_HOST_IRP_STATUS_ABORTED;

                    if(irp->callback != NULL)
                    {
                        irp->callback((USB_HOST_IRP*)irp);
                    }
                    irp->tempState = DRV_USB_UHP_HOST_IRP_STATE_ABORTED;
                }
                else
                {
                    /* IRP is pending */
                    irp->status = USB_HOST_IRP_STATUS_ABORTED;

                    if(irp->callback != NULL)
                    {
                        irp->callback((USB_HOST_IRP*)irp);
                    }
                }
                irp = irp->next;
            }

            /* Now we return the pipe back to the driver */
            pipe->inUse = false;

            /* 4.8.2 Removing Queue Heads from Asynchronous Schedule */

            /* Software should first deactivate all active qTDs, */
            for (pipeIter = 0; pipeIter < DRV_USB_UHP_MAX_TRANSACTION; pipeIter++)
            {
                EHCI_IRP_QueueTD[pipe->hostEndpoint][pipeIter].qTD.qTD_Token.qtdtoken |= (1 << 6);  /* Halted */
            }

            SYS_DEBUG_PRINT(SYS_ERROR_DEBUG, "\r\n\033[31mPCl = %d @%d\033[0m", pipe->hostEndpoint, pipe->deviceAddress );
            pQueueHeadToUnlink = &EHCI_QueueHead[pipe->hostEndpoint];

            /* wait for the queue head to go inactive */
            watchdog = 0;
            while((pQueueHeadToUnlink->qTD_Token.qtdtoken & (1<<7)) == (1<<7))
            {
                if(watchdog++>500)
                {
                    break;
                }
            }

            if((pipe->pipeType == USB_TRANSFER_TYPE_INTERRUPT)
               ||(pipe->pipeType == USB_TRANSFER_TYPE_BULK))
            {
                EHCI_QueueHead[pipe->hostEndpoint].qTD_Token.DataToggle = 0;
            }
            /* then remove the queue head from the asynchronous list. */
            if( ( 0 != pipe->hostEndpoint ) )
            {
                /* Find Previous QueuHead */
                for(pipeIter = 0; pipeIter < USB_HOST_PIPES_NUMBER; pipeIter++)
                {
                    if( (EHCI_QueueHead[pipeIter].Horizontal_Link_Pointer & 0xFFFFFFF8) == (uint32_t)pQueueHeadToUnlink )
                    {
                        pQueueHeadPrevious = &EHCI_QueueHead[pipeIter];
                        break;
                    }
                }
                if( pipeIter == USB_HOST_PIPES_NUMBER)
                {
                    SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: error pipe->hostEndpoint = %d ", pipe->hostEndpoint);
                    while(1);
                }
                pQueueHeadPrevious->Horizontal_Link_Pointer = pQueueHeadToUnlink->Horizontal_Link_Pointer;
                /* pQueueHeadToUnlink->Horizontal_Link_Pointer: Stay pointed to the next. */
                pQueueHeadCurrent = pQueueHeadPrevious;
            }
            else
            {
                pQueueHeadCurrent = &EHCI_QueueHead[0];
            }

            /* Enable interrupts */
            if(!hDriver->isInInterruptContext)
            {
                if(interruptWasEnabled)
                {
                    _DRV_USB_UHP_InterruptSourceEnable(hDriver->interruptSource);
                }

                if(OSAL_MUTEX_Unlock(&hDriver->mutexID) != OSAL_RESULT_TRUE)
                {
                    SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Mutex unlock failed");
                }
            }
        }
    }
}/* end of DRV_USB_UHP_EHCI_PipeClose() */


// *****************************************************************************
/* Function:
    void DRV_USB_UHP_EHCI_ClearDataToggle(DRV_USB_UHP_HOST_PIPE_OBJ *pipe);

  Summary:
    Clear the datatoggle for a pipe.

  Description:
    This function clear the datatoggle for a pipe.

  Remarks:
    See .h for usage information.
*/
void DRV_USB_UHP_EHCI_ClearDataToggle(DRV_USB_UHP_HOST_PIPE_OBJ *pipe)
{
    EHCI_QueueHead[pipe->hostEndpoint].qTD_Token.DataToggle = 0;
} /* end of DRV_USB_UHP_OHCI_ClearDataToggle() */


// *****************************************************************************
/* Function:
    USB_ERROR DRV_USB_UHP_EHCI_HOST_IRPSubmit
    (
        DRV_USB_UHP_HOST_PIPE_HANDLE  hPipe,
        USB_HOST_IRP * pinputIRP
    )

  Summary:
    Submits an IRP on a pipe.

  Description:
    This function submits an IRP on the specified pipe. The IRP will be added to
    the queue and will be processed in turn. The data will be transferred on the
    bus based on the USB bus scheduling rules. When the IRP has been processed,
    the callback function specified in the IRP will be called. The IRP status
    will be updated to reflect the completion status of the IRP.

  Remarks:
    See .h for usage information.
*/
USB_ERROR DRV_USB_UHP_EHCI_HOST_IRPSubmit
(
    DRV_USB_UHP_HOST_PIPE_HANDLE hPipe,
    USB_HOST_IRP *inputIRP
)
{
    USB_HOST_IRP_LOCAL *irpIterator;
    USB_HOST_IRP_LOCAL *irp  = (USB_HOST_IRP_LOCAL *)inputIRP;
    DRV_USB_UHP_HOST_PIPE_OBJ *pipe = (DRV_USB_UHP_HOST_PIPE_OBJ *)(hPipe);
    DRV_USB_UHP_OBJ *hDriver;
    uint8_t *point;
    volatile uint8_t idx = 0;
    volatile uint8_t idx_plus = 0;
    volatile uint8_t idx_eot = 0;
    volatile uhphs_registers_t *usbIDEHCI;
    volatile uint8_t DToggle = 0;
    USB_ERROR returnValue = USB_ERROR_PARAMETER_INVALID;
    volatile uint32_t i;

    if ((pipe == NULL) || (hPipe == (DRV_USB_HOST_PIPE_HANDLE_INVALID)))
    {
        /* This means an invalid pipe was specified.  Return with an error */
        SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Pipe handle is not valid");
    }
    else
    {
        hDriver = (DRV_USB_UHP_OBJ *)(pipe->hClient);
        usbIDEHCI = hDriver->usbIDEHCI;

        /* Assign owner pipe */
        irp->pipe = hPipe;
        irp->status = USB_HOST_IRP_STATUS_PENDING;
        irp->tempState = DRV_USB_UHP_HOST_IRP_STATE_PROCESSING;

        /* We need to disable interrupts was the queue state
         * does not change asynchronously */

        if(!hDriver->isInInterruptContext)
        {
            if(OSAL_MUTEX_Lock(&(hDriver->mutexID), OSAL_WAIT_FOREVER) != OSAL_RESULT_TRUE)
            {
                SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Mutex lock failed in DRV_USB_UHP_EHCI_HOST_IRPSubmit()");
                returnValue = USB_ERROR_OSAL_FUNCTION;
            }
            else
            {
                _DRV_USB_UHP_InterruptSourceDisable(hDriver->interruptSource);
            }
        }
        SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: IRPSubmit= %d", pipe->hostEndpoint);

        if(USB_ERROR_OSAL_FUNCTION != returnValue)
        {
            /* This needs to be done for all irp irrespective
             * of type or if there IRP is immediately processed */

            irp->next = NULL;
            irp->completedBytes = 0;
            irp->status = USB_HOST_IRP_STATUS_PENDING;

            if(pipe->irpQueueHead == NULL)
            {
                /* This means that there are no IRPs on this pipe. We can add
                 * this IRP directly */

                irp->previous = NULL;
                pipe->irpQueueHead = irp;

                EHCI_IRP_QueueTD[pipe->hostEndpoint][0].irp = inputIRP;

                /* Find qTD End of Tranfer */
                for (idx_eot = 0; idx_eot < DRV_USB_UHP_MAX_TRANSACTION; idx_eot++)
                {
                    if( (uint32_t)&EHCI_IRP_QueueTD[pipe->hostEndpoint][idx_eot].qTD == (uint32_t)(EHCI_QueueHead[pipe->hostEndpoint].Next_qTD_Pointer & 0xFFFFFFFE))
                    {
                        idx = idx_eot;
                        if( idx == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx = 0;
                        }
                        idx_plus = idx + 1;
                        if( idx_plus == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx_plus = 0;
                        }
                        break;
                    }
                }

                if(pipe->pipeType == USB_TRANSFER_TYPE_CONTROL)
                {
                    /* We need to update the flags parameter of the IRP
                     * to indicate the direction of the control transfer. */

                    /* SETUP */
                    /* Set the initial stage of the IRP */
                    irp->tempState = DRV_USB_UHP_HOST_IRP_STATE_PROCESSING;

                    /* We need to update the flags parameter of the IRP to
                     * indicate the direction of the control transfer. */

                    if(*((uint8_t*)(irp->setup)) & 0x80)
                    {
                        /* Device to Host: IN */
                        /* This means the data stage moves from device to host.
                         * Set bit 15 of the flags parameter */
                        irp->flags |= 0x80;
                    }
                    else
                    {
                        /* Host to Device: OUT */
                        /* This means the data stage moves from host to device.
                         * Clear bit 15 of the flags parameter. */
                        irp->flags &= 0x7F;
                    }

                    /* Send the setup packet to device */

                    /* SETUP Transaction */
                    if (*((uint8_t *)(irp->setup)) & 0x80)
                    {
                        /* SETUP IN: (1<<7) Device to host */
                        idx++;
                        if( idx == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx = 0;
                        }
                        idx_plus = idx + 1;
                        if( idx_plus == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx_plus = 0;
                        }

                        /* The host sends an IN packet to allow the device to send the descriptor. */
                        /* IN transaction */
                        /* Setup DATA IN packet */
                        _DRV_USB_UHP_EHCI_HOST_CreateQTD(&EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD, /* qTD address base */
                                        &EHCI_IRP_QueueTD[pipe->hostEndpoint][idx_plus].qTD, /* next qTD address base */
                                        1,                       /* PID: IN = 1 */
                                        (++DToggle)&0x01,        /* data toggle */
                                        irp->size,
                                        0,                       /* Interrupt on Complete */
                                        (uint32_t *)USBBufferNoCache[pipe->hostEndpoint]); /* Setup DATA IN packet */

                        irp->completedBytes += irp->size;

                        /* The host issues an OUT zero length packet (ZLP) to acknowledge reception of the descriptor. */
                        idx++;
                        if( idx == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx = 0;
                        }
                        idx_plus = idx + 1;
                        if( idx_plus == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx_plus = 0;
                        }

                        /* Setup STATUS OUT packet */
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].inUse = 1;
                        _DRV_USB_UHP_EHCI_HOST_CreateQTD(&EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD, /* qTD address base */
                                        &EHCI_IRP_QueueTD[pipe->hostEndpoint][idx_plus].qTD,               /* next qTD address base */
                                        0,                  /* PID: OUT = 0 */
                                        1,                  /* data toggle, STATUS is always DATA1 */
                                        0,                  /* Total Bytes to transfer: ZLP */
                                        1,                  /* Interrupt on Complete */
                                        NULL);              /* data buffer address base, 32-Byte align */
                    }
                    else
                    {
                        /* SETUP OUT: (1<<7) Host to Device */

                        if (irp->size != 0)
                        {
                            /* OUT Packet */
                            idx++;
                            if( idx == DRV_USB_UHP_MAX_TRANSACTION)
                            {
                                idx = 0;
                            }
                            idx_plus = idx + 1;
                            if( idx_plus == DRV_USB_UHP_MAX_TRANSACTION)
                            {
                                idx_plus = 0;
                            }

                            point = (uint8_t *)irp->data;
                            for (i = 0; i < irp->size; i++)
                            {
                                USBBufferNoCache[pipe->hostEndpoint][i] = point[i];
                            }

                            /* Setup DATA OUT Packet */
                            _DRV_USB_UHP_EHCI_HOST_CreateQTD(&EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD, /* qTD address base */
                                            &EHCI_IRP_QueueTD[pipe->hostEndpoint][idx_plus].qTD, /* next qTD address base */
                                            0,                       /* PID: OUT = 0 */
                                            1,                       /* data toggle */
                                            irp->size,               /* Total Bytes to transfer */
                                            0,                       /* Interrupt on Complete */
                                            (uint32_t *)USBBufferNoCache[pipe->hostEndpoint]); /* Setup DATA OUT Packet */
                            irp->completedBytes += irp->size;
                        }

                        idx++;
                        if( idx == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx = 0;
                        }
                        idx_plus = idx + 1;
                        if( idx_plus == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx_plus = 0;
                        }

                        /* Setup STATUS IN Packet (ZLP) */
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].inUse = 1;
                        _DRV_USB_UHP_EHCI_HOST_CreateQTD(&EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD, /* qTD address base */
                                        &EHCI_IRP_QueueTD[pipe->hostEndpoint][idx_plus].qTD, /* next qTD address base */
                                        1,                  /* PID: IN = 1 */
                                        1,                  /* data toggle */
                                        0,                  /* Total Bytes to transfer: ZLP */
                                        1,                  /* Interrupt on Complete */
                                        NULL);
                    }

                    /* Create new dummy qTD */
                    idx++;
                    if( idx == DRV_USB_UHP_MAX_TRANSACTION)
                    {
                        idx = 0;
                    }
                    EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.Alternate_Next_qTD_Pointer = 0;
                    EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Token.qtdtoken = (1 << 6) | /* Halted */
                                                                               (3 <<10);  /* CErr set to 3 */
                    EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[0] = 0;
                    EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[1] = 0;
                    EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[2] = 0;
                    EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[3] = 0;
                    EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[4] = 0;

                     /* The size of the data buffer  should not be larger than MaximumPacketSize
                     * from the Endpoint Descriptor (this is not checked by the Host Controller and
                     * transmission problems occur if software violates this restriction). */

                    /* Then overwrite the old dummy QTD: */
                    idx = idx_eot;
                    if( idx == DRV_USB_UHP_MAX_TRANSACTION)
                    {
                        idx = 0;
                    }
                    idx_plus = idx + 1;
                    if( idx_plus == DRV_USB_UHP_MAX_TRANSACTION)
                    {
                        idx_plus = 0;
                    }

                    point = (uint8_t *)irp->setup;
                    for (i = 0; i < 8; i++)
                    {
                        USBBufferNoCacheSetup[pipe->hostEndpoint][i] = point[i];
                    }

                    /* SETUP packet PID */
                    _DRV_USB_UHP_EHCI_HOST_CreateQTD(&EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD, /* qTD address base */
                                    &EHCI_IRP_QueueTD[pipe->hostEndpoint][idx_plus].qTD,  /* next qTD address base */
                                    2,                        /* PID: SETUP = 2 */
                                    0,                        /* data toggle */
                                    8,                        /* Total Bytes to transfer */
                                    0,                        /* Interrupt on Complete */
                                    (uint32_t *)USBBufferNoCacheSetup[pipe->hostEndpoint]);  /* SETUP packet PID */
                }
                else
                {
                    /* For non control transfers, if this is the first
                     * irp in the queue, then we can start the irp */

                    irp->tempState = DRV_USB_UHP_HOST_IRP_STATE_PROCESSING;

                    if( (pipe->endpointAndDirection & 0x80) == 0 )
                    {
                        /* Host to Device: OUT */
                        idx++;
                        if( idx == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx = 0;
                        }

                        /* Create new dummy qTD */
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.Alternate_Next_qTD_Pointer = 0;
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Token.qtdtoken = (1 << 6) | /* Halted */
                                                                                   (3 <<10);  /* CErr set to 3 */
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[0] = 0;
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[1] = 0;
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[2] = 0;
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[3] = 0;
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[4] = 0;

                        /* Then overwrite the old dummy QTD: */
                        idx = idx_eot;
                        if( idx == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx = 0;
                        }
                        idx_plus = idx + 1;
                        if( idx_plus == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx_plus = 0;
                        }

                        point = (uint8_t *)irp->data;
                        for (i = 0; i < irp->size; i++)
                        {
                            USBBufferNoCache[pipe->hostEndpoint][i] = point[i];
                        }

                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].inUse = 1;
                        _DRV_USB_UHP_EHCI_HOST_CreateQTD(&EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD, /* qTD address base */
                                        &EHCI_IRP_QueueTD[pipe->hostEndpoint][idx_plus].qTD, /* next qTD address base */
                                        0,                             /* PID: OUT = 0 */
                                        0x0,                           /* data toggle */
                                        // Ignore DT bit from incoming qTD. Host controller preserves DT bit in the queue head.
                                        irp->size,                     /* Total Bytes to transfer */
                                        1,                             /* Interrupt on Complete */
                                        (uint32_t *)USBBufferNoCache[pipe->hostEndpoint]);
                        irp->completedBytes += irp->size;

                    }
                    else
                    {
                        /* Device to Host: IN */
                        idx++;
                        if( idx == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx = 0;
                        }

                        /* Create new dummy qTD */
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.Alternate_Next_qTD_Pointer = 0;
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Token.qtdtoken = (1 << 6) | /* Halted */
                                                                                           (3 <<10);  /* CErr set to 3 */
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[0] = 0;
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[1] = 0;
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[2] = 0;
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[3] = 0;
                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD.qTD_Buffer_Pointer[4] = 0;

                        /* Then overwrite the old dummy QTD: */
                        idx = idx_eot;
                        if( idx == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx = 0;
                        }
                        idx_plus = idx + 1;
                        if( idx_plus == DRV_USB_UHP_MAX_TRANSACTION)
                        {
                            idx_plus = 0;
                        }

                        irp->completedBytes += irp->size;

                        EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].inUse = 1;
                        _DRV_USB_UHP_EHCI_HOST_CreateQTD(&EHCI_IRP_QueueTD[pipe->hostEndpoint][idx].qTD, /* qTD address base */
                                                &EHCI_IRP_QueueTD[pipe->hostEndpoint][idx_plus].qTD, /* next qTD address base */
                                                1,                            /* PID: IN = 1 */
                                                0x0,                          /* data toggle */
                                                // Ignore DT bit from incoming qTD. Host controller preserves DT bit in the queue head.
                                                irp->size,                    /* Total Bytes to transfer */
                                                1,                            /* Interrupt on Complete */
                                        (uint32_t *)USBBufferNoCache[pipe->hostEndpoint]);
                    }
                }
                /* In case of previous error on this pipe */
                EHCI_QueueHead[pipe->hostEndpoint].qTD_Token.Halted = 0;

                __DSB();
                __DMB();
                __ISB();

                /* Enable interrupts */
                usbIDEHCI->UHPHS_USBINTR = UHPHS_USBINTR_USBIE_Msk  /* (UHPHS_USBINTR) USB Interrupt Enable */
                                         | UHPHS_USBINTR_USBEIE_Msk /* (UHPHS_USBINTR) USB Error Interrupt Enable */
                                         | UHPHS_USBINTR_PCIE_Msk   /* (UHPHS_USBINTR) Port Change Interrupt Enable */
                                      /* | UHPHS_USBINTR_FLRE          (UHPHS_USBINTR) Frame List Rollover Enable */
                                         | UHPHS_USBINTR_HSEE_Msk   /* (UHPHS_USBINTR) Host System Error Enable */
                                         | UHPHS_USBINTR_IAAE_Msk;  /* (UHPHS_USBINTR) Interrupt on Async Advance Enable */

                irp->status = USB_HOST_IRP_STATUS_IN_PROGRESS;
                returnValue = USB_ERROR_NONE;
            }
            else
            {
                /* We need to add the irp to the last irp
                 * in the pipe queue (which is a linked list) */
                irpIterator = pipe->irpQueueHead;

                /* Find the last IRP in the linked list*/
                while(irpIterator->next != 0)
                {
                    irpIterator = irpIterator->next;
                }

                /* Add the item to the last irp in the linked list */
                irpIterator->next = irp;
                irp->previous = irpIterator;
            }

            if(!hDriver->isInInterruptContext)
            {
                _DRV_USB_UHP_InterruptSourceEnable(hDriver->interruptSource);

                if(OSAL_MUTEX_Unlock(&hDriver->mutexID) != OSAL_RESULT_TRUE)
                {
                    SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Mutex unlock failed");
                }
            }
            returnValue = USB_ERROR_NONE;
        }
    }
    return (returnValue);
}/* end of DRV_USB_UHP_EHCI_HOST_IRPSubmit() */


// *****************************************************************************
/* Function:
    void DRV_USB_UHP_EHCI_HOST_DisableAsynchronousList(DRV_USB_UHP_OBJ *hDriver)

  Summary:
    Disable the processing of the Asynchronous list

  Description:
    Disable the processing of the Asynchronous list

  Remarks:
    See drv_xxx.h for usage information.
*/
void DRV_USB_UHP_EHCI_HOST_DisableAsynchronousList(DRV_USB_UHP_OBJ *hDriver)
{
    volatile uhphs_registers_t *usbIDEHCI;

    usbIDEHCI = hDriver->usbIDEHCI;

    /* Disable Asynchronous list */
    usbIDEHCI->UHPHS_USBCMD &= ~UHPHS_USBCMD_ASE_Msk;
}

// *****************************************************************************
/* Function:
    void DRV_USB_UHP_EHCI_HOST_Tasks_ISR(DRV_USB_UHP_OBJ *hDriver)

  Summary:
    Interrupt handler

  Description:
    Management of all EHCI interrupt

  Remarks:
    See drv_xxx.h for usage information.
*/
void DRV_USB_UHP_EHCI_HOST_Tasks_ISR(DRV_USB_UHP_OBJ *hDriver)
{
    volatile uhphs_registers_t *usbIDEHCI = hDriver->usbIDEHCI;
    uint32_t isr_read_data;
    uint32_t read_data;
    uint32_t i, j;

    /* EHCI interrupts */
    isr_read_data = usbIDEHCI->UHPHS_USBINTR & usbIDEHCI->UHPHS_USBSTS & 0x3F;

    if (isr_read_data != 0)
    {
        /* Port Change Detect */
        if ((isr_read_data & UHPHS_USBSTS_PCD_Msk) == UHPHS_USBSTS_PCD_Msk)
        {
            usbIDEHCI->UHPHS_USBSTS = UHPHS_USBSTS_PCD_Msk; /* clear by writing "1" = Port Change Detect */

            SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\n\rEHCI port change interrupt");

            for (i = 0; i < (hDriver->usbIDOHCI->UHP_OHCI_HCRHDESCRIPTORA & UHP_OHCI_HCRHDESCRIPTORA_NDP_Msk); i++)
            {
                /* read_data = usbIDEHCI->UHPHS_PORTSC; */
                read_data = *((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + i);
                /* Over-current Change */
                if ((read_data & UHPHS_PORTSC_OCC_Msk) == UHPHS_PORTSC_OCC_Msk)
                {
                    SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\n\rOver-current Change on port %d", (int)i);
                }
                /* Port Enable/Disable Change */
                if ((read_data & UHPHS_PORTSC_PEDC_Msk) == UHPHS_PORTSC_PEDC_Msk)
                {
                    SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\n\rPort Enable/Disable Change on port %d", (int)i);
                }
                /* Connect Status Change */
                if ((read_data & UHPHS_PORTSC_CSC_Msk) == UHPHS_PORTSC_CSC_Msk)
                {
                    /* 1=Device is present on port. */
                    if (((*((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + i)) & UHPHS_PORTSC_CCS_Msk) == UHPHS_PORTSC_CCS_Msk)
                    {
                        /* New connection */
                        hDriver->deviceAttached = true;
                        SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\n\rConnect port %d", (int)i);
                        hDriver->portNumber = i;
                        break;
                    }
                    else
                    {
                        /* Disconnect */
                        SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\n\rEHCI IRQ : Device Disconnected: %d", (int)i);

                        /* Clear all it */
                        usbIDEHCI->UHPHS_USBSTS = usbIDEHCI->UHPHS_USBSTS;

                        /* Enable only port change interrupt */
                        usbIDEHCI->UHPHS_USBINTR = UHPHS_USBINTR_PCIE_Msk;   /* (UHPHS_USBINTR) Port Change Interrupt Enable */

                        hDriver->portNumber = 0;
                        /* We go a detach interrupt. The detach interrupt could have occurred
                         * while the attach de-bouncing is in progress. We just set a flag saying
                         * the device is detached; */
                        hDriver->deviceAttached = false;
                        if (hDriver->attachedDeviceObjHandle != USB_HOST_DEVICE_OBJ_HANDLE_INVALID)
                        {
                            /* Ask the host layer to de-enumerate this device. The de-enumeration
                             * must be done in the interrupt context. */
                            USB_HOST_DeviceDenumerate(hDriver->attachedDeviceObjHandle);
                        }
                        hDriver->attachedDeviceObjHandle = USB_HOST_DEVICE_OBJ_HANDLE_INVALID;
                        for(j=0; j<DRV_USB_UHP_PIPES_NUMBER; j++)
                        {
                            gDrvUSBHostPipeObj[j].staticDToggleIn = 0;  /* data toggle */
                            gDrvUSBHostPipeObj[j].staticDToggleOut = 0;  /* data toggle */
                        }
                    }
                }
            }
        }

        /* USB error */
        if ((isr_read_data & UHPHS_USBSTS_USBERRINT_Msk) == UHPHS_USBSTS_USBERRINT_Msk)
        {
            SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\033[31m\n\rEHCI IRQ : USB error interrupt\033[0m");

            for(i=0; i<DRV_USB_UHP_PIPES_NUMBER; i++)
            {
                if((EHCI_QueueHead[i].qTD_Token.XactErr != 0) ||
                   (EHCI_QueueHead[i].qTD_Token.MissedMiFrame != 0) ||
                   (EHCI_QueueHead[i].qTD_Token.BabbleDetect != 0) ||
                   (EHCI_QueueHead[i].qTD_Token.DataBuffErr != 0) ||
                   (EHCI_QueueHead[i].qTD_Token.Halted == 1))
                {
                    EHCI_QueueHead[i].qTD_Token.qtdtoken = (1 << 6); /* Halted */

                    /* Stall */
                    gDrvUSBHostPipeObj[i].intXfrQtdComplete = 0xFF;
                    DRV_USB_UHP_TransferProcess(hDriver, i);
                }
            }
            /* Clear It */
            usbIDEHCI->UHPHS_USBSTS = UHPHS_USBSTS_USBERRINT_Msk;

            return;
        }

        /* Host system error */
        if ((isr_read_data & UHPHS_USBSTS_HSE_Msk) == UHPHS_USBSTS_HSE_Msk)
        {
            SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\033[31m\n\rEHCI Host system error interrupt\033[0m");

            for(i=0; i<DRV_USB_UHP_PIPES_NUMBER; i++)
            {
                if((EHCI_QueueHead[i].qTD_Token.XactErr != 0) ||
                   (EHCI_QueueHead[i].qTD_Token.MissedMiFrame != 0) ||
                   (EHCI_QueueHead[i].qTD_Token.BabbleDetect != 0) ||
                   (EHCI_QueueHead[i].qTD_Token.DataBuffErr != 0))
                {
                    EHCI_QueueHead[i].qTD_Token.qtdtoken = 0;
                }
            }

            /* clear it */
            usbIDEHCI->UHPHS_USBSTS = UHPHS_USBSTS_HSE_Msk;
            /* - The Run/Stop bit in the USBCMD register is set to a zero.
             * - The following bits in the USBSTS register are set:
             *    - Host System Error bit is to a one.
             *    - HCHalted bit is set to a one. */
            /* After a Host System Error, Software must reset the host controller via
             * HCReset in the USBCMD register before re-initializing and restarting the host controller. */

            return;
        }

        /* Interrupt on Async Advance */
        if ((isr_read_data & UHPHS_USBSTS_IAA_Msk) == UHPHS_USBSTS_IAA_Msk)
        {
            SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\n\rEHCI interrupt on async advance");
            usbIDEHCI->UHPHS_USBSTS = UHPHS_USBSTS_IAA_Msk;
        }

        /* Frame list Rollover */
        if ((isr_read_data & UHPHS_USBSTS_FLR_Msk) == UHPHS_USBSTS_FLR_Msk)
        {
            SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\033[31m\n\rEHCI Frame list Rollover interrupt\033[0m");
            usbIDEHCI->UHPHS_USBSTS = UHPHS_USBSTS_FLR_Msk;
            /* After a Host System Error, Software must reset the host controller via HCReset
             * in the USBCMD register before re-initializing and restarting the host controller. */
        }

        /* USB Interrupt (USBINT) R/WC */
        /* The Host Controller sets this bit to 1 on the completion of a USB transaction, which results in the retirement of a Transfer Descriptor */
        /* that had its IOC bit set. */
        /* The Host Controller also sets this bit to 1 when a short packet is detected (actual */
        /* number of bytes received was less than the expected number of bytes). */
        if ((isr_read_data & UHPHS_USBSTS_USBINT_Msk) == UHPHS_USBSTS_USBINT_Msk)
        {
            for(i=0; i<DRV_USB_UHP_PIPES_NUMBER; i++)
            {
                for(j=0; j<DRV_USB_UHP_MAX_TRANSACTION; j++)
                {
                    if( EHCI_IRP_QueueTD[i][j].qTD.qTD_Token.Active == 0 )
                    {
                        if( EHCI_IRP_QueueTD[i][j].inUse == 1 )
                        {
                            gDrvUSBHostPipeObj[i].intXfrQtdComplete = 1;
                            DRV_USB_UHP_TransferProcess(hDriver, i);
                            EHCI_IRP_QueueTD[i][j].inUse = 0;
                        }
                    }
                }
            }
            usbIDEHCI->UHPHS_USBSTS = UHPHS_USBSTS_USBINT_Msk; /* clear by writing "1" */
        }
    }
}/* end of DRV_USB_UHP_EHCI_HOST_Tasks_ISR() */


/* ***************************************************************************** */
/* ***************************************************************************** */
/* Root Hub Driver Routines */
/* ***************************************************************************** */
/* ***************************************************************************** */

/* **************************************************************************** */
/* Function:
    void DRV_USB_UHP_EHCI_HOST_ROOT_HUB_OperationEnable(DRV_HANDLE handle, bool enable)

   Summary:
    Root hub enable

   Description:


   Remarks:
    Refer to .h for usage information.
 */
void DRV_USB_UHP_EHCI_HOST_ROOT_HUB_OperationEnable(DRV_HANDLE handle, bool enable)
{
    DRV_USB_UHP_OBJ *pUSBDrvObj = (DRV_USB_UHP_OBJ *)handle;
    volatile uhphs_registers_t *usbIDEHCI = pUSBDrvObj->usbIDEHCI;
    uint32_t loop1 = 0;

    /* Check if the handle is valid or opened */
    if((handle == DRV_HANDLE_INVALID) || (!(pUSBDrvObj->isOpened)))
    {
        SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else
    {
        if(false == enable)
        {
            /* If the root hub operation is disable, we disable detach and
             * attached event and switch off the port power. */
            /* Disable interrupt generation */
            usbIDEHCI->UHPHS_USBINTR &= ~UHPHS_USBINTR_USBIE_Msk;
            pUSBDrvObj->operationEnabled = false;

        }
        else
        {
            /* The USB Global interrupt and USB module is already enabled at
             * this point. We enable the attach interrupt to detect attach
             */
            pUSBDrvObj->operationEnabled = true;

            /* Initialize portsc registers */
            SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\n\rUHPHS Ports ");
            /* PPC: Port Power Control */
            if (((usbIDEHCI->UHPHS_HCCPARAMS & UHPHS_HCSPARAMS_PPC_Msk) == UHPHS_HCSPARAMS_PPC_Msk))
            {
                /* Put Port Power on (bit 12) : Host controller has port power control switches
                 * put Wake-up on (bit 22, 21, 20) */
                SYS_DEBUG_PRINT(SYS_ERROR_INFO, "have ");
                for (loop1 = 0; loop1 < (usbIDEHCI->UHPHS_HCSPARAMS&0xF); loop1++)
                {
                    *((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + loop1)
                        = UHPHS_PORTSC_PP_Msk |          /* Host controller has port power control switches. */
                          UHPHS_PORTSC_WKOC_E_Msk |      /* enables the port to be sensitive to over-current conditions as wake-up events. */
                          UHPHS_PORTSC_WKDSCNNT_E_Msk |  /* enables the port to be sensitive to device disconnects as wake-up events */
                          UHPHS_PORTSC_WKCNNT_E_Msk;     /* enables the port to be sensitive to device connects as wake-up events */
                }
            }
            else
            {
                /* Host controller does not have port power control switches. Each port is hard-wired to power. */
                SYS_DEBUG_PRINT(SYS_ERROR_INFO, "do not have ");
            }
            SYS_DEBUG_PRINT(SYS_ERROR_INFO, "port power switches");

            /* Turn the host controller ON via setting the Run/Stop bit. */
            if((usbIDEHCI->UHPHS_USBSTS & UHPHS_USBSTS_HCHLT_Msk) == UHPHS_USBSTS_HCHLT_Msk)
            {
                usbIDEHCI->UHPHS_USBCMD |= UHPHS_USBCMD_RS_Msk; /* RUN */
            }
            else
            {
                SYS_DEBUG_PRINT(SYS_ERROR_INFO, "\033[31m\n\rError during initialization\033[0m");
            }

            /* Port routing control logic default-routes all ports to this host controller. */
            usbIDEHCI->UHPHS_CONFIGFLAG = UHPHS_CONFIGFLAG_CF_Msk;

            /* Enable Device Connection Interrupt */
            /* Enable the attach and detach interrupt and EP0 interrupt. */
            usbIDEHCI->UHPHS_USBINTR = UHPHS_USBINTR_USBIE_Msk  /* (UHPHS_USBINTR) USB Interrupt Enable */
                                     | UHPHS_USBINTR_USBEIE_Msk /* (UHPHS_USBINTR) USB Error Interrupt Enable */
                                     | UHPHS_USBINTR_PCIE_Msk   /* (UHPHS_USBINTR) Port Change Interrupt Enable */
                                  /* | UHPHS_USBINTR_FLRE_Msk      (UHPHS_USBINTR) Frame List Rollover Enable */
                                     | UHPHS_USBINTR_HSEE_Msk   /* (UHPHS_USBINTR) Host System Error Enable */
                                     | UHPHS_USBINTR_IAAE_Msk;  /* (UHPHS_USBINTR) Interrupt on Async Advance Enable */

            /* Enable VBUS */
            if(pUSBDrvObj->rootHubInfo.portPowerEnable != NULL)
            {
                /* This USB module has only one port. So we call this function
                 * once to enable the port power on port 0 */
                pUSBDrvObj->rootHubInfo.portPowerEnable(0 /* Port 0 */, true);
            }
        }
    }
} /* end of DRV_USB_UHP_EHCI_HOST_ROOT_HUB_OperationEnable() */

