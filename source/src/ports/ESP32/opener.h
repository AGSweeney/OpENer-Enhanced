/*******************************************************************************
 * Copyright (c) 2023, Peter Christen
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - Port integration updates.
 *
 ******************************************************************************/

#ifndef OPENER_PORTS_ESP32_OPENER_H_
#define OPENER_PORTS_ESP32_OPENER_H_

#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

void opener_init(struct netif *netif);

#ifdef __cplusplus
}
#endif

#endif
