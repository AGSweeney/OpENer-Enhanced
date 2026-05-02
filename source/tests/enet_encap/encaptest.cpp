/*******************************************************************************
 * Copyright (c) 2018, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/


#include <CppUTest/TestHarness.h>
#include <stdint.h>
#include <string.h>

extern "C" {

#include "encap.h"
#include "cpf.h"

#include "ciperror.h"
#include "ciptypes.h"
#include "enipmessage.h"
#include "cipconnectionobject.h"

}

TEST_GROUP(EncapsulationProtocol) {

};

TEST(EncapsulationProtocol, AnswerListIdentityRequest) {

  CipOctet incoming_message[] =
    "\x63\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xd7\xdd\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00\x00";

  CipOctet expected_outgoing_message[] =
    "\x63\x00\x31\x00\x00\x00\x00\x00\x00\x00\x00\x00\xd7\xdd\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x0c\x00\x2b\x00\x01\x00" \
    "\x00\x02\xaf\x12\xc0\xa8\x38\x65\x00\x00\x00\x00\x00\x00\x00\x00" \
    "\x01\x00\x0c\x00\xe9\xfd\x02\x01\x00\x00\x15\xcd\x5b\x07\x09\x4f" \
    "\x70\x45\x4e\x65\x72\x20\x50\x43\xff";

  ENIPMessage outgoing_message;
  InitializeENIPMessage(&outgoing_message);

  EncapsulationData receive_data;
  CreateEncapsulationStructure(incoming_message,
                               sizeof(incoming_message),
                               &receive_data);

  EncapsulateListIdentityResponseMessage(&receive_data, &outgoing_message);

}

TEST(EncapsulationProtocol, AnswerListServicesRequest) {
  CipOctet incoming_message[] =
    "\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xe0\xdd\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00\x00";

  CipOctet expected_outgoing_message[] =
    "\x04\x00\x1a\x00\x00\x00\x00\x00\x00\x00\x00\x00\xe0\xdd\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00\x00";

  ENIPMessage outgoing_message;
  InitializeENIPMessage(&outgoing_message);

  EncapsulationData recieved_data = {0};
  CreateEncapsulationStructure(incoming_message,
                               sizeof(incoming_message),
                               &recieved_data);

  HandleReceivedListServicesCommand(&recieved_data, &outgoing_message);

}

TEST(EncapsulationProtocol, AnswerListInterfacesRequest) {
  CipOctet incoming_message[] = "";

  CipOctet expected_outgoing_message[] = "";

  ENIPMessage outgoing_message;
  InitializeENIPMessage(&outgoing_message);

  EncapsulationData received_data = {0};
  CreateEncapsulationStructure(incoming_message,
                               sizeof(incoming_message),
                               &received_data);

  HandleReceivedListInterfacesCommand(&received_data, &outgoing_message);
}

TEST(EncapsulationProtocol, AnswerRegisterSessionRequestWrongProtocolVersion) {

  CipOctet incoming_message[] =
    "\x65\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x67\x88\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

  CipOctet expected_outgoing_message[] = "";

  ENIPMessage outgoing_message;
  InitializeENIPMessage(&outgoing_message);

  EncapsulationData received_data = {0};
  CreateEncapsulationStructure(incoming_message,
                               sizeof(incoming_message),
                               &received_data);

  HandleReceivedRegisterSessionCommand(0, &received_data, &outgoing_message);

}

TEST(EncapsulationProtocol, SendRRData) {
  CipOctet incoming_message[] =
    "\x6f\x00\x0c\x00\x01\x00\x00\x00\x00\x00\x00\x00\xf0\xdd\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00" \
    "\x01\x00\x00\x00";

  CipOctet expected_outgoing_message[] = "";

  ENIPMessage outgoing_message = {0};
  InitializeENIPMessage(&outgoing_message);

  EncapsulationData received_data = {0};
  CreateEncapsulationStructure(incoming_message,
                               sizeof(incoming_message),
                               &received_data);

  struct sockaddr_in fake_originator = {0};
  struct sockaddr *fake_originator_pointer =
    (struct sockaddr *)&fake_originator;

  /* agsweeney@gmail.com: updated signature includes socket argument */
  HandleReceivedSendRequestResponseDataCommand(0,
                                               &received_data,
                                               fake_originator_pointer,
                                               &outgoing_message);
}

