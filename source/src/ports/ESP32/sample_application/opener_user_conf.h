/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - Port integration updates.
 *
 ******************************************************************************/
#ifndef OPENER_USER_CONF_H_
#define OPENER_USER_CONF_H_

#include <assert.h>
#include "typedefs.h"

#ifndef CIP_FILE_OBJECT
  #define CIP_FILE_OBJECT 0
#endif

#ifndef CIP_SECURITY_OBJECTS
  #define CIP_SECURITY_OBJECTS 0
#endif

#ifdef OPENER_UNIT_TEST
  #include "test_assert.h"
#endif

#ifndef OPENER_IS_DLR_DEVICE
  #define OPENER_IS_DLR_DEVICE 0
#endif

#if defined(OPENER_IS_DLR_DEVICE) && 0 != OPENER_IS_DLR_DEVICE
  #define OPENER_TCPIP_IFACE_CFG_SETTABLE   1
  #define OPENER_ETHLINK_CNTRS_ENABLE       1
  #define OPENER_ETHLINK_IFACE_CTRL_ENABLE  1
  #define OPENER_ETHLINK_LABEL_ENABLE       1
  #define OPENER_ETHLINK_INSTANCE_CNT       3
#endif

#ifdef ESP32
  #define OPENER_TCPIP_IFACE_CFG_SETTABLE 1
  #define OPENER_ETHLINK_CNTRS_ENABLE     1
#endif

#ifndef OPENER_TCPIP_IFACE_CFG_SETTABLE
  #define OPENER_TCPIP_IFACE_CFG_SETTABLE 0
#endif

#ifndef OPENER_ETHLINK_INSTANCE_CNT
  #define OPENER_ETHLINK_INSTANCE_CNT 1
#endif

#ifndef OPENER_ETHLINK_LABEL_ENABLE
  #define OPENER_ETHLINK_LABEL_ENABLE 0
#endif

#ifndef OPENER_ETHLINK_CNTRS_ENABLE
  #define OPENER_ETHLINK_CNTRS_ENABLE 0
#endif

#ifndef OPENER_ETHLINK_IFACE_CTRL_ENABLE
  #define OPENER_ETHLINK_IFACE_CTRL_ENABLE 0
#endif

#define OPENER_CIP_NUM_APPLICATION_SPECIFIC_CONNECTABLE_OBJECTS 1
#define OPENER_CIP_NUM_EXPLICIT_CONNS 6
#define OPENER_CIP_NUM_EXLUSIVE_OWNER_CONNS 1
#define OPENER_CIP_NUM_INPUT_ONLY_CONNS 1
#define OPENER_CIP_NUM_INPUT_ONLY_CONNS_PER_CON_PATH 3
#define OPENER_CIP_NUM_LISTEN_ONLY_CONNS 1
#define OPENER_CIP_NUM_LISTEN_ONLY_CONNS_PER_CON_PATH 3
#define OPENER_NUMBER_OF_SUPPORTED_SESSIONS 20

static const MilliSeconds kOpenerTimerTickInMilliSeconds = 10;

#ifndef OPENER_UNIT_TEST
#define OPENER_ASSERT(assertion) assert(assertion)
#endif

#endif
