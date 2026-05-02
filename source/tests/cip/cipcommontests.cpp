/*******************************************************************************
 * Copyright (c) 2018, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include <CppUTest/TestHarness.h>
#include "CppUTestExt/MockSupport.h"
#include <stdint.h>
#include <string.h>

extern "C" {

#include "opener_api.h"
#include "cipcommon.h"
#include "cipmessagerouter.h"
#include "cipstring.h"

/* Not exposed by cipmessagerouter.h, but intentionally unit-tested here. */
CipError CreateMessageRouterRequestStructure(
  const EipUint8 *data,
  size_t data_length,
  CipMessageRouterRequest *message_router_request);

}

ENIPMessage message; /**< Test variable holds ENIP message*/

TEST_GROUP(CipCommon) {

  void setup() {
    InitializeENIPMessage(&message);
  }

};

TEST(CipCommon, EncodeCipBool) {
  const CipBool value = false;
  EncodeCipBool(&value, &message);
  CHECK_EQUAL(0, *(CipBool *)message.message_buffer);
  POINTERS_EQUAL(message.message_buffer + 1, message.current_message_position);
}

TEST(CipCommon, EncodeCipByte) {
  const CipByte value = 173U;
  EncodeCipBool(&value, &message);
  CHECK_EQUAL(value, *(CipByte *)message.message_buffer);
  POINTERS_EQUAL(message.message_buffer + 1, message.current_message_position);
}

TEST(CipCommon, EncodeCipWord) {
  const CipWord value = 53678U;
  EncodeCipWord(&value, &message);
  CHECK_EQUAL( value, *(CipWord *)(message.message_buffer) );
  POINTERS_EQUAL(message.message_buffer + 2, message.current_message_position);
}

TEST(CipCommon, EncodeCipDword) {
  const CipDword value = 5357678U;
  EncodeCipDword(&value, &message);
  CHECK_EQUAL( value, *(CipDword *)(message.message_buffer) );
  POINTERS_EQUAL(message.message_buffer + 4, message.current_message_position);
}

TEST(CipCommon, EncodeCipLword) {
  const CipLword value = 8353457678U;
  EncodeCipLword(&value, &message);
  CHECK_EQUAL( value, *(CipLword *)(message.message_buffer) );
  POINTERS_EQUAL(message.message_buffer + 8, message.current_message_position);
}


TEST(CipCommon, EncodeCipUsint) {
  const CipUsint value = 212U;
  EncodeCipBool(&value, &message);
  CHECK_EQUAL(value, *(CipUsint *)message.message_buffer);
  POINTERS_EQUAL(message.message_buffer + 1, message.current_message_position);
}

TEST(CipCommon, EncodeCipUint) {
  const CipUint value = 42568U;
  EncodeCipUint(&value, &message);
  CHECK_EQUAL( value, *(CipUint *)(message.message_buffer) );
  POINTERS_EQUAL(message.message_buffer + 2, message.current_message_position);
}

TEST(CipCommon, EncodeCipUdint) {
  const CipUdint value = 1653245U;
  EncodeCipUdint(&value, &message);
  CHECK_EQUAL( value, *(CipUdint *)(message.message_buffer) );
  POINTERS_EQUAL(message.message_buffer + 4, message.current_message_position);
}

TEST(CipCommon, EncodeCipUlint) {
  const CipUlint value = 5357678U;
  EncodeCipUlint(&value, &message);
  CHECK_EQUAL( value, *(CipUlint *)(message.message_buffer) );
  POINTERS_EQUAL(message.message_buffer + 8, message.current_message_position);
}

TEST (CipCommon, DecodeCipString) {
  CipMessageRouterRequest request;
  size_t number_of_strings = 4;
  size_t length_of_string[] = {8,4,0,2};
  size_t pos_in_data = 0;
  const CipOctet data[] =
    "\x08\x00\x4F\x44\x56\x41\x5F\x44\x55\x54\x04\x00\x4F\x44\x56\x41\x00\x00\x02\x00\x43\x41";                       // hex data
  request.data = data;
  request.request_data_size = sizeof(data) - 1;

  CipMessageRouterResponse response;

  CipString string = {};

  for(size_t i = 0; i < number_of_strings; i++) {
    if(0 != length_of_string[i]) {
      mock().expectOneCall("CipCalloc");
      mock().expectOneCall("CipFree");
    }
    DecodeCipString(&string, &request, &response);

    // check string + length
    CHECK_EQUAL(*(data + pos_in_data),  string.length); // first element at current pos contains the length of the following string
    MEMCMP_EQUAL(data + pos_in_data + 2, string.string, string.length ); // pos_in_data + 2 bytes for length

    pos_in_data += length_of_string[i] + 2;
  }

  ClearCipString(&string);
}