TEST(EncapsulationProtocol, RejectsCpfWithTooManyItems) {
  CipCommonPacketFormatData cpf_data = {0};
  const EipUint8 malformed_cpf[] = {0x05, 0x00};

  EipStatus status = CreateCommonPacketFormatStructure(malformed_cpf,
                                                       sizeof(malformed_cpf),
                                                       &cpf_data);

  LONGS_EQUAL(kEipStatusError, status);
}

TEST(EncapsulationProtocol, RejectsCpfWithInvalidSockaddrLength) {
  CipCommonPacketFormatData cpf_data = {0};
  const EipUint8 malformed_cpf[] = {
    0x03, 0x00, /* item count */
    0x00, 0x00, 0x00, 0x00, /* null address item */
    0xB2, 0x00, 0x00, 0x00, /* unconnected data item with len 0 */
    0x00, 0x80, 0x01, 0x00 /* O->T sockaddr item with invalid len 1 */
  };

  EipStatus status = CreateCommonPacketFormatStructure(malformed_cpf,
                                                       sizeof(malformed_cpf),
                                                       &cpf_data);

  LONGS_EQUAL(kEipStatusError, status);
}

TEST(EncapsulationProtocol, IgnoreCloseSessionByHandleWithOutOfRangeSession) {
  CipConnectionObject connection_object = {0};
  connection_object.associated_encapsulation_session =
    OPENER_NUMBER_OF_SUPPORTED_SESSIONS + 1;

  CloseSessionBySessionHandle(&connection_object);
}

TEST(EncapsulationProtocol, CpfParsesWithoutGlobalScratchState) {
  CipCommonPacketFormatData cpf_data_first = {0};
  CipCommonPacketFormatData cpf_data_second = {0};
  const EipUint8 valid_cpf[] = {
    0x02, 0x00, /* item count */
    0x00, 0x00, 0x00, 0x00, /* null address item */
    0xB2, 0x00, 0x00, 0x00 /* unconnected data item with len 0 */
  };

  LONGS_EQUAL(kEipStatusOk, CreateCommonPacketFormatStructure(valid_cpf,
                                                              sizeof(valid_cpf),
                                                              &cpf_data_first));
  LONGS_EQUAL(kEipStatusOk, CreateCommonPacketFormatStructure(valid_cpf,
                                                              sizeof(valid_cpf),
                                                              &cpf_data_second));
}

TEST(EncapsulationProtocol, SendRRDataReturnsIncorrectDataOnMalformedCpf) {
  const EipUint8 incoming_message[] = {
    0x6F, 0x00, /* SendRRData */
    0x08, 0x00, /* data length: interface/timeout + CPF item count only */
    0x01, 0x00, 0x00, 0x00, /* session handle */
    0x00, 0x00, 0x00, 0x00, /* status */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sender context */
    0x00, 0x00, 0x00, 0x00, /* options */
    0x00, 0x00, 0x00, 0x00, /* interface handle */
    0x00, 0x00, /* timeout */
    0x05, 0x00 /* malformed CPF item count (>4) */
  };
  ENIPMessage outgoing_message = {0};
  InitializeENIPMessage(&outgoing_message);

  EncapsulationData received_data = {0};
  CreateEncapsulationStructure(incoming_message,
                               sizeof(incoming_message),
                               &received_data);

  struct sockaddr_in fake_originator = {0};
  struct sockaddr *fake_originator_pointer = (struct sockaddr *)&fake_originator;

  EipStatus status = HandleReceivedSendRequestResponseDataCommand(0,
                                                                  &received_data,
                                                                  fake_originator_pointer,
                                                                  &outgoing_message);
  LONGS_EQUAL(kEipStatusError, status);
  LONGS_EQUAL(0, outgoing_message.used_message_length);
}

