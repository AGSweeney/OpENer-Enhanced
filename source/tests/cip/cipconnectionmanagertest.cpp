/*******************************************************************************
 * Copyright (c) 2016, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include <CppUTest/TestHarness.h>
#include <stdint.h>
#include <string.h>

extern "C" {

#include "cipconnectionmanager.h"

EipStatus GetConnectionOwner(CipInstance *instance,
                             CipMessageRouterRequest *message_router_request,
                             CipMessageRouterResponse *message_router_response,
                             const struct sockaddr *originator_address,
                             const CipSessionHandle encapsulation_session);

}

TEST_GROUP(CipConnectionManager) {

};

TEST(CipConnectionManager, GetConnectionOwnerRejectsTooShortRequestPayload) {
  CipMessageRouterRequest request = {0};
  CipMessageRouterResponse response = {0};

  request.request_data_size = 0;

  EipStatus status = GetConnectionOwner(NULL,
                                        &request,
                                        &response,
                                        NULL,
                                        0);

  LONGS_EQUAL(kEipStatusOk, status);
  LONGS_EQUAL((0x80 | kGetConnectionOwner), response.reply_service);
  LONGS_EQUAL(kCipErrorNotEnoughData, response.general_status);
}

