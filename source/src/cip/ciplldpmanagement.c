/*******************************************************************************
 * Copyright (c) 2026, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include "ciplldpmanagement.h"

#include "cipcommon.h"
#include "opener_api.h"

typedef struct {
  CipUsint lldp_enable;
  CipUint msg_tx_interval;
  CipUsint msg_tx_hold;
  CipWord lldp_datastore;
  CipUdint last_change;
} CipLldpManagementObjectValues;

static CipLldpManagementObjectValues g_lldp_management_values = {
  .lldp_enable = 0,
  .msg_tx_interval = 30,
  .msg_tx_hold = 4,
  .lldp_datastore = 0,
  .last_change = 0
};

EipStatus CipLldpManagementInit(void) {
  CipClass *lldp_management_class = CreateCipClass(
    kCipLldpManagementClassCode,
    7,  /* class attributes */
    7,  /* highest class attribute number */
    2,  /* class services */
    5,  /* instance attributes */
    5,  /* highest instance attribute number */
    2,  /* instance services */
    1,  /* instances */
    "LLDP Management",
    1,
    NULL);
  if(NULL == lldp_management_class) {
    return kEipStatusError;
  }

  CipInstance *instance = GetCipInstance(lldp_management_class, 1);
  if(NULL == instance) {
    return kEipStatusError;
  }

  InsertAttribute(instance,
                  1,
                  kCipUsint,
                  EncodeCipUsint,
                  (CipAttributeDecodeFromMessage)DecodeCipUsint,
                  &g_lldp_management_values.lldp_enable,
                  kSetAndGetAble);
  InsertAttribute(instance,
                  2,
                  kCipUint,
                  EncodeCipUint,
                  (CipAttributeDecodeFromMessage)DecodeCipUint,
                  &g_lldp_management_values.msg_tx_interval,
                  kSetAndGetAble);
  InsertAttribute(instance,
                  3,
                  kCipUsint,
                  EncodeCipUsint,
                  (CipAttributeDecodeFromMessage)DecodeCipUsint,
                  &g_lldp_management_values.msg_tx_hold,
                  kSetAndGetAble);
  InsertAttribute(instance,
                  4,
                  kCipWord,
                  EncodeCipWord,
                  (CipAttributeDecodeFromMessage)DecodeCipWord,
                  &g_lldp_management_values.lldp_datastore,
                  kSetAndGetAble);
  InsertAttribute(instance,
                  5,
                  kCipUdint,
                  EncodeCipUdint,
                  (CipAttributeDecodeFromMessage)DecodeCipUdint,
                  &g_lldp_management_values.last_change,
                  kSetAndGetAble);

  return kEipStatusOk;
}
