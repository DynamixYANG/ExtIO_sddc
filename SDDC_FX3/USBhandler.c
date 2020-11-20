/*
 * USB_handler.c
 *
 *  Created on: 21/set/2020
 *
 *  HF103_FX3 project
 *  Author: Oscar Steila
 *  modified from: SuperSpeed Device Design By Example - John Hyde
 *
 *  https://sdr-prototypes.blogspot.com/
 */

#include "Application.h"

// Declare external functions
extern void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);
extern void StartApplication(void);
extern void StopApplication(void);
extern CyU3PReturnStatus_t SetUSBdescriptors(void);
extern void WriteAttenuator(uint8_t value);
extern CyU3PReturnStatus_t Si5351init(UINT32 freqa, UINT32 freqb);



// Declare external data
extern CyU3PQueue EventAvailable;			  	// Used for threads communications
extern CyBool_t glIsApplnActive;				// Set true once device is enumerated
extern uint8_t  HWconfig;       			    // Hardware config
extern uint16_t  FWconfig;       			    // Firmware config hb.lb

extern ReturnStatus_t rt820init(void);
extern void set_all_gains(UINT8 gain_index);
extern void set_freq(UINT32 freq);
extern void rt820shutdown(void);
extern uint8_t m_gain_index;
// Global data owned by this module



#define CYFX_SDRAPP_MAX_EP0LEN  64      /* Max. data length supported for EP0 requests. */

extern CyU3PDmaMultiChannel glMultiChHandleSlFifoPtoU;
// Global data owned by this module
CyU3PDmaChannel glGPIF2USB_Handle;
uint8_t  *glEp0Buffer = 0;              /* Buffer used to handle vendor specific control requests. */
uint8_t  vendorRqtCnt = 0;