TEST(EncapsulationProtocol, SendRRDataPropagatesGetAttributeListMalformedPayloadStatus) {
  const EipUint8 incoming_message[] = {
    0x6F, 0x00, /* SendRRData */
    0x11, 0x00, /* data length: 6 + CPF(11) */
    0x01, 0x00, 0x00, 0x00, /* session handle */
    0x00, 0x00, 0x00, 0x00, /* status */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sender context */
    0x00, 0x00, 0x00, 0x00, /* options */
    0x00, 0x00, 0x00, 0x00, /* interface handle */
    0x00, 0x00, /* timeout */
    0x02, 0x00, /* CPF item count */
    0x00, 0x00, 0x00, 0x00, /* null address item */
    0xB2, 0x00, 0x01, 0x00, /* unconnected data item, 1-byte CIP */
    0x03 /* GetAttributeList service only; missing path size and path */
  };
  ENIPMessage outgoing_message = {0};
  InitializeENIPMessage(&outgoing_message);

  EncapsulationData received_data = {0};
  CreateEncapsulationStructure(incoming_message,
                               sizeof(incoming_message),
                               &received_data);

  struct sockaddr_in fake_originator = {0};
  struct sockaddr *fake_originator_pointer = (struct sockaddr *)&fake_originator;

  EipStatus status = HandleReceivedSendRequestResponseDataCommand(0,
                                                                  &received_data,
                                                                  fake_originator_pointer,
                                                                  &outgoing_message);
  LONGS_EQUAL(kEipStatusOkSend, status);

  EncapsulationData response_data = {0};
  CreateEncapsulationStructure(outgoing_message.message_buffer,
                               outgoing_message.used_message_length,
                               &response_data);
  LONGS_EQUAL(kEncapsulationProtocolSuccess, response_data.status);

  GetDintFromMessage((const EipUint8 **const)&response_data.current_communication_buffer_position);
  GetIntFromMessage((const EipUint8 **const)&response_data.current_communication_buffer_position);
  response_data.data_length -= 6;

  CipCommonPacketFormatData response_cpf = {0};
  LONGS_EQUAL(kEipStatusOk, CreateCommonPacketFormatStructure(
                response_data.current_communication_buffer_position,
                response_data.data_length,
                &response_cpf));
  CHECK(response_cpf.data_item.length >= 4);
  LONGS_EQUAL(kCipErrorNotEnoughData, response_cpf.data_item.data[2]);
}

TEST(EncapsulationProtocol, SendRRDataPropagatesPathSizeInvalidStatus) {
  const EipUint8 incoming_message[] = {
    0x6F, 0x00, /* SendRRData */
    0x16, 0x00, /* data length: 6 + CPF(16) */
    0x01, 0x00, 0x00, 0x00, /* session handle */
    0x00, 0x00, 0x00, 0x00, /* status */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* sender context */
    0x00, 0x00, 0x00, 0x00, /* options */
    0x00, 0x00, 0x00, 0x00, /* interface handle */
    0x00, 0x00, /* timeout */
    0x02, 0x00, /* CPF item count */
    0x00, 0x00, 0x00, 0x00, /* null address item */
    0xB2, 0x00, 0x06, 0x00, /* unconnected data item, 6-byte CIP payload */
    0x0E, /* Get_Attribute_Single */
    0x03, /* declared path size in words => needs 7 bytes path portion */
    0x20, 0x01, 0x24, 0x01 /* only 4 path bytes provided */
  };
  ENIPMessage outgoing_message = {0};
  InitializeENIPMessage(&outgoing_message);

  EncapsulationData received_data = {0};
  CreateEncapsulationStructure(incoming_message,
                               sizeof(incoming_message),
                               &received_data);

  struct sockaddr_in fake_originator = {0};
  struct sockaddr *fake_originator_pointer = (struct sockaddr *)&fake_originator;

  EipStatus status = HandleReceivedSendRequestResponseDataCommand(0,
                                                                  &received_data,
                                                                  fake_originator_pointer,
                                                                  &outgoing_message);
  LONGS_EQUAL(kEipStatusOkSend, status);

  EncapsulationData response_data = {0};
  CreateEncapsulationStructure(outgoing_message.message_buffer,
                               outgoing_message.used_message_length,
                               &response_data);
  LONGS_EQUAL(kEncapsulationProtocolSuccess, response_data.status);

  GetDintFromMessage((const EipUint8 **const)&response_data.current_communication_buffer_position);
  GetIntFromMessage((const EipUint8 **const)&response_data.current_communication_buffer_position);
  response_data.data_length -= 6;

  CipCommonPacketFormatData response_cpf = {0};
  LONGS_EQUAL(kEipStatusOk, CreateCommonPacketFormatStructure(
                response_data.current_communication_buffer_position,
                response_data.data_length,
                &response_cpf));
  CHECK(response_cpf.data_item.length >= 4);
  LONGS_EQUAL(kCipErrorPathSizeInvalid, response_cpf.data_item.data[2]);
}
