/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - Port integration updates.
 *
 ******************************************************************************/

#ifndef OPENER_PORTS_CLEARCORE_NETWORKCONFIG_H_
#define OPENER_PORTS_CLEARCORE_NETWORKCONFIG_H_

#include "opener_api.h"
#include "ciptcpipinterface.h"
#include "lwip/netif.h"

#define IfaceLinkIsUp(iface) netif_is_link_up(iface)

#ifdef __cplusplus
extern "C" {
#endif

EipStatus IfaceApplyConfiguration(TcpIpInterface *iface, CipTcpIpObject *tcpip);

#ifdef __cplusplus
}
#endif

#endif