TEST (CipCommon, DecodeCipShortString) {
  CipMessageRouterRequest request;
  size_t number_of_strings = 4;
  size_t length_of_string[] = {8,4,0,2};
  size_t pos_in_data = 0;
  const CipOctet data[] =
    "\x08\x4F\x44\x56\x41\x5F\x44\x55\x54\x04\x4F\x44\x56\x41\x00\x02\x43\x41";                       // hex data
  request.data = data;
  request.request_data_size = sizeof(data) - 1;

  CipMessageRouterResponse response;

  CipShortString short_string = {};

  for(size_t i = 0; i < number_of_strings; i++) {
    if(0 != length_of_string[i]) {
      mock().expectOneCall("CipCalloc");
      mock().expectOneCall("CipFree");
    }
    DecodeCipShortString(&short_string, &request, &response);

    // check string + length
    CHECK_EQUAL(*(data + pos_in_data),  short_string.length); // first element at current pos contains the length of the following string
    MEMCMP_EQUAL(data + pos_in_data + 1,
                 short_string.string,
                 short_string.length );                                          // pos_in_data + 1 byte for length

    pos_in_data += length_of_string[i] + 1;
  }

  ClearCipShortString(&short_string);
}

TEST(CipCommon, DecodePaddedEPathSupportsExtendedLogicalSegment) {
  const EipUint8 encoded_path[] = {
    0x02, /* path size in words */
    0x3C, /* logical segment + extended logical + 8-bit format */
    0x01, /* extended logical type: array index */
    0x05, /* value */
    0x00 /* path padding */
  };
  const EipUint8 *message = encoded_path;
  size_t bytes_consumed = 0;
  CipEpath decoded_path = {0};

  EipStatus status = DecodePaddedEPath(&decoded_path,
                                       &message,
                                       sizeof(encoded_path),
                                       &bytes_consumed);

  LONGS_EQUAL(kEipStatusOk, status);
  LONGS_EQUAL(5, bytes_consumed);
  POINTERS_EQUAL(encoded_path + 5, message);
}

TEST(CipCommon, DecodePaddedEPathRejectsTooLargeClassId) {
  const EipUint8 encoded_path[] = {
    0x03, /* path size in words */
    0x22, 0x00, /* logical segment class id 32-bit with pad */
    0x00, 0x00, 0x01, 0x00 /* class id = 65536 */
  };
  const EipUint8 *message = encoded_path;
  size_t bytes_consumed = 0;
  CipEpath decoded_path = {0};

  EipStatus status = DecodePaddedEPath(&decoded_path,
                                       &message,
                                       sizeof(encoded_path),
                                       &bytes_consumed);

  LONGS_EQUAL(kEipStatusError, status);
}

TEST(CipCommon, DecodePaddedEPathRejectsTrailingJunkInsideDeclaredPath) {
  const EipUint8 encoded_path[] = {
    0x02, /* path size in words => 4 bytes follow */
    0x20, 0x01, /* class 1 (8-bit) */
    0xFF, 0xFF /* trailing bytes that are not a valid remaining segment */
  };
  const EipUint8 *message = encoded_path;
  size_t bytes_consumed = 0;
  CipEpath decoded_path = {0};

  EipStatus status = DecodePaddedEPath(&decoded_path,
                                       &message,
                                       sizeof(encoded_path),
                                       &bytes_consumed);

  LONGS_EQUAL(kEipStatusError, status);
}

TEST(CipCommon, CreateMessageRouterRequestRejectsOversizedDeclaredPath) {
  const EipUint8 cip_payload[] = {
    0x0E, /* Get_Attribute_Single */
    0x03, /* path size in words => needs 7 bytes total for path portion */
    0x20, 0x01, 0x24, 0x01 /* only 4 bytes supplied */
  };
  CipMessageRouterRequest request = {0};

  CipError status = CreateMessageRouterRequestStructure(cip_payload,
                                                        sizeof(cip_payload),
                                                        &request);

  LONGS_EQUAL(kCipErrorPathSizeInvalid, status);
}

