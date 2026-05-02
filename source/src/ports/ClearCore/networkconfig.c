/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - Port integration updates.
 *
 ******************************************************************************/
#include <string.h>

#include "cipstring.h"
#include "networkconfig.h"
#include "cipcommon.h"
#include "ciperror.h"
#include "trace.h"
#include "opener_api.h"
#include "lwip/netif.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/netifapi.h"
#include "lwip/dhcp.h"

EipStatus IfaceGetMacAddress(TcpIpInterface *iface,
                             uint8_t *const physical_address) {
  memcpy(physical_address, iface->hwaddr, NETIF_MAX_HWADDR_LEN);
  return kEipStatusOk;
}

static EipStatus GetIpAndNetmaskFromInterface(
    TcpIpInterface *iface, CipTcpIpInterfaceConfiguration *iface_cfg) {
  iface_cfg->ip_address = iface->ip_addr.addr;
  iface_cfg->network_mask = iface->netmask.addr;
  return kEipStatusOk;
}

static EipStatus GetGatewayFromRoute(TcpIpInterface *iface,
                                     CipTcpIpInterfaceConfiguration *iface_cfg) {
  iface_cfg->gateway = iface->gw.addr;
  return kEipStatusOk;
}

EipStatus IfaceGetConfiguration(TcpIpInterface *iface,
                                CipTcpIpInterfaceConfiguration *iface_cfg) {
  CipTcpIpInterfaceConfiguration local_cfg;
  EipStatus status;

  memset(&local_cfg, 0x00, sizeof local_cfg);

  status = GetIpAndNetmaskFromInterface(iface, &local_cfg);
  if (kEipStatusOk == status) {
    status = GetGatewayFromRoute(iface, &local_cfg);
  }
  if (kEipStatusOk == status) {
    ClearCipString(&iface_cfg->domain_name);
    *iface_cfg = local_cfg;
  }
  return status;
}

EipStatus IfaceApplyConfiguration(TcpIpInterface *iface, CipTcpIpObject *tcpip) {
  CipDword config_method;

  if ((NULL == iface) || (NULL == tcpip)) {
    return kEipStatusError;
  }

  config_method = tcpip->config_control & kTcpipCfgCtrlMethodMask;
  if (config_method == kTcpipCfgCtrlDhcp) {
#if LWIP_DHCP
#if LWIP_NETIF_API
    if (ERR_OK != netifapi_dhcp_start(iface)) {
      return kEipStatusError;
    }
#else
    if (ERR_OK != dhcp_start(iface)) {
      return kEipStatusError;
    }
#endif
#endif
  } else if (config_method == kTcpipCfgCtrlStaticIp) {
    CipUdint ip_addr = ntohl(tcpip->interface_configuration.ip_address);
    CipUdint netmask = ntohl(tcpip->interface_configuration.network_mask);
    CipUdint gateway = ntohl(tcpip->interface_configuration.gateway);
    ip4_addr_t ip4_addr;
    ip4_addr_t ip4_netmask;
    ip4_addr_t ip4_gateway;

    IP4_ADDR(&ip4_addr, (ip_addr >> 24) & 0xFF, (ip_addr >> 16) & 0xFF,
             (ip_addr >> 8) & 0xFF, ip_addr & 0xFF);
    IP4_ADDR(&ip4_netmask, (netmask >> 24) & 0xFF, (netmask >> 16) & 0xFF,
             (netmask >> 8) & 0xFF, netmask & 0xFF);
    IP4_ADDR(&ip4_gateway, (gateway >> 24) & 0xFF, (gateway >> 16) & 0xFF,
             (gateway >> 8) & 0xFF, gateway & 0xFF);

#if LWIP_NETIF_API && LWIP_IPV4
    netifapi_netif_set_addr(iface, &ip4_addr, &ip4_netmask, &ip4_gateway);
#else
    netif_set_addr(iface, &ip4_addr, &ip4_netmask, &ip4_gateway);
#endif
  }

#if LWIP_NETIF_HOSTNAME
  if ((NULL != tcpip->hostname.string) && (tcpip->hostname.length > 0)) {
    static char hostname_buffer[65];
    size_t copy_len = (tcpip->hostname.length < 64) ? tcpip->hostname.length : 64;
    memcpy(hostname_buffer, tcpip->hostname.string, copy_len);
    hostname_buffer[copy_len] = '\0';
    netif_set_hostname(iface, hostname_buffer);
  }
#endif

  tcpip->status &= ~kTcpipStatusIfaceCfgPend;
  return kEipStatusOk;
}

void GetHostName(TcpIpInterface *iface,
                 CipString *hostname) {
#if LWIP_NETIF_HOSTNAME
  SetCipStringByCstr(hostname, netif_get_hostname(iface));
#else
  (void)iface;
  ClearCipString(hostname);
#endif
}
