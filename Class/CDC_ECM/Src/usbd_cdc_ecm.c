/*
    USB CDC-ECM for STM32F072 microcontroller

    Copyright (C) 2015,2016,2018 Peter Lawrence

    Permission is hereby granted, free of charge, to any person obtaining a 
    copy of this software and associated documentation files (the "Software"), 
    to deal in the Software without restriction, including without limitation 
    the rights to use, copy, modify, merge, publish, distribute, sublicense, 
    and/or sell copies of the Software, and to permit persons to whom the 
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in 
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
    DEALINGS IN THE SOFTWARE.
*/

/*
Theory of operation:

Each incoming virtual Ethernet packet from the host arrives via CDC_ECM_DATA_OUT_EP.
It arrives in 64-byte chunks, and the last chunk will have a length of less than 64 (signifying a whole packet).
These chunks are accumulated in  cdc_ecm_rx_buffer, and the whole packet is passed to the user's usb_cdc_ecm_recv_callback function.
The user indicates that another packet can be received (writing over the existing data) by calling usb_cdc_ecm_recv_renew().

Note that ST's stack may refuse to "renew", so OutboundTransferNeedsRenewal exists to mop up when this happens.

Outgoing virtual Ethernet packets are sent to the host via CDC_ECM_DATA_IN_IP.
In theory, the ST stack can handle "multi-packet" automatically, but there seem to be issues with this.
So, as with receive packets, transmit packets are sent to the host in 64-byte chunks.
The user should call usb_cdc_ecm_can_xmit() to verify whether it is possible to transmit another packet.
The user then calls usb_cdc_ecm_xmit_packet() to transmit such a packet.
*/

#include "usbd_cdc_ecm.h"
#include "usbd_desc.h"

/* my changes */
#define PCD_SNG_BUF                                      0
#define PCD_DBL_BUF                                      1

/* USB handle declared in main.c */
extern USBD_HandleTypeDef USBD_Device;

extern void CDC_ECM_ReceivePacket(unsigned index, uint8_t *data, uint16_t length);

/* local function prototyping */