/* Callback to handle the USB setup requests. */
CyBool_t
CyFxSlFifoApplnUSBSetupCB (
        uint32_t setupdat0,
        uint32_t setupdat1
    )
{
    /* Fast enumeration is used. Only requests addressed to the interface, class,
     * vendor and unknown control requests are received by this function.
     * This application does not support any class or vendor requests. */

    uint8_t  bRequest, bReqType;
    uint8_t  bType, bTarget;
    uint16_t wValue, wIndex;
    uint16_t wLength;
    CyBool_t isHandled = CyFalse;
    CyU3PReturnStatus_t apiRetStatus;

    /* Decode the fields from the setup request. */
    bReqType = (setupdat0 & CY_U3P_USB_REQUEST_TYPE_MASK);
    bType    = (bReqType & CY_U3P_USB_TYPE_MASK);
    bTarget  = (bReqType & CY_U3P_USB_TARGET_MASK);
    bRequest = ((setupdat0 & CY_U3P_USB_REQUEST_MASK) >> CY_U3P_USB_REQUEST_POS);
    wValue   = ((setupdat0 & CY_U3P_USB_VALUE_MASK)   >> CY_U3P_USB_VALUE_POS);
    wIndex   = ((setupdat1 & CY_U3P_USB_INDEX_MASK)   >> CY_U3P_USB_INDEX_POS);
    wLength   = ((setupdat1 & CY_U3P_USB_LENGTH_MASK)   >> CY_U3P_USB_LENGTH_POS);

    if (bType == CY_U3P_USB_STANDARD_RQT)
    {
        /* Handle SET_FEATURE(FUNCTION_SUSPEND) and CLEAR_FEATURE(FUNCTION_SUSPEND)
         * requests here. It should be allowed to pass if the device is in configured
         * state and failed otherwise. */
        if ((bTarget == CY_U3P_USB_TARGET_INTF) && ((bRequest == CY_U3P_USB_SC_SET_FEATURE)
                    || (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)) && (wValue == 0))
        {
            if (glIsApplnActive)
                CyU3PUsbAckSetup ();
            else
            {
                CyU3PUsbStall (0, CyTrue, CyFalse);
            }
            isHandled = CyTrue;
        }

        /* CLEAR_FEATURE request for endpoint is always passed to the setup callback
         * regardless of the enumeration model used. When a clear feature is received,
         * the previous transfer has to be flushed and cleaned up. This is done at the
         * protocol level. So flush the EP memory and reset the DMA channel associated
         * with it. If there are more than one EP associated with the channel reset both
         * the EPs. The endpoint stall and toggle / sequence number is also expected to be
         * reset. Return CyFalse to make the library clear the stall and reset the endpoint
         * toggle. Or invoke the CyU3PUsbStall (ep, CyFalse, CyTrue) and return CyTrue.
         * Here we are clearing the stall. */
        if ((bTarget == CY_U3P_USB_TARGET_ENDPT) && (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)
                && (wValue == CY_U3P_USBX_FS_EP_HALT))
        {
            if (glIsApplnActive)
            {
                if (wIndex == CY_FX_EP_CONSUMER)
                {
                    CyU3PDmaMultiChannelReset (&glMultiChHandleSlFifoPtoU);
                    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
                    CyU3PUsbResetEp (CY_FX_EP_CONSUMER);
                    CyU3PDmaMultiChannelSetXfer (&glMultiChHandleSlFifoPtoU,FIFO_DMA_RX_SIZE,0);
                }
                CyU3PUsbStall (wIndex, CyFalse, CyTrue);
                isHandled = CyTrue;
            }
        }
    } else if (bType == CY_U3P_USB_VENDOR_RQT) {

    	outxio_t * pdata;
    	/*
   	    uint8_t * pd = (uint8_t *) &glEp0Buffer[0];
    	uint32_t event =( (VENDOR_RQT<<24) | ( bRequest<<16) | (pd[0]<<8) | pd[1] );
    	CyU3PQueueSend(&EventAvailable, &event, CYU3P_NO_WAIT);
  */
    	isHandled = CyFalse;

    	switch (bRequest)
    	 {
			case GPIOFX3:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{
						uint16_t mdata = * (uint16_t *) &glEp0Buffer[0];
						switch(HWconfig)
						{
						case BBRF103:
							bbrf103_GpioSet(mdata);
							isHandled = CyTrue;
						   break;

						case HF103:
							hf103_GpioSet(mdata);
							isHandled = CyTrue;
							break;

							isHandled = CyTrue;
						   break;

						case RX888:
							rx888_GpioSet(mdata);
							isHandled = CyTrue;
							break;

						}
					}
					break;

			case DAT31FX3:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{
						pdata = (outxio_t *) &glEp0Buffer[0];
						switch(HWconfig)
						{
							case HF103:
							hf103_SetAttenuator(pdata->buffer[0]);
							isHandled = CyTrue;
							break;
						}
					}
					break;

			case SI5351A:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{
						uint32_t fa,fb;
						fa = *(uint32_t *) &glEp0Buffer[0];
						fb = *(uint32_t *) &glEp0Buffer[4];
						Si5351init( fa ,fb );
						isHandled = CyTrue;
					}
					break;


			case I2CWFX3:
					apiRetStatus  = CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
					if (apiRetStatus == CY_U3P_SUCCESS)
						{
							apiRetStatus = I2cTransfer ( wIndex, wValue, wLength, glEp0Buffer, CyFalse);
							if (apiRetStatus == CY_U3P_SUCCESS)
								isHandled = CyTrue;
							else
							{
								CyU3PDebugPrint (4, "I2cwrite Error %d\n", apiRetStatus);
								isHandled = CyTrue; // ?!?!?!
							}
						}
					break;

			case I2CRFX3:
					CyU3PMemSet (glEp0Buffer, 0, sizeof (glEp0Buffer));
					apiRetStatus = I2cTransfer (wIndex, wValue, wLength, glEp0Buffer, CyTrue);
					if (apiRetStatus == CY_U3P_SUCCESS)
					{
						apiRetStatus = CyU3PUsbSendEP0Data(wLength, glEp0Buffer);
						isHandled = CyTrue;
					}
					break;

			case R820T2INIT:
					{
						uint16_t dataw = rt820init();
						glEp0Buffer[0] = dataw;
						glEp0Buffer[1] = dataw >> 8;
						glEp0Buffer[2] = 0;
						glEp0Buffer[3] = 0;

						CyU3PUsbSendEP0Data (4, glEp0Buffer);
						vendorRqtCnt++;
						isHandled = CyTrue;
					}
					break;

			case R820T2STDBY:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
						{
							rt820shutdown();
							isHandled = CyTrue;
						}
					break;


			case R820T2TUNE:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{

						uint32_t freq;
						freq = *(uint32_t *) &glEp0Buffer[0];
						set_freq((uint32_t) freq);
						// R820T2 tune
						DebugPrint(4, "\r\n\r\nTune R820T2 %d \r\n",freq);
						isHandled = CyTrue;
					}
					break;

			case R820T2SETATT:
					if(CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL)== CY_U3P_SUCCESS)
					{
						uint8_t attIdx;
						attIdx = glEp0Buffer[0];
						set_all_gains(attIdx); // R820T2 set att
						isHandled = CyTrue;
					}
					break;

			case R820T2GETATT:
					glEp0Buffer[0] = m_gain_index;
					CyU3PUsbSendEP0Data (1, glEp0Buffer);
					isHandled = CyTrue;
					break;

    	 	case STARTFX3:
    	 		    CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
    	 		    CyU3PGpifControlSWInput ( CyFalse );
    	 		 	CyU3PDmaMultiChannelReset (&glMultiChHandleSlFifoPtoU);  // Reset existing channel
					apiRetStatus = CyU3PDmaMultiChannelSetXfer (&glMultiChHandleSlFifoPtoU, FIFO_DMA_RX_SIZE,0); //start
					if (apiRetStatus == CY_U3P_SUCCESS)
					{
						isHandled = CyTrue;
					}
					CyU3PGpifControlSWInput ( CyTrue );
					break;

			case STOPFX3:
				    CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
					CyU3PGpifControlSWInput ( CyFalse   );
					CyU3PThreadSleep(10);
					CyU3PDmaMultiChannelReset (&glMultiChHandleSlFifoPtoU);
					CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
					isHandled = CyTrue;
					break;

			case RESETFX3:	// RESETTING CPU BY PC APPLICATION
				    CyU3PUsbGetEP0Data(wLength, glEp0Buffer, NULL);
					DebugPrint(4, "\r\n\r\nHOST RESETTING CPU \r\n");
					CyU3PThreadSleep(100);
					CyU3PDeviceReset(CyFalse);
					break;


            case TESTFX3:
					glEp0Buffer[0] =  HWconfig;
					glEp0Buffer[1] = (uint8_t) (FWconfig >> 8);
					glEp0Buffer[2] = (uint8_t) FWconfig;
					glEp0Buffer[3] = vendorRqtCnt;
					CyU3PUsbSendEP0Data (4, glEp0Buffer);
					vendorRqtCnt++;
					isHandled = CyTrue;
					break;

            default: /* unknown request, stall the endpoint. */

					isHandled = CyFalse;
					CyU3PDebugPrint (4, "STALL EP0 V.REQ %x\n",bRequest);
					CyU3PUsbStall (0, CyTrue, CyFalse);
					break;
    	}
   	    uint8_t * pd = (uint8_t *) &glEp0Buffer[0];
    	uint32_t event =( (VENDOR_RQT<<24) | ( bRequest<<16) | (pd[1]<<8) | pd[0] );
    	CyU3PQueueSend(&EventAvailable, &event, CYU3P_NO_WAIT);
    }
    return isHandled;
}


