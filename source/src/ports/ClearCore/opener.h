/*******************************************************************************
 * Copyright (c) 2021, Peter Christen
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - Port integration updates.
 *
 ******************************************************************************/

#ifndef OPENER_PORTS_CLEARCORE_OPENER_H_
#define OPENER_PORTS_CLEARCORE_OPENER_H_

#ifdef CLEARCORE
#include "ports/ClearCore/socket_types.h"
#endif
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

void opener_init(struct netif *netif);
void opener_cyclic(void);
void opener_shutdown(void);
int opener_get_status(void);

#ifdef __cplusplus
}
#endif

#endif
