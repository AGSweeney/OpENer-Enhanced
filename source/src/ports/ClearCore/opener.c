/*******************************************************************************
 * Copyright (c) 2023, Peter Christen
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - ClearCore port integration updates.
 *
 ******************************************************************************/

#include "generic_networkhandler.h"
#include "opener_api.h"
#include "cipethernetlink.h"
#include "ciptcpipinterface.h"
#include "trace.h"
#include "networkconfig.h"
#include "doublylinkedlist.h"
#include "cipconnectionobject.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/tcpip.h"
#include <stdlib.h>

#ifdef CLEARCORE
#include "ports/nvdata/nvdata.h"
#endif

volatile int g_end_stack = 0;
struct netif *g_netif = NULL;
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

  g_netif = netif;

  if (IfaceLinkIsUp(netif)) {
    DoublyLinkedListInitialize(&connection_list,
                               CipConnectionObjectListArrayAllocator,
                               CipConnectionObjectListArrayFree);

    {
      uint8_t iface_mac[6];
      IfaceGetMacAddress(netif, iface_mac);
      CipEthernetLinkSetMac(iface_mac);
      SetDeviceSerialNumber(SerialNumberFromMac(iface_mac));
    }

    {
      EipUint16 unique_connection_id = (EipUint16)rand();

#ifdef CLEARCORE
      NvdataLoad();
      (void)IfaceApplyConfiguration(netif, &g_tcpip);
#endif
      GetHostName(netif, &g_tcpip.hostname);

      eip_status = CipStackInit(unique_connection_id);
    }

    g_end_stack = 0;
    eip_status = IfaceGetConfiguration(netif, &g_tcpip.interface_configuration);
    if (eip_status < 0) {
      OPENER_TRACE_WARN("Problems getting interface configuration\n");
    }

    eip_status = NetworkHandlerInitialize();
    if (eip_status != kEipStatusOk) {
      OPENER_TRACE_ERR("NetworkHandlerInitialize failed with status %d\n", eip_status);
      g_end_stack = 1;
    }
  } else {
    OPENER_TRACE_WARN("Network link is down, OpENer not started\n");
    g_end_stack = 1;
  }

  if ((g_end_stack == 0) && (eip_status == kEipStatusOk)) {
    OPENER_TRACE_INFO("OpENer: initialized successfully\n");
  } else {
    OPENER_TRACE_ERR("OpENer initialization failed: g_end_stack=%d, eip_status=%d\n",
                     g_end_stack, eip_status);
  }
}

void opener_cyclic(void) {
  if (g_end_stack) {
    return;
  }

  if (!g_netif || !IfaceLinkIsUp(g_netif)) {
    OPENER_TRACE_INFO("Network link is down, exiting OpENer\n");
    g_end_stack = 1;
    return;
  }

  sys_check_timeouts();

#ifdef TCPIP_THREAD_TEST
  while (tcpip_thread_poll_one() > 0) {
  }
#endif

  if (kEipStatusOk != NetworkHandlerProcessCyclic()) {
    OPENER_TRACE_ERR("Error in NetworkHandler loop! Exiting OpENer!\n");
    g_end_stack = 1;
  }
}

void opener_shutdown(void) {
  if (!g_end_stack) {
    g_end_stack = 1;
    NetworkHandlerFinish();
    ShutdownCipStack();
  }
}

int opener_get_status(void) {
  return g_end_stack;
}
