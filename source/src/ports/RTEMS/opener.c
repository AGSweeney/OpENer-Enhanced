/*******************************************************************************
 * OpENer stack bootstrap for RTEMS libbsd.
 ******************************************************************************/

#include "opener.h"

#include "generic_networkhandler.h"
#include "opener_api.h"
#include "cipethernetlink.h"
#include "ciptcpipinterface.h"
#include "trace.h"
#include "networkconfig.h"
#include "doublylinkedlist.h"
#include "cipconnectionobject.h"

#include <stdlib.h>
#include <string.h>

static char g_rtems_ifname[IF_NAMESIZE];
volatile int g_end_stack = 0;

static CipUdint SerialNumberFromMac(const uint8_t *mac) {
  return ((CipUdint)mac[2] << 24U) |
         ((CipUdint)mac[3] << 16U) |
         ((CipUdint)mac[4] << 8U) |
         (CipUdint)mac[5];
}

void opener_init(const char *ifname) {
  EipStatus eip_status = kEipStatusError;
  uint8_t mac[6];

  if (ifname == NULL) {
    g_end_stack = 1;
    return;
  }

  strncpy(g_rtems_ifname, ifname, sizeof(g_rtems_ifname) - 1U);
  g_rtems_ifname[sizeof(g_rtems_ifname) - 1U] = '\0';

  if (IfaceGetMacAddress(g_rtems_ifname, mac) != kEipStatusOk) {
    OPENER_TRACE_WARN("OpENer: interface %s unavailable\n", g_rtems_ifname);
    g_end_stack = 1;
    return;
  }

  DoublyLinkedListInitialize(&connection_list,
                             CipConnectionObjectListArrayAllocator,
                             CipConnectionObjectListArrayFree);

  CipEthernetLinkSetMac(mac);
  SetDeviceSerialNumber(SerialNumberFromMac(mac));

  eip_status = CipStackInit((EipUint16)rand());
  if (eip_status != kEipStatusOk) {
    g_end_stack = 1;
    return;
  }

  GetHostName(&g_tcpip.hostname);
  eip_status = IfaceGetConfiguration(g_rtems_ifname, &g_tcpip.interface_configuration);
  if (eip_status != kEipStatusOk) {
    OPENER_TRACE_WARN("OpENer: could not read TCP/IP configuration\n");
  }

  eip_status = NetworkHandlerInitialize();
  if (eip_status != kEipStatusOk) {
    g_end_stack = 1;
    return;
  }

  g_end_stack = 0;
  OPENER_TRACE_INFO("OpENer: initialized on %s\n", g_rtems_ifname);
}

void opener_cyclic(void) {
  if (g_end_stack) {
    return;
  }

  if (kEipStatusOk != NetworkHandlerProcessCyclic()) {
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
