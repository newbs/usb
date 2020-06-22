<#--
/*******************************************************************************
  USB Driver Freemarker Template File

  Company:
    Microchip Technology Inc.

  File Name:
    system_init_c_driver_data.ftl

  Summary:
    USB Driver Freemarker Template File

  Description:
    This file contains configurations necessary to run the system.  It
    implements USB driver initialize data. 
*******************************************************************************/

/*******************************************************************************
* Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries.
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
-->
/******************************************************
 * USB Driver Initialization
 ******************************************************/
 
<#if (USB_OPERATION_MODE == "Device") && (USB_DEVICE_VBUS_SENSE == true)>
/*  When designing a Self-powered USB Device, the application should make sure
    that USB_DEVICE_Attach() function is called only when VBUS is actively powered.
	Therefore, the firmware needs some means to detect when the Host is powering 
	the VBUS. A 5V tolerant I/O pin can be connected to VBUS (through a resistor)
	and can be used to detect when VBUS is high or low. The application can specify
	a VBUS Detect function through the USB Driver Initialize data structure. 
	The USB device stack will periodically call this function. If the VBUS is 
	detected, the USB_DEVICE_EVENT_POWER_DETECTED event is generated. If the VBUS 
	is removed (i.e., the device is physically detached from Host), the USB stack 
	will generate the event USB_DEVICE_EVENT_POWER_REMOVED. The application should 
	call USB_DEVICE_Detach() when VBUS is removed. 
    
    The following are the steps to generate the VBUS_SENSE Function through MHC     
        1) Navigate to MHC->Tools->Pin Configuration and Configure the pin used 
		   as VBUS_SENSE. Set this pin Function as "GPIO" and set as "Input". 
		   Provide a custom name to the pin.
        2) Select the USB Driver Component in MHC Project Graph and enable the  
		   "Enable VBUS Sense" Check-box.     
        3) Specify the custom name of the VBUS SENSE pin in the "VBUS SENSE Pin Name" box.  */ 
static DRV_USB_VBUS_LEVEL DRV_USBFSV1_VBUS_Comparator(void)
{
    DRV_USB_VBUS_LEVEL retVal = DRV_USB_VBUS_LEVEL_INVALID;
    
	<#if USB_DEVICE_VBUS_SENSE_PIN_NAME?has_content >
    if(true == ${USB_DEVICE_VBUS_SENSE_PIN_NAME}_Get())
    {
        retVal = DRV_USB_VBUS_LEVEL_VALID;
    }
	</#if>
	return (retVal);

}
</#if>

<#if (USB_OPERATION_MODE == "Host") && (USB_HOST_VBUS_ENABLE == true)>

void _DRV_USB_VBUSPowerEnable(uint8_t port, bool enable)
{
	/* Note: When operating in Host mode, the application can specify a Root 
	   hub port enable function. The USB Host Controller driver initialize data 
	   structure has a member for specifying the root hub enable function. 
	   This parameter should point to Root hub port enable function. If this 
	   parameter is NULL, it implies that the port is always enabled. 
	   
	   This function generated by MHC to let the user enable the root hub. 
	   User must use the MHC pin configuration utility to configure the pin 
	   used for enabling VBUS  */
	<#if USB_HOST_VBUS_ENABLE_PIN_NAME?has_content >	   
    if (enable == true)
	{
		/* Enable the VBUS */
		${USB_HOST_VBUS_ENABLE_PIN_NAME}_PowerEnable();
	}
	else
	{
		/* Disable the VBUS */
		${USB_HOST_VBUS_ENABLE_PIN_NAME}_PowerDisable();
	}
	</#if>
}
</#if>

const DRV_USBFSV1_INIT drvUSBInit =
{
	<#if (__PROCESSOR?matches("ATSAME5.*") == true) || (__PROCESSOR?matches("ATSAMD5.*") == true) >
	/* Interrupt Source for USB module */
	.interruptSource = USB_OTHER_IRQn,
 
	/* Interrupt Source for USB module */
	.interruptSource1 = USB_SOF_HSOF_IRQn,
 
	/* Interrupt Source for USB module */
	.interruptSource2 = USB_TRCPT0_IRQn,
 
	/* Interrupt Source for USB module */
	.interruptSource3 = USB_TRCPT1_IRQn,
<#elseif (__PROCESSOR?matches("ATSAMD2.*") == true) || (__PROCESSOR?matches("ATSAMDA.*") == true)>
    /* Interrupt Source for USB module */
    .interruptSource = USB_IRQn,
</#if>

    /* System module initialization */
    .moduleInit = {0},

<#if (USB_OPERATION_MODE == "Device")>
    /* USB Controller to operate as USB Device */
    .operationMode = DRV_USBFSV1_OPMODE_DEVICE,
<#elseif (USB_OPERATION_MODE == "Host")>
	/* USB Controller to operate as USB Host */
    .operationMode = DRV_USBFSV1_OPMODE_HOST,
</#if>

    /* USB Full Speed Operation */
	.operationSpeed = USB_SPEED_FULL,
    
    /* Stop in idle */
    .runInStandby = true,

    /* Suspend in sleep */
    .suspendInSleep = false,

    /* Identifies peripheral (PLIB-level) ID */
    .usbID = USB_REGS,
	
<#if (USB_OPERATION_MODE == "Device") && (USB_DEVICE_VBUS_SENSE == true)>    
    /* Function to check for VBus */
    .vbusComparator = DRV_USBFSV1_VBUS_Comparator
	
<#elseif (USB_OPERATION_MODE == "Host")>
	/* Function to check for VBus */
    .vbusComparator = NULL,
       
<#if (USB_HOST_VBUS_ENABLE == true)> 
    /* USB Host Power Enable. USB Driver uses this function to Enable the VBUS */ 
    .portPowerEnable = _DRV_USB_VBUSPowerEnable,
	
<#else>
	/* USB Host Power Enable */ 
    .portPowerEnable = NULL,
	
</#if>    
    /* Root hub available current in milliamperes */
    .rootHubAvailableCurrent = 500,
</#if>
};

<#--
/*******************************************************************************
 End of File
*/
-->