/* This is the callback function to handle the USB events. */
void USBEvent_Callback ( CyU3PUsbEventType_t evtype, uint16_t evdata)
{
	uint32_t event = evtype;
	CyU3PQueueSend(&EventAvailable, &event, CYU3P_NO_WAIT);
    switch (evtype)
    {
        case CY_U3P_USB_EVENT_SETCONF:
            /* Stop the application before re-starting. */
            if (glIsApplnActive) StopApplication ();
            	StartApplication ();
            break;

        case CY_U3P_USB_EVENT_CONNECT:
        	break;
        case CY_U3P_USB_EVENT_RESET:
        case CY_U3P_USB_EVENT_DISCONNECT:
            if (glIsApplnActive)
            {
            	StopApplication ();
              	CyU3PUsbLPMEnable ();
            }
            break;

        case CY_U3P_USB_EVENT_EP_UNDERRUN:
        	DebugPrint (4, "\r\nEP Underrun on %d", evdata);
            break;

        case CY_U3P_USB_EVENT_EP0_STAT_CPLT:
               /* Make sure the bulk pipe is resumed once the control transfer is done. */
            break;

        default:
            break;
    }
}
/* Callback function to handle LPM requests from the USB 3.0 host. This function is invoked by the API
   whenever a state change from U0 -> U1 or U0 -> U2 happens. If we return CyTrue from this function, the
   FX3 device is retained in the low power state. If we return CyFalse, the FX3 device immediately tries
   to trigger an exit back to U0.
   This application does not have any state in which we should not allow U1/U2 transitions; and therefore
   the function always return CyTrue.
 */
CyBool_t  LPMRequest_Callback ( CyU3PUsbLinkPowerMode link_mode)
{
	DebugPrint (4, "\r\n LPMRequest_Callback link_mode %x \r\n", link_mode );
	return CyTrue;
}



// Spin up USB, let the USB driver handle enumeration
CyU3PReturnStatus_t InitializeUSB(void)
{
	CyU3PReturnStatus_t Status;
	CyBool_t NeedToRenumerate = CyTrue;
    /* Allocate a buffer for handling control requests. */
    glEp0Buffer = (uint8_t *)CyU3PDmaBufferAlloc (CYFX_SDRAPP_MAX_EP0LEN);

	Status = CyU3PUsbStart();

    if (Status == CY_U3P_ERROR_NO_REENUM_REQUIRED)
    {
    	NeedToRenumerate = CyFalse;
    	Status = CY_U3P_SUCCESS;
    	DebugPrint(4,"\r\nNeedToRenumerate = CyFalse");
    }
    CheckStatus("Start USB Driver", Status);

  // Setup callbacks to handle the setup requests, USB Events and LPM Requests (for USB 3.0)
    CyU3PUsbRegisterSetupCallback(CyFxSlFifoApplnUSBSetupCB, CyTrue);
    CyU3PUsbRegisterEventCallback(USBEvent_Callback);
    CyU3PUsbRegisterLPMRequestCallback( LPMRequest_Callback );

    // Driver needs all of the descriptors so it can supply them to the host when requested
    Status = SetUSBdescriptors();
    CheckStatus("Set USB Descriptors", Status);
    // Connect the USB Pins with SuperSpeed operation enabled
    if (NeedToRenumerate)
    {
		  Status = CyU3PConnectState(CyTrue, CyTrue);
		  CheckStatus("ConnectUSB", Status);

    }
    else	// USB connection already exists, restart the Application
    {
    	if (glIsApplnActive) StopApplication();
    	StartApplication();
    }

	return Status;
}
