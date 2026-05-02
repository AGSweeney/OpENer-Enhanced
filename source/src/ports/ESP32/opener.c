/*******************************************************************************
 * Copyright (c) 2023, Peter Christen
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - ESP32 port integration updates.
 *
 ******************************************************************************/
#include "generic_networkhandler.h"
#include "opener_api.h"
#include "cipcommon.h"
#include "cipethernetlink.h"
#include "ciptcpipinterface.h"
#include "trace.h"
#include "networkconfig.h"
#include "doublylinkedlist.h"
#include "cipconnectionobject.h"
#include "nvdata.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portable.h"
#include "esp_random.h"

#define OPENER_THREAD_PRIO 5
#define OPENER_STACK_SIZE  8192

static void opener_thread(void *argument);
static SemaphoreHandle_t opener_init_mutex = NULL;
static bool opener_initialized = false;
TaskHandle_t opener_task_handle = NULL;
volatile int g_end_stack = 0;
static const unsigned int kSerialMacByte0 = 2U;
static const unsigned int kSerialMacByte1 = 3U;
static const unsigned int kSerialMacByte2 = 4U;
static const unsigned int kSerialMacByte3 = 5U;
static const unsigned int kSerialShiftMsb = 24U;
static const unsigned int kSerialShiftMidHigh = 16U;
static const unsigned int kSerialShiftMidLow = 8U;

/** @brief Build a deterministic serial number from the device MAC address.
 *
 * @param mac Pointer to a 6-byte MAC address.
 * @return 32-bit serial number derived from MAC bytes 2..5.
 */
static CipUdint SerialNumberFromMac(const uint8_t *mac) {
  return ((CipUdint)mac[kSerialMacByte0] << kSerialShiftMsb) |
         ((CipUdint)mac[kSerialMacByte1] << kSerialShiftMidHigh) |
         ((CipUdint)mac[kSerialMacByte2] << kSerialShiftMidLow)  |
         (CipUdint)mac[kSerialMacByte3];
}

void opener_init(struct netif *netif) {
  EipStatus eip_status = 0;

  if (opener_init_mutex == NULL) {
    opener_init_mutex = xSemaphoreCreateMutex();
    if (opener_init_mutex == NULL) {
      OPENER_TRACE_ERR("Failed to create opener init mutex\n");
      return;
    }
  }

  if (xSemaphoreTake(opener_init_mutex, portMAX_DELAY) != pdTRUE) {
    OPENER_TRACE_ERR("Failed to take opener init mutex\n");
    return;
  }

  if (opener_initialized) {
    OPENER_TRACE_WARN("Opener already initialized, skipping\n");
    xSemaphoreGive(opener_init_mutex);
    return;
  }

  if (IfaceLinkIsUp(netif)) {
    DoublyLinkedListInitialize(&connection_list,
                               CipConnectionObjectListArrayAllocator,
                               CipConnectionObjectListArrayFree);

    {
      uint8_t iface_mac[6];
      EipUint16 unique_connection_id = (EipUint16)(esp_random() & 0xFFFFU);
      CipClass *tcp_ip_class;

      IfaceGetMacAddress(netif, iface_mac);
      SetDeviceSerialNumber(SerialNumberFromMac(iface_mac));

      eip_status = CipStackInit(unique_connection_id);
      tcp_ip_class = GetCipClass(kCipTcpIpInterfaceClassCode);
      if (NULL != tcp_ip_class) {
        InsertGetSetCallback(tcp_ip_class, NvTcpipSetCallback, kNvDataFunc);
      }

      CipEthernetLinkSetMac(iface_mac);
      GetHostName(netif, &g_tcpip.hostname);
    }

    g_end_stack = 0;

    eip_status = IfaceGetConfiguration(netif, &g_tcpip.interface_configuration);
    if (eip_status < 0) {
      OPENER_TRACE_WARN("Problems getting interface configuration\n");
    }

    eip_status = NetworkHandlerInitialize();
  } else {
    OPENER_TRACE_WARN("Network link is down, OpENer not started\n");
    g_end_stack = 1;
  }

  if ((g_end_stack == 0) && (eip_status == kEipStatusOk)) {
    BaseType_t result = xTaskCreatePinnedToCore(opener_thread,
                                                "OpENer",
                                                OPENER_STACK_SIZE,
                                                netif,
                                                OPENER_THREAD_PRIO,
                                                &opener_task_handle,
                                                0);
    if (result == pdPASS) {
      opener_initialized = true;
      OPENER_TRACE_INFO("OpENer: opener_thread started\n");
    } else {
      OPENER_TRACE_ERR("Failed to create OpENer task\n");
    }
  } else {
    OPENER_TRACE_ERR("NetworkHandlerInitialize error %d\n", eip_status);
  }

  xSemaphoreGive(opener_init_mutex);
}

static void opener_thread(void *argument) {
  struct netif *netif = (struct netif*)argument;

  while (!g_end_stack) {
    if (kEipStatusOk != NetworkHandlerProcessCyclic()) {
      OPENER_TRACE_ERR("Error in NetworkHandler loop! Exiting OpENer!\n");
      g_end_stack = 1;
    }
    if (!IfaceLinkIsUp(netif)) {
      OPENER_TRACE_INFO("Network link is down, exiting OpENer\n");
      g_end_stack = 1;
    }
  }

  NetworkHandlerFinish();
  ShutdownCipStack();

  if ((opener_init_mutex != NULL) &&
      (xSemaphoreTake(opener_init_mutex, portMAX_DELAY) == pdTRUE)) {
    opener_initialized = false;
    opener_task_handle = NULL;
    xSemaphoreGive(opener_init_mutex);
  } else {
    opener_task_handle = NULL;
  }

  vTaskDelete(NULL);
}
