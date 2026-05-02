/*******************************************************************************
 * Copyright (c) 2026, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/
#ifndef OPENER_ETHERNETIPSECURITY_H
#define OPENER_ETHERNETIPSECURITY_H

#include "ciptypes.h"

/** @brief EtherNet/IP Security object class code */
static const CipUint kEIPSecurityObjectClassCode = 0x5EU;

typedef struct {
  CipUsint state;
  CipDword capability_flags;
  CipBool verify_client_certificate;
  CipBool send_certificate_chain;
  CipBool check_expiration;
  CipUint dtls_timeout;
  CipUsint udp_only_policy;
} EIPSecurityObject;

extern EIPSecurityObject g_eip_security;

EipStatus EIPSecurityInit(void);

#endif /* OPENER_ETHERNETIPSECURITY_H */
