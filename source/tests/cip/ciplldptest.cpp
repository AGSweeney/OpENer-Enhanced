/*******************************************************************************
 * Copyright (c) 2026, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include <CppUTest/TestHarness.h>

extern "C" {
#include "opener_api.h"
#include "cipcommon.h"
#include "ciplldpmanagement.h"
#include "ciplldpdatatable.h"
}

TEST_GROUP(CipLldp) {
};

#if defined(OPENER_LLDP) && 0 != OPENER_LLDP
TEST(CipLldp, RegistersLldpClassesWhenInitialized) {
  LONGS_EQUAL(kEipStatusOk, CipLldpManagementInit());
  LONGS_EQUAL(kEipStatusOk, CipLldpDataTableInit());

  CipClass *lldp_management_class = GetCipClass(kCipLldpManagementClassCode);
  CipClass *lldp_data_table_class = GetCipClass(kCipLldpDataTableClassCode);
  CHECK_TRUE_TEXT(NULL != lldp_management_class,
                  "LLDP Management class should be registered");
  CHECK_TRUE_TEXT(NULL != lldp_data_table_class,
                  "LLDP Data Table class should be registered");

  CipInstance *lldp_management_instance = GetCipInstance(lldp_management_class, 1);
  CipInstance *lldp_data_table_instance = GetCipInstance(lldp_data_table_class, 1);
  CHECK_TRUE_TEXT(NULL != lldp_management_instance,
                  "LLDP Management instance should be available");
  CHECK_TRUE_TEXT(NULL != lldp_data_table_instance,
                  "LLDP Data Table instance should be available");

  CHECK_TRUE_TEXT(NULL != GetCipAttribute(lldp_management_instance, 1),
                  "LLDP Management attribute 1 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(lldp_management_instance, 2),
                  "LLDP Management attribute 2 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(lldp_management_instance, 3),
                  "LLDP Management attribute 3 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(lldp_management_instance, 4),
                  "LLDP Management attribute 4 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(lldp_management_instance, 5),
                  "LLDP Management attribute 5 should be registered");

  CHECK_TRUE_TEXT(NULL != GetCipAttribute(lldp_data_table_instance, 1),
                  "LLDP Data Table attribute 1 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(lldp_data_table_instance, 4),
                  "LLDP Data Table attribute 4 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(lldp_data_table_instance, 8),
                  "LLDP Data Table attribute 8 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(lldp_data_table_instance, 9),
                  "LLDP Data Table attribute 9 should be registered");
}
#else
TEST(CipLldp, FeatureDisabledBuildCompiles) {
  CHECK_TRUE(true);
}
#endif
