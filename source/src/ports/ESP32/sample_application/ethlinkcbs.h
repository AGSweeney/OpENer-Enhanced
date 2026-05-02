/******************************************************************************
 * Copyright (c) 2019, Rockwell Automation, Inc.
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - Port integration updates.
 *
 *****************************************************************************/
#ifndef OPENER_ETHLINKCBS_H_
#define OPENER_ETHLINKCBS_H_

#include "typedefs.h"
#include "ciptypes.h"

EipStatus EthLnkPreGetCallback(
    CipInstance *const instance,
    CipAttributeStruct *const attribute,
    CipByte service);

EipStatus EthLnkPostGetCallback(
    CipInstance *const instance,
    CipAttributeStruct *const attribute,
    CipByte service);

#endif
