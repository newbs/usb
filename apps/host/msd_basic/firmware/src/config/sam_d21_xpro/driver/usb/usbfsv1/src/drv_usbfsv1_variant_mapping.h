/*******************************************************************************
  USB Driver Feature Variant Implementations

  Company:
    Microchip Technology Inc.

  File Name:
    drv_USBFSV1_variant_mapping.h

  Summary:
    USB Driver Feature Variant Implementations

  Description:
    This file implements the functions which differ based on different parts
    and various implementations of the same feature.
*******************************************************************************/

//DOM-IGNORE-BEGIN
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
//DOM-IGNORE-END

#ifndef _DRV_USBFSV1_VARIANT_MAPPING_H
#define _DRV_USBFSV1_VARIANT_MAPPING_H


// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************


#include "configuration.h"

/**********************************************
 * Macro Mapping
 **********************************************/

/* With v1.04 the USB Driver implementation has been been split such
 * multiple USB Driver can be included in the same application. But to
 * continue support for application developed before v1.04, we should
 * map the DRV_USB configuration macros to DRV_USBFSV1 macros */

#if defined(DRV_USB_INSTANCES_NUMBER)
    #define DRV_USBFSV1_INSTANCES_NUMBER  DRV_USB_INSTANCES_NUMBER
#endif

#if defined(DRV_USB_DEVICE_SUPPORT)
    #define DRV_USBFSV1_DEVICE_SUPPORT  DRV_USB_DEVICE_SUPPORT
#endif

#if defined(DRV_USB_HOST_SUPPORT)
    #define DRV_USBFSV1_HOST_SUPPORT  DRV_USB_HOST_SUPPORT
#endif

#if defined(DRV_USB_ENDPOINTS_NUMBER)
    #define DRV_USBFSV1_ENDPOINTS_NUMBER  DRV_USB_ENDPOINTS_NUMBER
#endif

/**********************************************
 * Sets up driver mode-specific init routine
 * based on selected support.
 *********************************************/

#ifndef DRV_USBFSV1_DEVICE_SUPPORT
    #error "DRV_USBFSV1_DEVICE_SUPPORT must be defined and be either true or false"
#endif

#ifndef DRV_USBFSV1_HOST_SUPPORT
    #error "DRV_USBFSV1_HOST_SUPPORT must be defined and be either true or false"
#endif

#if (DRV_USBFSV1_DEVICE_SUPPORT == true)
    #define _DRV_USBFSV1_DEVICE_INIT(x, y)      _DRV_USBFSV1_DEVICE_Initialize(x , y)
    #define _DRV_USBFSV1_DEVICE_TASKS_ISR(x)    _DRV_USBFSV1_DEVICE_Tasks_ISR(x)
    #define _DRV_USBFSV1_FOR_DEVICE(x, y)       x y
#elif (DRV_USBFSV1_DEVICE_SUPPORT == false)
    #define _DRV_USBFSV1_DEVICE_INIT(x, y)  
    #define _DRV_USBFSV1_DEVICE_TASKS_ISR(x) 
    #define _DRV_USBFSV1_FOR_DEVICE(x, y)
#endif
 
#if (DRV_USBFSV1_HOST_SUPPORT == true)
    #define _DRV_USBFSV1_HOST_INIT(x, y)    _DRV_USBFSV1_HOST_Initialize(x , y)
    #define _DRV_USBFSV1_HOST_TASKS_ISR(x)  _DRV_USBFSV1_HOST_Tasks_ISR(x)
    #define _DRV_USBFSV1_HOST_ATTACH_DETACH_STATE_MACHINE(x)  _DRV_USBFSV1_HOST_AttachDetachStateMachine(x)
    #define _DRV_USBFSV1_FOR_HOST(x, y)     x y
#elif (DRV_USBFSV1_HOST_SUPPORT == false)
    #define _DRV_USBFSV1_HOST_INIT(x, y)
    #define _DRV_USBFSV1_HOST_TASKS_ISR(x) 
    #define _DRV_USBFSV1_HOST_ATTACH_DETACH_STATE_MACHINE(x)
    #define _DRV_USBFSV1_FOR_HOST(x, y)
#endif

#endif