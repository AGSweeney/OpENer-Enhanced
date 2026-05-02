/*******************************************************************************
 * Copyright (c) 2026, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/
#ifndef OPENER_CIPSECURITY_H
#define OPENER_CIPSECURITY_H

#include "ciptypes.h"

/** @brief CIP Security object class code */
static const CipUint kCipSecurityObjectClassCode = 0x5DU;

typedef struct {
  CipUsint state;
  CipWord security_profiles;
  CipWord security_profiles_configured;
} CipSecurityObject;

extern CipSecurityObject g_security;

EipStatus CipSecurityInit(void);

#endif /* OPENER_CIPSECURITY_H */
