/******************************************************************************
 * Copyright (c) 2019, Rockwell Automation, Inc.
 * All rights reserved.
 *
 * Contributors:
 *   2026-05-02: Adam G Sweeney <agsweeney@gmail.com> - Port integration updates.
 *
 *****************************************************************************/

#include "ethlinkcbs.h"

#include <stdbool.h>
#include <inttypes.h>

#include "cipethernetlink.h"
#include "trace.h"

static CipUdint iface_calls[OPENER_ETHLINK_INSTANCE_CNT];
static CipUdint media_calls[OPENER_ETHLINK_INSTANCE_CNT];

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
#define MAKE_CNTR(inst, attr, idx, cnt) ((10000000U * inst) + (100000U * attr) + (1000U * idx) + cnt)

EipStatus EthLnkPreGetCallback(
  CipInstance *const instance,
  CipAttributeStruct *const attribute,
  CipByte service) {
  bool had_action = true;
  EipStatus status = kEipStatusOk;
  CipUint attr_no = attribute->attribute_number;
  CipInstanceNum inst_no = instance->instance_number;
  unsigned idx = inst_no - 1;

  switch (attr_no) {
    case 4: {
      CipEthernetLinkInterfaceCounters *p_iface_cntrs = &g_ethernet_link[idx].interface_cntrs;
      ++iface_calls[idx];
      p_iface_cntrs->ul.in_octets = MAKE_CNTR(inst_no, attr_no, 0, iface_calls[idx]);
      p_iface_cntrs->ul.in_ucast = MAKE_CNTR(inst_no, attr_no, 1, iface_calls[idx]);
      p_iface_cntrs->ul.in_nucast = MAKE_CNTR(inst_no, attr_no, 2, iface_calls[idx]);
      p_iface_cntrs->ul.in_discards = MAKE_CNTR(inst_no, attr_no, 3, iface_calls[idx]);
      p_iface_cntrs->ul.in_errors = MAKE_CNTR(inst_no, attr_no, 4, iface_calls[idx]);
      p_iface_cntrs->ul.in_unknown_protos = MAKE_CNTR(inst_no, attr_no, 5, iface_calls[idx]);
      p_iface_cntrs->ul.out_octets = MAKE_CNTR(inst_no, attr_no, 6, iface_calls[idx]);
      p_iface_cntrs->ul.out_ucast = MAKE_CNTR(inst_no, attr_no, 7, iface_calls[idx]);
      p_iface_cntrs->ul.out_nucast = MAKE_CNTR(inst_no, attr_no, 8, iface_calls[idx]);
      p_iface_cntrs->ul.out_discards = MAKE_CNTR(inst_no, attr_no, 9, iface_calls[idx]);
      p_iface_cntrs->ul.out_errors = MAKE_CNTR(inst_no, attr_no, 10, iface_calls[idx]);
      break;
    }
    case 5: {
      CipEthernetLinkMediaCounters *p_media_cntrs = &g_ethernet_link[idx].media_cntrs;
      ++media_calls[idx];
      if (1 != media_calls[idx]) {
        p_media_cntrs->ul.align_errs = MAKE_CNTR(inst_no, attr_no, 0, media_calls[idx]);
        p_media_cntrs->ul.fcs_errs = MAKE_CNTR(inst_no, attr_no, 1, media_calls[idx]);
        p_media_cntrs->ul.single_coll = MAKE_CNTR(inst_no, attr_no, 2, media_calls[idx]);
        p_media_cntrs->ul.multi_coll = MAKE_CNTR(inst_no, attr_no, 3, media_calls[idx]);
        p_media_cntrs->ul.sqe_test_errs = MAKE_CNTR(inst_no, attr_no, 4, media_calls[idx]);
        p_media_cntrs->ul.def_trans = MAKE_CNTR(inst_no, attr_no, 5, media_calls[idx]);
        p_media_cntrs->ul.late_coll = MAKE_CNTR(inst_no, attr_no, 6, media_calls[idx]);
        p_media_cntrs->ul.exc_coll = MAKE_CNTR(inst_no, attr_no, 7, media_calls[idx]);
        p_media_cntrs->ul.mac_tx_errs = MAKE_CNTR(inst_no, attr_no, 8, media_calls[idx]);
        p_media_cntrs->ul.crs_errs = MAKE_CNTR(inst_no, attr_no, 9, media_calls[idx]);
        p_media_cntrs->ul.frame_too_long = MAKE_CNTR(inst_no, attr_no, 10, media_calls[idx]);
        p_media_cntrs->ul.mac_rx_errs = MAKE_CNTR(inst_no, attr_no, 11, media_calls[idx]);
      }
      break;
    }
    default:
      had_action = false;
      break;
  }

  if (had_action) {
    OPENER_TRACE_INFO("Eth Link PreCallback: %s, i %" PRIu32 ", a %" PRIu16 ", s %" PRIu8 "\n",
                      instance->cip_class->class_name,
                      instance->instance_number,
                      attribute->attribute_number,
                      service);
  }

  return status;
}

EipStatus EthLnkPostGetCallback(
  CipInstance *const instance,
  CipAttributeStruct *const attribute,
  CipByte service) {
  CipInstanceNum inst_no = instance->instance_number;
  EipStatus status = kEipStatusOk;

  if (kEthLinkGetAndClear == (service & 0x7f)) {
    switch (attribute->attribute_number) {
      case 4:
        iface_calls[inst_no - 1] = 0U;
        break;
      case 5:
        media_calls[inst_no - 1] = 0U;
        for (int idx = 0; idx < 12; ++idx) {
          g_ethernet_link[inst_no - 1].media_cntrs.cntr32[idx] = 0U;
        }
        break;
      default:
        OPENER_TRACE_INFO("Wrong attribute number %" PRIu16 " in GetAndClear callback\n",
                          attribute->attribute_number);
        break;
    }
  }

  return status;
}
#endif
