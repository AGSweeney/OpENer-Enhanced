/*******************************************************************************
 * Copyright (c) 2026, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/
#ifndef OPENER_CERTIFICATEMANAGEMENT_H
#define OPENER_CERTIFICATEMANAGEMENT_H

#include "ciptypes.h"

/** @brief Certificate Management object class code */
static const CipUint kCertificateManagementObjectClassCode = 0x5FU;

typedef struct {
  CipShortString name;
  CipUsint state;
  CipUsint certificate_encoding;
} CertificateManagementObject;

extern CertificateManagementObject g_certificate_management;

EipStatus CertificateManagementObjectInit(void);

#endif /* OPENER_CERTIFICATEMANAGEMENT_H */
