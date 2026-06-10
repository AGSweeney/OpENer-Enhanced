/*******************************************************************************
 * RTEMS libbsd network configuration for OpENer (POSIX-compatible API).
 ******************************************************************************/

#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "cipstring.h"
#include "networkconfig.h"
#include "trace.h"
#include "opener_api.h"

EipStatus IfaceGetMacAddress(const char *iface,
                             uint8_t *const physical_address) {
  struct ifaddrs *ifap = NULL;
  struct ifaddrs *ifa = NULL;
  EipStatus status = kEipStatusError;

  if (getifaddrs(&ifap) != 0) {
    return kEipStatusError;
  }

  for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
    const struct sockaddr_dl *sdl;

    if ((ifa->ifa_addr == NULL) || (ifa->ifa_name == NULL)) {
      continue;
    }
    if (strcmp(ifa->ifa_name, iface) != 0) {
      continue;
    }
    if (ifa->ifa_addr->sa_family != AF_LINK) {
      continue;
    }

    sdl = (const struct sockaddr_dl *)ifa->ifa_addr;
    if (sdl->sdl_alen >= 6U) {
      memcpy(physical_address, LLADDR(sdl), 6);
      status = kEipStatusOk;
      break;
    }
  }

  freeifaddrs(ifap);
  return status;
}

static EipStatus GetIpAndNetmaskFromInterface(const char *iface,
                                              CipTcpIpInterfaceConfiguration *iface_cfg) {
  struct ifaddrs *ifap = NULL;
  struct ifaddrs *ifa = NULL;
  EipStatus status = kEipStatusError;

  if (getifaddrs(&ifap) != 0) {
    return kEipStatusError;
  }

  for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
    const struct sockaddr_in *sin;

    if ((ifa->ifa_addr == NULL) || (ifa->ifa_name == NULL)) {
      continue;
    }
    if (strcmp(ifa->ifa_name, iface) != 0) {
      continue;
    }
    if (ifa->ifa_addr->sa_family != AF_INET) {
      continue;
    }

    sin = (const struct sockaddr_in *)ifa->ifa_addr;
    iface_cfg->ip_address = sin->sin_addr.s_addr;
    if (ifa->ifa_netmask != NULL) {
      const struct sockaddr_in *mask = (const struct sockaddr_in *)ifa->ifa_netmask;
      iface_cfg->network_mask = mask->sin_addr.s_addr;
    }
    status = kEipStatusOk;
    break;
  }

  freeifaddrs(ifap);
  return status;
}

EipStatus IfaceGetConfiguration(const char *iface,
                                CipTcpIpInterfaceConfiguration *iface_cfg) {
  CipTcpIpInterfaceConfiguration local_cfg;
  EipStatus status;

  memset(&local_cfg, 0, sizeof local_cfg);
  status = GetIpAndNetmaskFromInterface(iface, &local_cfg);
  if (kEipStatusOk == status) {
    ClearCipString(&iface_cfg->domain_name);
    *iface_cfg = local_cfg;
  }
  return status;
}

EipStatus IfaceWaitForIp(const char *const iface,
                         int timeout,
                         volatile int *const p_abort_wait) {
  (void)iface;
  (void)timeout;
  (void)p_abort_wait;
  return kEipStatusOk;
}

void GetHostName(CipString *hostname) {
  SetCipStringByCstr(hostname, "LeapOS-Gateway");
}
