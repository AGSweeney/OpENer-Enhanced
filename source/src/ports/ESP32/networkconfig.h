/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - Port integration updates.
 *
 ******************************************************************************/

#ifndef OPENER_PORTS_ESP32_NETWORKCONFIG_H_
#define OPENER_PORTS_ESP32_NETWORKCONFIG_H_

#include "lwip/netif.h"

#define IfaceLinkIsUp(iface) netif_is_link_up(iface)

#endif
