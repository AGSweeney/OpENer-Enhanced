/*******************************************************************************
 * Copyright (c) 2026, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include "ciplldpdatatable.h"

#include "cipcommon.h"
#include "opener_api.h"

typedef struct {
  CipUint ethernet_link_instance_number;
  CipUint time_to_live;
  CipUdint last_change;
  CipUint position_id;
} CipLldpDataTableObjectValues;

static CipLldpDataTableObjectValues g_lldp_data_table_values = {
  .ethernet_link_instance_number = 1,
  .time_to_live = 120,
  .last_change = 0,
  .position_id = 0
};

EipStatus CipLldpDataTableInit(void) {
  CipClass *lldp_data_table_class = CreateCipClass(
    kCipLldpDataTableClassCode,
    7, /* class attributes */
    7, /* highest class attribute number */
    2, /* class services */
    4, /* instance attributes */
    9, /* highest instance attribute number */
    2, /* instance services */
    1, /* instances */
    "LLDP Data Table",
    1,
    NULL);
  if(NULL == lldp_data_table_class) {
    return kEipStatusError;
  }

  CipInstance *instance = GetCipInstance(lldp_data_table_class, 1);
  if(NULL == instance) {
    return kEipStatusError;
  }

  InsertAttribute(instance,
                  1,
                  kCipUint,
                  EncodeCipUint,
                  (CipAttributeDecodeFromMessage)DecodeCipUint,
                  &g_lldp_data_table_values.ethernet_link_instance_number,
                  kGetableSingleAndAll);
  InsertAttribute(instance,
                  4,
                  kCipUint,
                  EncodeCipUint,
                  (CipAttributeDecodeFromMessage)DecodeCipUint,
                  &g_lldp_data_table_values.time_to_live,
                  kGetableSingleAndAll);
  InsertAttribute(instance,
                  8,
                  kCipUdint,
                  EncodeCipUdint,
                  (CipAttributeDecodeFromMessage)DecodeCipUdint,
                  &g_lldp_data_table_values.last_change,
                  kGetableSingleAndAll);
  InsertAttribute(instance,
                  9,
                  kCipUint,
                  EncodeCipUint,
                  (CipAttributeDecodeFromMessage)DecodeCipUint,
                  &g_lldp_data_table_values.position_id,
                  kGetableSingleAndAll);

  return kEipStatusOk;
}
