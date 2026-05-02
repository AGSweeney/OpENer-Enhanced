/******************************************************************************
 * Copyright (c) 2019, Rockwell Automation, Inc.
 * All rights reserved.
 *
 *****************************************************************************/

/** @file
 * @brief Ethernet Link object callbacks
 *
 * This module implements the Ethernet Link object callbacks. These callbacks
 *  handle the update and clear operation for the interface and media counters
 *  of every Ethernet Link object of our device.
 *
 * The current implementation is only a dummy implementation that doesn't
 *  return real counters of the interface(s). It is only intended to check
 *  whether the EIP stack transmits the counters at the right position in
 *  the response while we're are filling the counters in the Ethernet Link
 *  counter attributes by their union member names.
 */

/*---------------------------------------------------------------------------*/
/*                               INCLUDES                                    */
/*---------------------------------------------------------------------------*/
#include "ethlinkcbs.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iphlpapi.h>

#include "opener_user_conf.h"
#include "cipethernetlink.h"
#include "trace.h"

/*---------------------------------------------------------------------------*/
/*                                LOCALS                                     */
/*---------------------------------------------------------------------------*/

typedef struct {
  CipEthernetLinkInterfaceCounters interface_cntrs;
  CipEthernetLinkMediaCounters media_cntrs;
} EthLinkCounterSnapshot;

static EthLinkCounterSnapshot s_last_absolute[OPENER_ETHLINK_INSTANCE_CNT];
static EthLinkCounterSnapshot s_baseline[OPENER_ETHLINK_INSTANCE_CNT];
static bool s_baseline_valid[OPENER_ETHLINK_INSTANCE_CNT];
static NET_IFINDEX s_if_index = 0U;
static bool s_if_index_valid = false;

/*---------------------------------------------------------------------------*/
/*                           IMPLEMENTATION                                  */
/*---------------------------------------------------------------------------*/

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
/** @brief Clamp 64-bit OS counters to CIP UDINT range.
 *
 * @param value Platform counter value.
 * @return Counter value saturated to UINT32_MAX.
 */
static CipUdint ClampU64ToCipUdint(const ULONG64 value) {
  return value > UINT32_MAX ? UINT32_MAX : (CipUdint)value;
}

/** @brief Calculate non-negative counter delta from a baseline.
 *
 * @param current Current absolute counter value.
 * @param baseline Baseline value captured at last clear.
 * @return Non-negative delta between current and baseline.
 */
static CipUdint CounterDelta(const CipUdint current,
                             const CipUdint baseline) {
  return current >= baseline ? (current - baseline) : 0U;
}

/** @brief Resolve Windows interface index from configured OpENer MAC address.
 *
 * @return true if an interface index matching the configured MAC was found.
 */
static bool ResolveInterfaceIndexByMac(void) {
  MIB_IF_TABLE2 *if_table = NULL;
  DWORD result = GetIfTable2(&if_table);
  if(NO_ERROR != result || NULL == if_table) {
    return false;
  }

  s_if_index_valid = false;
  for(ULONG idx = 0; idx < if_table->NumEntries; ++idx) {
    const MIB_IF_ROW2 *const row = &if_table->Table[idx];
    if(6 == row->PhysicalAddressLength &&
       0 == memcmp(row->PhysicalAddress, g_ethernet_link[0].physical_address, 6)) {
      s_if_index = row->InterfaceIndex;
      s_if_index_valid = true;
      break;
    }
  }

  FreeMibTable(if_table);
  return s_if_index_valid;
}

/** @brief Read absolute interface and media counters from Windows IP Helper.
 *
 * @param snapshot Destination for absolute counter values.
 * @return true on successful counter read.
 */
static bool ReadAbsoluteCounters(EthLinkCounterSnapshot *const snapshot) {
  if((!s_if_index_valid) && (!ResolveInterfaceIndexByMac())) {
    return false;
  }

  MIB_IF_ROW2 row;
  memset(&row, 0, sizeof(row));
  row.InterfaceIndex = s_if_index;
  if(NO_ERROR != GetIfEntry2(&row)) {
    s_if_index_valid = false;
    return false;
  }

  snapshot->interface_cntrs.ul.in_octets = ClampU64ToCipUdint(row.InOctets);
  snapshot->interface_cntrs.ul.in_ucast = ClampU64ToCipUdint(row.InUcastPkts);
  snapshot->interface_cntrs.ul.in_nucast = ClampU64ToCipUdint(row.InNUcastPkts);
  snapshot->interface_cntrs.ul.in_discards = ClampU64ToCipUdint(row.InDiscards);
  snapshot->interface_cntrs.ul.in_errors = ClampU64ToCipUdint(row.InErrors);
  snapshot->interface_cntrs.ul.in_unknown_protos = ClampU64ToCipUdint(row.InUnknownProtos);
  snapshot->interface_cntrs.ul.out_octets = ClampU64ToCipUdint(row.OutOctets);
  snapshot->interface_cntrs.ul.out_ucast = ClampU64ToCipUdint(row.OutUcastPkts);
  snapshot->interface_cntrs.ul.out_nucast = ClampU64ToCipUdint(row.OutNUcastPkts);
  snapshot->interface_cntrs.ul.out_discards = ClampU64ToCipUdint(row.OutDiscards);
  snapshot->interface_cntrs.ul.out_errors = ClampU64ToCipUdint(row.OutErrors);

  snapshot->media_cntrs.ul.align_errs = ClampU64ToCipUdint(row.InErrors);
  snapshot->media_cntrs.ul.fcs_errs = ClampU64ToCipUdint(row.InErrors);
  snapshot->media_cntrs.ul.single_coll = 0U;
  snapshot->media_cntrs.ul.multi_coll = 0U;
  snapshot->media_cntrs.ul.sqe_test_errs = 0U;
  snapshot->media_cntrs.ul.def_trans = 0U;
  snapshot->media_cntrs.ul.late_coll = ClampU64ToCipUdint(row.OutDiscards);
  snapshot->media_cntrs.ul.exc_coll = 0U;
  snapshot->media_cntrs.ul.mac_tx_errs = ClampU64ToCipUdint(row.OutErrors);
  snapshot->media_cntrs.ul.crs_errs = 0U;
  snapshot->media_cntrs.ul.frame_too_long = ClampU64ToCipUdint(row.InDiscards);
  snapshot->media_cntrs.ul.mac_rx_errs = ClampU64ToCipUdint(row.InErrors);

  return true;
}