TEST(CipCommon, CreateMessageRouterRequestRejectsTruncatedLogicalSegment) {
  const EipUint8 cip_payload[] = {
    0x0E, /* Get_Attribute_Single */
    0x02, /* path size in words => 4 bytes path */
    0x22, 0x00, 0x01, 0x00 /* 32-bit logical class segment header + truncated value */
  };
  CipMessageRouterRequest request = {0};

  CipError status = CreateMessageRouterRequestStructure(cip_payload,
                                                        sizeof(cip_payload),
                                                        &request);

  LONGS_EQUAL(kCipErrorPathSegmentError, status);
}

TEST(CipCommon, NotifyMessageRouterReturnsPathSizeInvalidForMalformedDeclaredPath) {
  const EipUint8 cip_payload[] = {
    0x0E, /* Get_Attribute_Single */
    0x03, /* declared path size in words => needs 7 bytes for path portion */
    0x20, 0x01, 0x24, 0x01 /* only 4 path bytes provided */
  };
  CipMessageRouterResponse response = {0};

  EipStatus status = NotifyMessageRouter((EipUint8 *)cip_payload,
                                         (int)sizeof(cip_payload),
                                         &response,
                                         NULL,
                                         0);

  LONGS_EQUAL(kEipStatusOkSend, status);
  LONGS_EQUAL(kCipErrorPathSizeInvalid, response.general_status);
}

TEST(CipCommon, GetAttributeListRejectsMissingAttributeCountField) {
  CipMessageRouterRequest request = {0};
  CipMessageRouterResponse response = {0};
  CipInstance instance = {0};

  request.service = kGetAttributeList;
  request.request_data_size = 0;
  request.data = NULL;

  EipStatus status = GetAttributeList(&instance, &request, &response, NULL, 0);

  LONGS_EQUAL(kEipStatusOkSend, status);
  LONGS_EQUAL(kCipErrorNotEnoughData, response.general_status);
}

TEST(CipCommon, GetAttributeListRejectsCountLargerThanPayload) {
  const EipUint8 payload[] = {
    0x02, 0x00, /* attribute count says 2 */
    0x01, 0x00 /* only one attribute id provided */
  };
  CipMessageRouterRequest request = {0};
  CipMessageRouterResponse response = {0};
  CipInstance instance = {0};

  request.service = kGetAttributeList;
  request.request_data_size = sizeof(payload);
  request.data = payload;

  EipStatus status = GetAttributeList(&instance, &request, &response, NULL, 0);

  LONGS_EQUAL(kEipStatusOkSend, status);
  LONGS_EQUAL(kCipErrorNotEnoughData, response.general_status);
}

TEST(CipCommon, GetAttributeListRejectsTruncatedAttributeIdSequence) {
  const EipUint8 payload[] = {
    0x01, 0x00, /* attribute count says 1 */
    0x42 /* only one byte of the 16-bit attribute id */
  };
  CipMessageRouterRequest request = {0};
  CipMessageRouterResponse response = {0};
  CipInstance instance = {0};

  request.service = kGetAttributeList;
  request.request_data_size = sizeof(payload);
  request.data = payload;

  EipStatus status = GetAttributeList(&instance, &request, &response, NULL, 0);

  LONGS_EQUAL(kEipStatusOkSend, status);
  LONGS_EQUAL(kCipErrorNotEnoughData, response.general_status);
}

TEST(CipCommon, EncodeCipShortStringDoesNotOverflowWhenBufferIsFull) {
  const CipOctet payload[] = { 'T', 'E', 'S', 'T' };
  CipShortString short_string = {0};
  short_string.length = sizeof(payload);
  short_string.string = (EipByte *) payload;

  message.used_message_length = sizeof(message.message_buffer) - 1;
  message.current_message_position = message.message_buffer +
                                     message.used_message_length;

  EncodeCipShortString(&short_string, &message);

  LONGS_EQUAL(sizeof(message.message_buffer), message.used_message_length);
  POINTERS_EQUAL(message.message_buffer + sizeof(message.message_buffer),
                 message.current_message_position);
}

TEST(CipCommon, EncodeCipStringDoesNotOverflowWhenBufferIsFull) {
  const CipOctet payload[] = { 'A', 'B', 'C', 'D' };
  CipString string = {0};
  string.length = sizeof(payload);
  string.string = (EipByte *) payload;

  message.used_message_length = sizeof(message.message_buffer) - 2;
  message.current_message_position = message.message_buffer +
                                     message.used_message_length;

  EncodeCipString(&string, &message);

  LONGS_EQUAL(sizeof(message.message_buffer), message.used_message_length);
  POINTERS_EQUAL(message.message_buffer + sizeof(message.message_buffer),
                 message.current_message_position);
}

