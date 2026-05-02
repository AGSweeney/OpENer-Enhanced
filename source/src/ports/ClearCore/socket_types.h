/*******************************************************************************
 * Copyright (c) 2026, Adam G Sweeney
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - ClearCore socket type definitions.
 ******************************************************************************/

#ifndef CLEARCORE_SOCKET_TYPES_H_
#define CLEARCORE_SOCKET_TYPES_H_

#ifdef CLEARCORE
#include "lwip/opt.h"
#ifndef LWIP_SOCKET
#define LWIP_SOCKET 1
#endif
#ifndef LWIP_COMPAT_SOCKETS
#define LWIP_COMPAT_SOCKETS 1
#endif
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

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

#endif
