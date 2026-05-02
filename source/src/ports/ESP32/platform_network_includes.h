/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - Port integration updates.
 *
 ******************************************************************************/

#ifndef OPENER_PORTS_ESP32_PLATFORM_NETWORK_INCLUDES_H_
#define OPENER_PORTS_ESP32_PLATFORM_NETWORK_INCLUDES_H_

#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#ifndef O_NONBLOCK
#define O_NONBLOCK 1
#endif

#ifndef F_SETFL
#define F_SETFL 4
#endif

#ifndef F_GETFL
#define F_GETFL 3
#endif

#endif
