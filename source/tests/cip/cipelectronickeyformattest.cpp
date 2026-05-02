/*******************************************************************************
 * Copyright (c) 2016, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include <CppUTest/TestHarness.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

extern "C" {

#include "cipepath.h"
#include "cipelectronickey.h"

}

TEST_GROUP(CipElectronicKeyFormat) {
  ElectronicKeyFormat4 *key;

  void setup() {
    key = ElectronicKeyFormat4New();
  }

  void teardown() {
    ElectronicKeyFormat4Delete (&key);
  }

};

TEST(CipElectronicKeyFormat, CreateElectronicKey) {
  CipOctet *dummy_area = (CipOctet *)calloc(kElectronicKeyFormat4Size,
                                            sizeof(CipOctet));
  MEMCMP_EQUAL(dummy_area, key, kElectronicKeyFormat4Size);
  free(dummy_area);
};

TEST(CipElectronicKeyFormat, DeleteElectronicKey) {
  ElectronicKeyFormat4Delete(&key);
  POINTERS_EQUAL(NULL, key);
}

TEST(CipElectronicKeyFormat, SetVendorID) {
  CipOctet *demo_area = (CipOctet *)calloc(kElectronicKeyFormat4Size,
                                           sizeof(CipOctet));
  CipUint *vendor_id = (CipUint *)demo_area;
  *vendor_id = 1;
  ElectronicKeyFormat4SetVendorId(key, 1);

  MEMCMP_EQUAL(demo_area, key, kElectronicKeyFormat4Size);
  free(demo_area);
}

TEST(CipElectronicKeyFormat, GetVendorID) {
  CipUint expected_vendor_id = 1;

  CipUint *vendor_id_data = (CipUint *)key;
  *vendor_id_data = expected_vendor_id;

  CipUint vendor_id = ElectronicKeyFormat4GetVendorId(key);
  CHECK_EQUAL(expected_vendor_id, vendor_id);
}

TEST(CipElectronicKeyFormat, SetDeviceType) {
  CipOctet *demo_area = (CipOctet *)calloc(kElectronicKeyFormat4Size,
                                           sizeof(CipOctet));
  CipUint *device_type = (CipUint *)demo_area + 1;
  *device_type = 1;

  ElectronicKeyFormat4SetDeviceType(key, 1);
  MEMCMP_EQUAL(demo_area, key, kElectronicKeyFormat4Size);
  free(demo_area);
}

TEST(CipElectronicKeyFormat, GetDeviceType) {
  CipUint expected_device_type = 1;

  CipUint *device_type_data = (CipUint *)key + 1;
  *device_type_data = expected_device_type;

  CipUint device_type = ElectronicKeyFormat4GetDeviceType(key);
  CHECK_EQUAL(expected_device_type, device_type);
}

TEST(CipElectronicKeyFormat, SetProductCode) {
  CipOctet *demo_area = (CipOctet *)calloc(kElectronicKeyFormat4Size,
                                           sizeof(CipOctet));
  CipUint *product_code = (CipUint *)demo_area + 2;
  *product_code = 1;

  ElectronicKeyFormat4SetProductCode(key, 1);
  MEMCMP_EQUAL(demo_area, key, kElectronicKeyFormat4Size);
  free(demo_area);
}

TEST(CipElectronicKeyFormat, GetProductCode) {
  CipUint expected_product_code = 1;

  CipUint *product_code_data = (CipUint *)key + 2;
  *product_code_data = expected_product_code;

  CipUint product_code = ElectronicKeyFormat4GetProductCode(key);
  CHECK_EQUAL(expected_product_code, product_code);
}

TEST(CipElectronicKeyFormat, SetMajorRevisionCompatibility) {
  CipOctet *demo_area = (CipOctet *)calloc(kElectronicKeyFormat4Size,
                                           sizeof(CipOctet));
  CipByte *major_revision_compatiblitiy = (CipByte *)demo_area + 6;
  *major_revision_compatiblitiy = 0x81;

  ElectronicKeyFormat4SetMajorRevisionCompatibility(key, 0x81);
  MEMCMP_EQUAL(demo_area, key, kElectronicKeyFormat4Size);
  free(demo_area);
}

TEST(CipElectronicKeyFormat, GetMajorRevision) {
  const CipUsint expected_major_revision = 0x1;

  CipUsint set_major_revision = 0x1;
  CipByte *expected_major_data = (CipByte *)key + 6;
  *expected_major_data = set_major_revision;

  CipUint product_code = ElectronicKeyFormat4GetMajorRevision(key);
  CHECK_EQUAL(expected_major_revision, product_code);
}

TEST(CipElectronicKeyFormat, GetMajorRevisionCompatibility) {
  const CipUsint expected_major_revision = 0x81;

  CipByte *expected_major_data = (CipByte *)key + 6;
  *expected_major_data = expected_major_revision;

  bool compatibility = ElectronicKeyFormat4GetMajorRevisionCompatibility(key);
  CHECK_TEXT(compatibility, "Compatibility flag not working");
}

TEST(CipElectronicKeyFormat, SetMinorRevision) {
  CipOctet *demo_area = (CipOctet *)calloc(kElectronicKeyFormat4Size,
                                           sizeof(CipOctet));
  CipByte *minor_revision_compatiblitiy = (CipByte *)demo_area + 7;
  *minor_revision_compatiblitiy = 0x81;

  ElectronicKeyFormat4SetMinorRevision(key, 0x81);
  MEMCMP_EQUAL(demo_area, key, kElectronicKeyFormat4Size);
  free(demo_area);
}

TEST(CipElectronicKeyFormat, GetMinorRevision) {
  CipUsint expected_minor_revision = 0x1;

  CipByte *expected_minor_data = (CipByte *)key + 7;
  *expected_minor_data = expected_minor_revision;

  CipUint product_code = ElectronicKeyFormat4GetMinorRevision(key);
  CHECK_EQUAL(expected_minor_revision, product_code);
}

TEST(CipElectronicKeyFormat, ParseElectronicKeyTest) {
  /* Size of an electronic key is 1 + 1 + 8 (Segment, Key format, Key) */
  const CipOctet message[] =
  { 0x34, 0x04, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x84, 0x05};
  const CipOctet *message_buffer = message;
  GetElectronicKeyFormat4FromMessage(&message_buffer, key);

  message_buffer = message;
  CHECK_EQUAL( 256, ElectronicKeyFormat4GetVendorId(key) );
  CHECK_EQUAL( 512, ElectronicKeyFormat4GetDeviceType(key) );
  CHECK_EQUAL( 768, ElectronicKeyFormat4GetProductCode(key) );
  CHECK_TRUE( ElectronicKeyFormat4GetMajorRevisionCompatibility(key) );
  CHECK_EQUAL( 0x04, ElectronicKeyFormat4GetMajorRevision(key) );
  CHECK_EQUAL( 0x05, ElectronicKeyFormat4GetMinorRevision(key) );

  MEMCMP_EQUAL(message_buffer + 2, key, 8);

}

//TEST(CipElectronicKeyFormat, CheckElectronicKeyWrongVendorId) {
//	const CipOctet message [] = "\x34\x04\x02\x00\x0c\x00\xe9\xfd\x01\x02";
//	const CipOctet *message_buffer = message;
//	EipUint16 *extended_status = 0;
//
//	GetElectronicKeyFormat4FromMessage((const CipOctet**)&message_buffer, key);
//
//	CheckElectronicKeyData(4, key_data, extended_status);
//
//}
