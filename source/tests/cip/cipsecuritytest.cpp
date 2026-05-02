/*******************************************************************************
 * Copyright (c) 2026, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include <CppUTest/TestHarness.h>

extern "C" {
#include "opener_api.h"
#include "cipcommon.h"
#include "cip_security/cipsecurity.h"
#include "cip_security/ethernetipsecurity.h"
#include "cip_security/certificatemanagement.h"
}

TEST_GROUP(CipSecurityObjects) {
};

#if defined(CIP_SECURITY_OBJECTS) && 0 != CIP_SECURITY_OBJECTS
TEST(CipSecurityObjects, RegistersSecurityClassesAndAttributes) {
  LONGS_EQUAL(kEipStatusOk, CipSecurityInit());
  LONGS_EQUAL(kEipStatusOk, EIPSecurityInit());
  LONGS_EQUAL(kEipStatusOk, CertificateManagementObjectInit());

  CipClass *cip_security_class = GetCipClass(kCipSecurityObjectClassCode);
  CipClass *eip_security_class = GetCipClass(kEIPSecurityObjectClassCode);
  CipClass *certificate_management_class =
    GetCipClass(kCertificateManagementObjectClassCode);
  CHECK_TRUE_TEXT(NULL != cip_security_class,
                  "CIP Security class should be registered");
  CHECK_TRUE_TEXT(NULL != eip_security_class,
                  "EtherNet/IP Security class should be registered");
  CHECK_TRUE_TEXT(NULL != certificate_management_class,
                  "Certificate Management class should be registered");

  CipInstance *cip_security_instance = GetCipInstance(cip_security_class, 1);
  CipInstance *eip_security_instance = GetCipInstance(eip_security_class, 1);
  CipInstance *certificate_management_instance =
    GetCipInstance(certificate_management_class, 1);
  CHECK_TRUE_TEXT(NULL != cip_security_instance,
                  "CIP Security instance should be available");
  CHECK_TRUE_TEXT(NULL != eip_security_instance,
                  "EIP Security instance should be available");
  CHECK_TRUE_TEXT(NULL != certificate_management_instance,
                  "Certificate Management instance should be available");

  CHECK_TRUE_TEXT(NULL != GetCipAttribute(cip_security_instance, 1),
                  "CIP Security attribute 1 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(cip_security_instance, 2),
                  "CIP Security attribute 2 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(cip_security_instance, 3),
                  "CIP Security attribute 3 should be registered");

  CHECK_TRUE_TEXT(NULL != GetCipAttribute(eip_security_instance, 1),
                  "EIP Security attribute 1 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(eip_security_instance, 2),
                  "EIP Security attribute 2 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(eip_security_instance, 9),
                  "EIP Security attribute 9 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(eip_security_instance, 10),
                  "EIP Security attribute 10 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(eip_security_instance, 11),
                  "EIP Security attribute 11 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(eip_security_instance, 15),
                  "EIP Security attribute 15 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(eip_security_instance, 16),
                  "EIP Security attribute 16 should be registered");

  CHECK_TRUE_TEXT(NULL != GetCipAttribute(certificate_management_instance, 1),
                  "Certificate Management attribute 1 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(certificate_management_instance, 2),
                  "Certificate Management attribute 2 should be registered");
  CHECK_TRUE_TEXT(NULL != GetCipAttribute(certificate_management_instance, 5),
                  "Certificate Management attribute 5 should be registered");
}
#else
TEST(CipSecurityObjects, FeatureDisabledBuildCompiles) {
  CHECK_TRUE(true);
}
#endif