/** @brief Apply GetAndClear baseline to a fresh absolute counter snapshot.
 *
 * @param instance_number Ethernet link instance number.
 * @param absolute_snapshot Latest absolute counter values.
 */
static void ApplyCounterSnapshotWithBaseline(const CipInstanceNum instance_number,
                                             const EthLinkCounterSnapshot *const absolute_snapshot) {
  const unsigned idx = (unsigned)(instance_number - 1U);
  const EthLinkCounterSnapshot *const baseline =
    s_baseline_valid[idx] ? &s_baseline[idx] : NULL;

  for(size_t i = 0; i < 11; ++i) {
    const CipUdint baseline_value =
      NULL != baseline ? baseline->interface_cntrs.cntr32[i] : 0U;
    g_ethernet_link[idx].interface_cntrs.cntr32[i] =
      CounterDelta(absolute_snapshot->interface_cntrs.cntr32[i], baseline_value);
  }

  for(size_t i = 0; i < 12; ++i) {
    const CipUdint baseline_value =
      NULL != baseline ? baseline->media_cntrs.cntr32[i] : 0U;
    g_ethernet_link[idx].media_cntrs.cntr32[i] =
      CounterDelta(absolute_snapshot->media_cntrs.cntr32[i], baseline_value);
  }
}

EipStatus EthLnkPreGetCallback
(
  CipInstance *const instance,
  CipAttributeStruct *const attribute,
  CipByte service
)
{
  (void)service;
  CipUint attr_no = attribute->attribute_number;
  /* ATTENTION: Array indices run from 0..(N-1), instance numbers from 1..N */
  CipInstanceNum inst_no  = instance->instance_number;
  if(4 != attr_no && 5 != attr_no) {
    return kEipStatusOk;
  }

  EthLinkCounterSnapshot snapshot;
  if(!ReadAbsoluteCounters(&snapshot)) {
    OPENER_TRACE_WARN("Eth Link PreCallback: failed to read interface counters\n");
    return kEipStatusOk;
  }
  s_last_absolute[inst_no - 1U] = snapshot;
  ApplyCounterSnapshotWithBaseline(inst_no, &snapshot);

  OPENER_TRACE_INFO(
    "Eth Link PreCallback: %s, i %" PRIu32 ", a %" PRIu16 ", s %" PRIu8 "\n",
    instance->cip_class->class_name,
    instance->instance_number,
    attribute->attribute_number,
    service);
  return kEipStatusOk;
}


EipStatus EthLnkPostGetCallback
(
  CipInstance *const instance,
  CipAttributeStruct *const attribute,
  CipByte service
)
{
  (void)service;
  CipInstanceNum inst_no = instance->instance_number;
  if (kEthLinkGetAndClear == (service & 0x7f)) {
    OPENER_TRACE_INFO(
      "Eth Link PostCallback: %s, i %" PRIu32 ", a %" PRIu16 ", s %" PRIu8 "\n",
      instance->cip_class->class_name,
      inst_no,
      attribute->attribute_number,
      service);
    /* Hardware counters are monotonic. Keep a local baseline so the object
     * reports zero-based values after GetAndClear. */
    switch (attribute->attribute_number) {
    case 4:
    case 5:
      s_baseline[inst_no - 1U] = s_last_absolute[inst_no - 1U];
      s_baseline_valid[inst_no - 1U] = true;
      break;
    default:
      OPENER_TRACE_INFO(
        "Wrong attribute number %" PRIu16 " in GetAndClear callback\n",
        attribute->attribute_number);
      break;
    }
  }
  return kEipStatusOk;
}
#endif /* ... && 0 != OPENER_ETHLINK_CNTRS_ENABLE */
