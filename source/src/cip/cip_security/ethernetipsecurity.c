/*******************************************************************************
 * Copyright (c) 2026, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include "ethernetipsecurity.h"

#include "cipcommon.h"
#include "opener_api.h"

#define ETHERNET_IP_SECURITY_OBJECT_REVISION 1

EIPSecurityObject g_eip_security = {
  .state = 0,
  .capability_flags = 0,
  .verify_client_certificate = false,
  .send_certificate_chain = false,
  .check_expiration = true,
  .dtls_timeout = 1000,
  .udp_only_policy = 0
};

EipStatus EIPSecurityInit(void) {
  CipClass *eip_security_class = CreateCipClass(
    kEIPSecurityObjectClassCode,
    7, /* class attributes */
    7, /* highest class attribute number */
    2, /* class services */
    8, /* instance attributes */
    16, /* highest instance attribute number */
    2, /* instance services */
    1, /* instances */
    "EtherNet/IP Security",
    ETHERNET_IP_SECURITY_OBJECT_REVISION,
    NULL);
  if(NULL == eip_security_class) {
    return kEipStatusError;
  }

  CipInstance *instance = GetCipInstance(eip_security_class, 1);
  if(NULL == instance) {
    return kEipStatusError;
  }

  InsertAttribute(instance, 1, kCipUsint, EncodeCipUsint,
                  (CipAttributeDecodeFromMessage)DecodeCipUsint,
                  &g_eip_security.state, kSetAndGetAble);
  InsertAttribute(instance, 2, kCipDword, EncodeCipDword,
                  (CipAttributeDecodeFromMessage)DecodeCipDword,
                  &g_eip_security.capability_flags, kSetAndGetAble);
  InsertAttribute(instance, 9, kCipBool, EncodeCipBool,
                  (CipAttributeDecodeFromMessage)DecodeCipBool,
                  &g_eip_security.verify_client_certificate, kSetAndGetAble);
  InsertAttribute(instance, 10, kCipBool, EncodeCipBool,
                  (CipAttributeDecodeFromMessage)DecodeCipBool,
                  &g_eip_security.send_certificate_chain, kSetAndGetAble);
  InsertAttribute(instance, 11, kCipBool, EncodeCipBool,
                  (CipAttributeDecodeFromMessage)DecodeCipBool,
                  &g_eip_security.check_expiration, kSetAndGetAble);
  InsertAttribute(instance, 15, kCipUint, EncodeCipUint,
                  (CipAttributeDecodeFromMessage)DecodeCipUint,
                  &g_eip_security.dtls_timeout, kSetAndGetAble);
  InsertAttribute(instance, 16, kCipUsint, EncodeCipUsint,
                  (CipAttributeDecodeFromMessage)DecodeCipUsint,
                  &g_eip_security.udp_only_policy, kSetAndGetAble);

  return kEipStatusOk;
}
