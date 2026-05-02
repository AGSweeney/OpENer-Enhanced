/*******************************************************************************
 * Copyright (c) 2026, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include "cipsecurity.h"

#include "cipcommon.h"
#include "opener_api.h"

#define CIP_SECURITY_OBJECT_REVISION 1

CipSecurityObject g_security = {
  .state = 0,
  .security_profiles = 0x02,
  .security_profiles_configured = 0x00
};

EipStatus CipSecurityInit(void) {
  CipClass *cip_security_class = CreateCipClass(
    kCipSecurityObjectClassCode,
    7, /* class attributes */
    7, /* highest class attribute number */
    2, /* class services */
    3, /* instance attributes */
    3, /* highest instance attribute number */
    2, /* instance services */
    1, /* instances */
    "CIP Security",
    CIP_SECURITY_OBJECT_REVISION,
    NULL);
  if(NULL == cip_security_class) {
    return kEipStatusError;
  }

  CipInstance *instance = GetCipInstance(cip_security_class, 1);
  if(NULL == instance) {
    return kEipStatusError;
  }

  InsertAttribute(instance, 1, kCipUsint, EncodeCipUsint,
                  (CipAttributeDecodeFromMessage)DecodeCipUsint,
                  &g_security.state, kSetAndGetAble);
  InsertAttribute(instance, 2, kCipWord, EncodeCipWord,
                  (CipAttributeDecodeFromMessage)DecodeCipWord,
                  &g_security.security_profiles, kGetableSingleAndAll);
  InsertAttribute(instance, 3, kCipWord, EncodeCipWord,
                  (CipAttributeDecodeFromMessage)DecodeCipWord,
                  &g_security.security_profiles_configured, kSetAndGetAble);

  return kEipStatusOk;
}
