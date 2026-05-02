/*******************************************************************************
 * Copyright (c) 2012, Rockwell Automation, Inc.
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - Port integration updates.
 *
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "opener_api.h"
#include "appcontype.h"
#include "trace.h"
#include "cipidentity.h"
#include "ciptcpipinterface.h"
#include "cipqos.h"
#include "nvdata.h"
#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
  #include "cipethernetlink.h"
  #include "ethlinkcbs.h"
#endif

#define DEMO_APP_INPUT_ASSEMBLY_NUM                 100
#define DEMO_APP_OUTPUT_ASSEMBLY_NUM                150
#define DEMO_APP_CONFIG_ASSEMBLY_NUM                151
#define DEMO_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM 152
#define DEMO_APP_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM 153
#define DEMO_APP_EXPLICT_ASSEMBLY_NUM               154

EipUint8 g_assembly_data064[32];
EipUint8 g_assembly_data096[32];
EipUint8 g_assembly_data097[10];
EipUint8 g_assembly_data09A[32];

EipStatus ApplicationInitialization(void) {
  CreateAssemblyObject(DEMO_APP_INPUT_ASSEMBLY_NUM, g_assembly_data064, sizeof(g_assembly_data064));
  CreateAssemblyObject(DEMO_APP_OUTPUT_ASSEMBLY_NUM, g_assembly_data096, sizeof(g_assembly_data096));
  CreateAssemblyObject(DEMO_APP_CONFIG_ASSEMBLY_NUM, g_assembly_data097, sizeof(g_assembly_data097));
  CreateAssemblyObject(DEMO_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM, NULL, 0);
  CreateAssemblyObject(DEMO_APP_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM, NULL, 0);
  CreateAssemblyObject(DEMO_APP_EXPLICT_ASSEMBLY_NUM, g_assembly_data09A, sizeof(g_assembly_data09A));

  ConfigureExclusiveOwnerConnectionPoint(0, DEMO_APP_OUTPUT_ASSEMBLY_NUM,
                                         DEMO_APP_INPUT_ASSEMBLY_NUM,
                                         DEMO_APP_CONFIG_ASSEMBLY_NUM);
  ConfigureInputOnlyConnectionPoint(0, DEMO_APP_HEARTBEAT_INPUT_ONLY_ASSEMBLY_NUM,
                                    DEMO_APP_INPUT_ASSEMBLY_NUM,
                                    DEMO_APP_CONFIG_ASSEMBLY_NUM);
  ConfigureListenOnlyConnectionPoint(0, DEMO_APP_HEARTBEAT_LISTEN_ONLY_ASSEMBLY_NUM,
                                     DEMO_APP_INPUT_ASSEMBLY_NUM,
                                     DEMO_APP_CONFIG_ASSEMBLY_NUM);

  InsertGetSetCallback(GetCipClass(kCipQoSClassCode), NvQosSetCallback, kNvDataFunc);
  InsertGetSetCallback(GetCipClass(kCipTcpIpInterfaceClassCode), NvTcpipSetCallback, kNvDataFunc);

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
  {
    CipClass *p_eth_link_class = GetCipClass(kCipEthernetLinkClassCode);
    InsertGetSetCallback(p_eth_link_class, EthLnkPreGetCallback, kPreGetFunc);
    InsertGetSetCallback(p_eth_link_class, EthLnkPostGetCallback, kPostGetFunc);
    for (int idx = 0; idx < OPENER_ETHLINK_INSTANCE_CNT; ++idx) {
      CipAttributeStruct *p_eth_link_attr;
      CipInstance *p_eth_link_inst = GetCipInstance(p_eth_link_class, idx + 1);
      OPENER_ASSERT(p_eth_link_inst);

      p_eth_link_attr = GetCipAttribute(p_eth_link_inst, 4);
      p_eth_link_attr->attribute_flags |= (kPreGetFunc | kPostGetFunc);

      p_eth_link_attr = GetCipAttribute(p_eth_link_inst, 5);
      p_eth_link_attr->attribute_flags |= (kPreGetFunc | kPostGetFunc);
    }
  }
#endif

  return kEipStatusOk;
}

void HandleApplication(void) {
}

void CheckIoConnectionEvent(unsigned int output_assembly_id,
                            unsigned int input_assembly_id,
                            IoConnectionEvent io_connection_event) {
  (void)output_assembly_id;
  (void)input_assembly_id;
  (void)io_connection_event;
}

EipStatus AfterAssemblyDataReceived(CipInstance *instance) {
  EipStatus status = kEipStatusOk;

  switch (instance->instance_number) {
    case DEMO_APP_OUTPUT_ASSEMBLY_NUM:
      memcpy(&g_assembly_data064[0], &g_assembly_data096[0], sizeof(g_assembly_data064));
      break;
    case DEMO_APP_EXPLICT_ASSEMBLY_NUM:
      break;
    case DEMO_APP_CONFIG_ASSEMBLY_NUM:
      status = kEipStatusOk;
      break;
    default:
      OPENER_TRACE_INFO("Unknown assembly instance in AfterAssemblyDataReceived\n");
      break;
  }

  return status;
}

EipBool8 BeforeAssemblyDataSend(CipInstance *pa_pstInstance) {
  (void)pa_pstInstance;
  return true;
}

EipStatus ResetDevice(void) {
  CloseAllConnections();
  CipQosUpdateUsedSetQosValues();
  return kEipStatusOk;
}

EipStatus ResetDeviceToInitialConfiguration(void) {
  g_tcpip.encapsulation_inactivity_timeout = 120;
  CipQosResetAttributesToDefaultValues();
  ResetDevice();
  return kEipStatusOk;
}

void *CipCalloc(size_t number_of_elements,
                size_t size_of_element) {
  return calloc(number_of_elements, size_of_element);
}

void CipFree(void *data) {
  free(data);
}

void RunIdleChanged(EipUint32 run_idle_value) {
  if ((0x0001 & run_idle_value) == 1) {
    CipIdentitySetExtendedDeviceStatus(kAtLeastOneIoConnectionInRunMode);
  } else {
    CipIdentitySetExtendedDeviceStatus(kAtLeastOneIoConnectionEstablishedAllInIdleMode);
  }
}
