/*******************************************************************************
 * LeapOS-Gateway EtherNet/IP sample application for OpENer RTEMS.
 *
 * Assembly IDs match leap_gateway_config defaults (100 input / 150 output).
 * Bridge hooks are implemented in LeapGateway when OpENer is linked.
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "opener_api.h"
#include "appcontype.h"
#include "trace.h"
#include "cipidentity.h"
#include "ciptcpipinterface.h"
#include "cipqos.h"
#include "nvdata.h"

#define LEAP_GATEWAY_INPUT_ASSEMBLY_NUM  100U
#define LEAP_GATEWAY_OUTPUT_ASSEMBLY_NUM 150U
#define LEAP_GATEWAY_CONFIG_ASSEMBLY_NUM 151U

EipUint8 g_assembly_data064[32];
EipUint8 g_assembly_data096[32];
EipUint8 g_assembly_data097[10];

__attribute__((weak)) void
leap_gateway_eip_apply_output_assembly(const EipUint8 *data, size_t length)
{
  (void)data;
  (void)length;
}

__attribute__((weak)) void
leap_gateway_eip_pack_input_assembly(EipUint8 *data, size_t capacity, size_t *length)
{
  if ((data != NULL) && (capacity > 0U) && (length != NULL)) {
    memset(data, 0, capacity);
    *length = capacity;
  }
}

EipStatus ApplicationInitialization(void) {
  CreateAssemblyObject(LEAP_GATEWAY_INPUT_ASSEMBLY_NUM, g_assembly_data064, sizeof(g_assembly_data064));
  CreateAssemblyObject(LEAP_GATEWAY_OUTPUT_ASSEMBLY_NUM, g_assembly_data096, sizeof(g_assembly_data096));
  CreateAssemblyObject(LEAP_GATEWAY_CONFIG_ASSEMBLY_NUM, g_assembly_data097, sizeof(g_assembly_data097));

  ConfigureExclusiveOwnerConnectionPoint(
    0,
    LEAP_GATEWAY_OUTPUT_ASSEMBLY_NUM,
    LEAP_GATEWAY_INPUT_ASSEMBLY_NUM,
    LEAP_GATEWAY_CONFIG_ASSEMBLY_NUM);

  InsertGetSetCallback(GetCipClass(kCipQoSClassCode), NvQosSetCallback, kNvDataFunc);
  InsertGetSetCallback(GetCipClass(kCipTcpIpInterfaceClassCode), NvTcpipSetCallback, kNvDataFunc);
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
  if (instance->instance_number == LEAP_GATEWAY_OUTPUT_ASSEMBLY_NUM) {
    leap_gateway_eip_apply_output_assembly(
      g_assembly_data096,
      sizeof(g_assembly_data096));
    return kEipStatusOk;
  }
  return kEipStatusOk;
}

EipBool8 BeforeAssemblyDataSend(CipInstance *instance) {
  size_t packed_len = 0U;

  (void)instance;
  leap_gateway_eip_pack_input_assembly(
    g_assembly_data064,
    sizeof(g_assembly_data064),
    &packed_len);
  return true;
}

EipStatus ResetDevice(void) {
  CloseAllConnections();
  CipQosUpdateUsedSetQosValues();
  return kEipStatusOk;
}

void *CipCalloc(size_t number_of_elements, size_t size_of_element) {
  return calloc(number_of_elements, size_of_element);
}

void CipFree(void *data) {
  free(data);
}

EipStatus ResetDeviceToInitialConfiguration(void) {
  return ResetDevice();
}

void RunIdleChanged(EipUint32 run_idle_value) {
  (void)run_idle_value;
}

void ApplicationNotifyLinkDown(void) {
}

void ApplicationNotifyLinkUp(void) {
}
