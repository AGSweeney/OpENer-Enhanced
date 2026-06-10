/*******************************************************************************
 * RTEMS network handler platform hooks for OpENer.
 ******************************************************************************/

#include "networkhandler.h"

#include "opener_error.h"
#include "trace.h"
#include "opener_user_conf.h"

#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

MicroSeconds GetMicroSeconds(void) {
  struct timespec now = { .tv_nsec = 0, .tv_sec = 0 };

  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    return 0;
  }

  return (MicroSeconds)now.tv_nsec / 1000ULL +
         (MicroSeconds)now.tv_sec * 1000000ULL;
}

MilliSeconds GetMilliSeconds(void) {
  return (MilliSeconds)(GetMicroSeconds() / 1000ULL);
}

EipStatus NetworkHandlerInitializePlatform(void) {
  return kEipStatusOk;
}

void ShutdownSocketPlatform(int socket_handle) {
  if (0 != shutdown(socket_handle, SHUT_RDWR)) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR("Failed shutdown() socket %d - Error Code: %d - %s\n",
                     socket_handle,
                     error_code,
                     error_message);
    FreeErrorMessage(error_message);
  }
}

void CloseSocketPlatform(int socket_handle) {
  close(socket_handle);
}

int SetSocketToNonBlocking(int socket_handle) {
  return fcntl(socket_handle, F_SETFL, fcntl(socket_handle, F_GETFL, 0) | O_NONBLOCK);
}

int SetQosOnSocket(const int socket, CipUsint qos_value) {
  int set_tos = qos_value << 2;
  return setsockopt(socket, IPPROTO_IP, IP_TOS, &set_tos, sizeof(set_tos));
}