static uint8_t USBD_CDC_ECM_Init (USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_CDC_ECM_DeInit (USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_CDC_ECM_Setup (USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_CDC_ECM_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_CDC_ECM_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_CDC_ECM_EP0_RxReady (USBD_HandleTypeDef *pdev);
static const uint8_t *USBD_CDC_ECM_GetFSCfgDesc (uint16_t *length);
static uint8_t USBD_CDC_ECM_SOF (USBD_HandleTypeDef *pdev);

static USBD_StatusTypeDef USBD_CDC_ECM_ReceivePacket (USBD_HandleTypeDef *pdev, unsigned index);

/* class callbacks structure that is used by main.c */
const USBD_ClassTypeDef USBD_CDC_ECM = 
{
  .Init                  = USBD_CDC_ECM_Init,
  .DeInit                = USBD_CDC_ECM_DeInit,
  .Setup                 = USBD_CDC_ECM_Setup,
  .EP0_TxSent            = NULL,
  .EP0_RxReady           = USBD_CDC_ECM_EP0_RxReady,
  .DataIn                = USBD_CDC_ECM_DataIn,
  .DataOut               = USBD_CDC_ECM_DataOut,
  .SOF                   = USBD_CDC_ECM_SOF,
  .IsoINIncomplete       = NULL,
  .IsoOUTIncomplete      = NULL,     
  .GetFSConfigDescriptor = USBD_CDC_ECM_GetFSCfgDesc,    
};

static USBD_HandleTypeDef *registered_pdev;

__ALIGN_BEGIN static uint8_t  cdc_ecm_rx_buffer[CDC_ECM_MAX_SEGMENT_SIZE] __ALIGN_END;
__ALIGN_BEGIN static uint8_t  cdc_ecm_tx_buffer[CDC_ECM_MAX_SEGMENT_SIZE] __ALIGN_END;
__ALIGN_BEGIN static USBD_SetupReqTypedef notify __ALIGN_END =
{
  .bmRequest = 0x21,
  .bRequest = 0 /* NETWORK_CONNECTION */,
  .wValue = 1 /* Connected */,
  .wLength = 0,
};

static int  cdc_ecm_rx_index;
static bool can_xmit;
static bool OutboundTransferNeedsRenewal;
static uint8_t *cdc_ecm_tx_ptr;
static int  cdc_ecm_tx_remaining;
static int  cdc_ecm_tx_busy;
static int copy_length;

void usb_cdc_ecm_recv_renew(void)
{
  USBD_StatusTypeDef outcome;

  outcome = USBD_LL_PrepareReceive(registered_pdev, CDC_ECM_DATA_OUT_EP,  cdc_ecm_rx_buffer +  cdc_ecm_rx_index, CDC_ECM_DATA_OUT_SZ);

  OutboundTransferNeedsRenewal = (USBD_OK != outcome); /* set if the HAL was busy so that we know to retry it */
}

static uint8_t USBD_CDC_ECM_Init (USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  registered_pdev = pdev;

  /* Open EP IN */
  USBD_LL_OpenEP(pdev, CDC_ECM_DATA_IN_EP, USBD_EP_TYPE_BULK, CDC_ECM_DATA_IN_SZ);
  
  /* Open EP OUT */
  USBD_LL_OpenEP(pdev, CDC_ECM_DATA_OUT_EP, USBD_EP_TYPE_BULK, CDC_ECM_DATA_OUT_SZ);

  /* Open Command IN EP */
  USBD_LL_OpenEP(pdev, CDC_ECM_NOTIFICATION_IN_EP, USBD_EP_TYPE_INTR, CDC_ECM_NOTIFICATION_IN_SZ);

  usb_cdc_ecm_recv_renew();
  can_xmit = true;
  OutboundTransferNeedsRenewal = false;
   cdc_ecm_tx_busy = 0;
   cdc_ecm_tx_remaining = 0;

  return USBD_OK;
}

static uint8_t USBD_CDC_ECM_DeInit (USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  registered_pdev = NULL;

  /* Close EP IN */
  USBD_LL_CloseEP(pdev, CDC_ECM_DATA_IN_EP);

  /* Close EP OUT */
  USBD_LL_CloseEP(pdev, CDC_ECM_DATA_OUT_EP);

  /* Close Command IN EP */
  USBD_LL_CloseEP(pdev, CDC_ECM_NOTIFICATION_IN_EP);

  can_xmit = false;

  return USBD_OK;
}

static uint8_t USBD_CDC_ECM_Setup (USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  if (0x43 /* SET_ETHERNET_PACKET_FILTER */ == req->bRequest)
  {
    notify.wIndex = req->wIndex;
    USBD_LL_Transmit(pdev, CDC_ECM_NOTIFICATION_IN_EP, (uint8_t *)&notify, sizeof(notify));
  }

  return USBD_OK;
}

static void  cdc_ecm_incoming_attempt(void)
{
  int chunk_size;

  if (!cdc_ecm_tx_remaining ||  cdc_ecm_tx_busy)
    return;

  chunk_size =  cdc_ecm_tx_remaining;
  if (chunk_size > CDC_ECM_DATA_IN_SZ)
    chunk_size = CDC_ECM_DATA_IN_SZ;

  /* ST stack always returns a success code, so reading the return value is pointless */
  USBD_LL_Transmit(registered_pdev, CDC_ECM_DATA_IN_EP,  cdc_ecm_tx_ptr, chunk_size);

   cdc_ecm_tx_ptr += chunk_size;
   cdc_ecm_tx_remaining -= chunk_size;
   cdc_ecm_tx_busy = 1;
}

static uint8_t USBD_CDC_ECM_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  if (CDC_ECM_DATA_IN_EP == (epnum | 0x80))
  {
     cdc_ecm_tx_busy = 0;
    if (0 ==  cdc_ecm_tx_remaining)
      can_xmit = true;
     cdc_ecm_incoming_attempt();
  }

  return USBD_OK;
}

static uint8_t USBD_CDC_ECM_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum)
{      
  uint32_t RxLength;

  if (CDC_ECM_DATA_OUT_EP != epnum)
    return USBD_OK;

  /* Get the received data length */
  RxLength = USBD_LL_GetRxDataSize (pdev, epnum);

   cdc_ecm_rx_index += RxLength;

  if (RxLength < CDC_ECM_DATA_OUT_SZ)
  {
    usb_cdc_ecm_recv_callback(cdc_ecm_rx_buffer,  cdc_ecm_rx_index);
     cdc_ecm_rx_index = 0;
  }
  else
  {
    /* Initiate next USB packet transfer */
    usb_cdc_ecm_recv_renew();
  }

  return USBD_OK;
}

static uint8_t USBD_CDC_ECM_SOF (USBD_HandleTypeDef *pdev)
{
  /* mop up for any failed USBD_LL_PrepareReceive() call */
  if (OutboundTransferNeedsRenewal)
    usb_cdc_ecm_recv_renew();

  if (cdc_ecm_tx_busy)
  {
    /* ugly hack for ST stack sometimes not providing the DataOut callback */
    if (++cdc_ecm_tx_busy > 32)
    {
       cdc_ecm_tx_busy = 0;
      if (0 ==  cdc_ecm_tx_remaining)
        can_xmit = true;
    }
  }

   cdc_ecm_incoming_attempt();

  return USBD_OK;
}

static uint8_t USBD_CDC_ECM_EP0_RxReady (USBD_HandleTypeDef *pdev)
{ 
  return USBD_OK;
}

static const uint8_t *USBD_CDC_ECM_GetFSCfgDesc (uint16_t *length)
{
  *length = USBD_CfgFSDesc_len;
  return USBD_CfgFSDesc_pnt;
}

uint8_t USBD_CDC_ECM_RegisterInterface(USBD_HandleTypeDef *pdev)
{
  unsigned index;

  return USBD_OK;
}

void USBD_CDC_ECM_PMAConfig(PCD_HandleTypeDef *hpcd, uint32_t *pma_address)
{
  /* allocate PMA memory for all endpoints associated with ECM */
  HAL_PCDEx_PMAConfig(hpcd, CDC_ECM_DATA_IN_EP,  PCD_SNG_BUF, *pma_address);
  *pma_address += CDC_ECM_DATA_IN_SZ;
  HAL_PCDEx_PMAConfig(hpcd, CDC_ECM_DATA_OUT_EP, PCD_SNG_BUF, *pma_address);
  *pma_address += CDC_ECM_DATA_OUT_SZ;
  HAL_PCDEx_PMAConfig(hpcd, CDC_ECM_NOTIFICATION_IN_EP,  PCD_SNG_BUF, *pma_address);
  *pma_address += CDC_ECM_NOTIFICATION_IN_SZ;
}

bool usb_cdc_ecm_can_xmit(void)
{
  bool outcome;

  __disable_irq();
  outcome = can_xmit;
  __enable_irq();

  return outcome;
}

void usb_cdc_ecm_xmit_packet(struct pbuf *p)
{
  struct pbuf *q;
  int packet_size;
  uint8_t *data;

  if (!registered_pdev || !can_xmit)
    return;

  data =  cdc_ecm_tx_buffer;
  packet_size = 0;
  for(q = p; q != NULL; q = q->next)
  {
      memcpy(data, q->payload, q->len);
      data += q->len;
      packet_size += q->len;
  }

  __disable_irq();
  can_xmit = false;
   cdc_ecm_tx_ptr =  cdc_ecm_tx_buffer;
   cdc_ecm_tx_remaining = packet_size;
  copy_length = packet_size;
  __enable_irq();
}
