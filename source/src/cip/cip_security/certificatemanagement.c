/*******************************************************************************
 * Copyright (c) 2026, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include "certificatemanagement.h"

#include "cipcommon.h"
#include "opener_api.h"

#define CERTIFICATE_MANAGEMENT_OBJECT_REVISION 1

static CipByte g_certificate_name_storage[] = "Default Device Certificate";

CertificateManagementObject g_certificate_management = {
  .name = {
    .length = sizeof("Default Device Certificate") - 1,
    .string = g_certificate_name_storage
  },
  .state = 0,
  .certificate_encoding = 0
};

EipStatus CertificateManagementObjectInit(void) {
  CipClass *certificate_management_class = CreateCipClass(
    kCertificateManagementObjectClassCode,
    7, /* class attributes */
    7, /* highest class attribute number */
    2, /* class services */
    3, /* instance attributes */
    5, /* highest instance attribute number */
    2, /* instance services */
    1, /* instances */
    "Certificate Management",
    CERTIFICATE_MANAGEMENT_OBJECT_REVISION,
    NULL);
  if(NULL == certificate_management_class) {
    return kEipStatusError;
  }

  CipInstance *instance = GetCipInstance(certificate_management_class, 1);
  if(NULL == instance) {
    return kEipStatusError;
  }

  InsertAttribute(instance, 1, kCipShortString, EncodeCipShortString,
                  (CipAttributeDecodeFromMessage)DecodeCipShortString,
                  &g_certificate_management.name,
                  kSetAndGetAble);
  InsertAttribute(instance, 2, kCipUsint, EncodeCipUsint,
                  (CipAttributeDecodeFromMessage)DecodeCipUsint,
                  &g_certificate_management.state, kSetAndGetAble);
  InsertAttribute(instance, 5, kCipUsint, EncodeCipUsint,
                  (CipAttributeDecodeFromMessage)DecodeCipUsint,
                  &g_certificate_management.certificate_encoding, kSetAndGetAble);

  return kEipStatusOk;
}
