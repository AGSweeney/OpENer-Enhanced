/*******************************************************************************
 * RTEMS error helpers for OpENer.
 ******************************************************************************/

#undef _GNU_SOURCE

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "opener_error.h"

const size_t kErrorMessageBufferSize = 255U;

int GetSocketErrorNumber(void) {
  return errno;
}

char *GetErrorMessage(int error_number) {
  char *error_message = malloc(kErrorMessageBufferSize);
  if (error_message != NULL) {
    strerror_r(error_number, error_message, kErrorMessageBufferSize);
  }
  return error_message;
}

void FreeErrorMessage(char *error_message) {
  free(error_message);
}
