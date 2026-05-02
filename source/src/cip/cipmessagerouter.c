/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/
#include "opener_api.h"
#include "cipcommon.h"
#include "cipidentity.h"
#include "endianconv.h"
#include "ciperror.h"
#include "cipepath.h"
#include "trace.h"
#include "enipmessage.h"

#include "cipmessagerouter.h"

CipMessageRouterRequest g_message_router_request;

/** @brief A class registry list node
 *
 * A linked list of this  object is the registry of classes known to the message router
 * for small devices with very limited memory it could make sense to change this list into an
 * array with a given max size for removing the need for having to dynamically allocate
 * memory. The size of the array could be a parameter in the platform config file.
 */
typedef struct cip_message_router_object {
  struct cip_message_router_object *next; /**< link */
  CipClass *cip_class; /**< object */
} CipMessageRouterObject;

/** @brief Pointer to first registered object in MessageRouter*/
CipMessageRouterObject *g_first_object = NULL;

/** @brief Register a CIP Class to the message router
 *  @param cip_class Pointer to a class object to be registered.
 *  @return kEipStatusOk on success
 *          kEipStatusError on no memory available to register more objects
 */
EipStatus RegisterCipClass(CipClass *cip_class);

/** @brief Create Message Router Request structure out of the received data.
 *
 * Parses the UCMM header consisting of: service, IOI size, IOI, data into a request structure
 * @param data pointer to the message data received
 * @param data_length number of bytes in the message
 * @param message_router_request pointer to structure of MRRequest data item.
 * @return kEipStatusOk on success. otherwise kEipStatusError
 */
CipError CreateMessageRouterRequestStructure(const EipUint8 *data,
                                             size_t data_length,
                                             CipMessageRouterRequest *message_router_request);

static CipError ValidateUcmmElectronicKey(const EipUint8 *path_data,
                                          const size_t path_words) {
  size_t index = 0;
  const size_t path_bytes = path_words * sizeof(CipWord);
  while(index < path_bytes) {
    if( (index + 1 == path_bytes) && (0 == path_data[index]) ) {
      return kCipErrorSuccess;
    }
    const EipUint8 segment = path_data[index];
    if( (segment & 0xE0) != SEGMENT_TYPE_LOGICAL_SEGMENT) {
      /* Keep legacy behavior for non-logical segments in UCMM paths. */
      return kCipErrorSuccess;
    }
    const EipUint8 logical_type = segment & 0x1C;
    const EipUint8 logical_format = segment & 0x03;
    if(logical_type == LOGICAL_SEGMENT_TYPE_SPECIAL) {
      if(logical_format != LOGICAL_SEGMENT_SPECIAL_TYPE_FORMAT_ELECTRONIC_KEY) {
        return kCipErrorPathSegmentError;
      }
      if( (index + 10) > path_bytes) {
        return kCipErrorPathSegmentError;
      }
      if(path_data[index + 1] != ELECTRONIC_KEY_SEGMENT_KEY_FORMAT_4) {
        return kCipErrorPathSegmentError;
      }
      const CipUint vendor_id = (CipUint) path_data[index + 2] |
                                ( (CipUint) path_data[index + 3] << 8 );
      const CipUint device_type = (CipUint) path_data[index + 4] |
                                  ( (CipUint) path_data[index + 5] << 8 );
      const CipUint product_code = (CipUint) path_data[index + 6] |
                                   ( (CipUint) path_data[index + 7] << 8 );
      const CipUsint major_revision_compatibility = path_data[index + 8];
      const CipUsint major_revision = major_revision_compatibility & 0x7F;
      const bool compatibility_mode =
        (major_revision_compatibility & 0x80) == 0x80;
      const CipUsint minor_revision = path_data[index + 9];

      /* agsweeney@gmail.com: validate UCMM electronic key format 4 against identity */
      if( (vendor_id != 0) && (vendor_id != g_identity.vendor_id) ) {
        return kCipErrorPathSegmentError;
      }
      if( (product_code != 0) && (product_code != g_identity.product_code) ) {
        return kCipErrorPathSegmentError;
      }
      if( (device_type != 0) && (device_type != g_identity.device_type) ) {
        return kCipErrorPathSegmentError;
      }
      if(!compatibility_mode) {
        if( (major_revision != 0) && (major_revision != g_identity.revision.major_revision) ) {
          return kCipErrorPathSegmentError;
        }
        if( (minor_revision != 0) && (minor_revision != g_identity.revision.minor_revision) ) {
          return kCipErrorPathSegmentError;
        }
      } else {
        if( (major_revision != g_identity.revision.major_revision)
            || (minor_revision == 0)
            || (minor_revision > g_identity.revision.minor_revision) ) {
          return kCipErrorPathSegmentError;
        }
      }
      index += 10;
      continue;
    }

    size_t segment_size = 0;
    if(logical_type == LOGICAL_SEGMENT_TYPE_EXTENDED_LOGICAL) {
      switch(logical_format) {
        case LOGICAL_SEGMENT_FORMAT_EIGHT_BIT:
          segment_size = 3;
          break;
        case LOGICAL_SEGMENT_FORMAT_SIXTEEN_BIT:
          segment_size = 4;
          break;
        case LOGICAL_SEGMENT_FORMAT_THIRTY_TWO_BIT:
          segment_size = 6;
          break;
        default:
          return kCipErrorPathSegmentError;
      }
    } else {
      switch(logical_format) {
        case LOGICAL_SEGMENT_FORMAT_EIGHT_BIT:
          segment_size = 2;
          break;
        case LOGICAL_SEGMENT_FORMAT_SIXTEEN_BIT:
          segment_size = 4;
          break;
        case LOGICAL_SEGMENT_FORMAT_THIRTY_TWO_BIT:
          segment_size = 6;
          break;
        default:
          return kCipErrorPathSegmentError;
      }
    }
    if(index + segment_size > path_bytes) {
      return kCipErrorPathSegmentError;
    }
    index += segment_size;
  }
  return kCipErrorSuccess;
}

void InitializeCipMessageRouterClass(CipClass *cip_class) {

  CipClass *meta_class = cip_class->class_instance.cip_class;

  InsertAttribute( (CipInstance *) cip_class, 1, kCipUint, EncodeCipUint, NULL,
                   (void *) &cip_class->revision, kGetableSingleAndAll );                                                          /* revision */
  InsertAttribute( (CipInstance *) cip_class, 2, kCipUint, EncodeCipUint, NULL,
                   (void *) &cip_class->number_of_instances, kGetableSingle );                                                          /*  largest instance number */
  InsertAttribute( (CipInstance *) cip_class, 3, kCipUint, EncodeCipUint, NULL,
                   (void *) &cip_class->number_of_instances, kGetableSingle );                                                          /* number of instances currently existing*/
  InsertAttribute( (CipInstance *) cip_class, 4, kCipUint, EncodeCipUint, NULL,
                   (void *) &kCipUintZero, kGetableAll );                                                          /* optional attribute list - default = 0 */
  InsertAttribute( (CipInstance *) cip_class, 5, kCipUint, EncodeCipUint, NULL,
                   (void *) &kCipUintZero, kGetableAll );                                                          /* optional service list - default = 0 */
  InsertAttribute( (CipInstance *) cip_class, 6, kCipUint, EncodeCipUint, NULL,
                   (void *) &meta_class->highest_attribute_number,
                   kGetableSingleAndAll );                                                                                                          /* max class attribute number*/
  InsertAttribute( (CipInstance *) cip_class, 7, kCipUint, EncodeCipUint, NULL,
                   (void *) &cip_class->highest_attribute_number,
                   kGetableSingleAndAll );                                                                                                         /* max instance attribute number*/

  InsertService(meta_class,
                kGetAttributeAll,
                &GetAttributeAll,
                "GetAttributeAll");                                                 /* bind instance services to the metaclass*/
  InsertService(meta_class,
                kGetAttributeSingle,
                &GetAttributeSingle,
                "GetAttributeSingle");
}

EipStatus CipMessageRouterInit() {

  CipClass *message_router = CreateCipClass(kCipMessageRouterClassCode, /* class code */
                                            7, /* # of class attributes */
                                            7, /* # highest class attribute number */
                                            2, /* # of class services */
                                            0, /* # of instance attributes */
                                            0, /* # highest instance attribute number */
                                            1, /* # of instance services */
                                            1, /* # of instances */
                                            "message router", /* class name */
                                            1, /* # class revision*/
                                            InitializeCipMessageRouterClass); /* # function pointer for initialization*/
  if(NULL == message_router) {
    return kEipStatusError;
  }
  InsertService(message_router,
                kGetAttributeSingle,
                &GetAttributeSingle,
                "GetAttributeSingle");

  /* reserved for future use -> set to zero */
  return kEipStatusOk;
}

/** @brief Get the registered MessageRouter object corresponding to ClassID.
 *  given a class ID, return a pointer to the registration node for that object
 *
 *  @param class_id Class code to be searched for.
 *  @return Pointer to registered message router object
 *      NULL .. Class not registered
 */
CipMessageRouterObject *GetRegisteredObject(EipUint32 class_id) {
  CipMessageRouterObject *object = g_first_object; /* get pointer to head of class registration list */

  while(NULL != object) /* for each entry in list*/
  {
    OPENER_ASSERT(NULL != object->cip_class);
    if(object->cip_class->class_code == class_id) {
      return object; /* return registration node if it matches class ID*/
    }
    object = object->next;
  }
  return NULL;
}

CipClass *GetCipClass(const CipUdint class_code) {
  CipMessageRouterObject *message_router_object =
    GetRegisteredObject(class_code);

  if(message_router_object) {
    return message_router_object->cip_class;
  } else {
    return NULL;
  }
}

CipInstance *GetCipInstance(const CipClass *RESTRICT const cip_class,
                            const CipInstanceNum instance_number) {

  if(instance_number == 0) {
    return (CipInstance *) cip_class; /* if the instance number is zero, return the class object itself*/

  }
  /* pointer to linked list of instances from the class object*/
  for(CipInstance *instance = cip_class->instances; instance;
      instance = instance->next)                                                         /* follow the list*/
  {
    if(instance->instance_number == instance_number) {
      return instance; /* if the number matches, return the instance*/
    }
  }

  return NULL;
}

EipStatus RegisterCipClass(CipClass *cip_class) {
  CipMessageRouterObject **message_router_object = &g_first_object;

  while(*message_router_object) {
    message_router_object = &(*message_router_object)->next; /* follow the list until p points to an empty link (list end)*/

  }
  *message_router_object =
    (CipMessageRouterObject *) CipCalloc(1, sizeof(CipMessageRouterObject) );                      /* create a new node at the end of the list*/
  if(*message_router_object == 0) {
    return kEipStatusError; /* check for memory error*/

  }
  (*message_router_object)->cip_class = cip_class; /* fill in the new node*/
  (*message_router_object)->next = NULL;

  return kEipStatusOk;
}

EipStatus NotifyMessageRouter(EipUint8 *data,
                              int data_length,
                              CipMessageRouterResponse *message_router_response,
                              const struct sockaddr *const originator_address,
                              const CipSessionHandle encapsulation_session) {
  EipStatus eip_status = kEipStatusOkSend;
  CipError status = kCipErrorSuccess;

  /* agsweeney@gmail.com: reject malformed or truncated CIP payload lengths early */
  if(data_length <= 0) {
    message_router_response->general_status = kCipErrorNotEnoughData;
    message_router_response->size_of_additional_status = 0;
    message_router_response->reserved = 0;
    message_router_response->reply_service = 0x80;
    return kEipStatusOkSend;
  }

  OPENER_TRACE_INFO("NotifyMessageRouter: routing unconnected message\n");
  if(kCipErrorSuccess !=
     (status =
        CreateMessageRouterRequestStructure(data, (size_t)data_length,
                                            &g_message_router_request) ) ) {                                             /* error from create MR structure*/
    OPENER_TRACE_ERR(
      "NotifyMessageRouter: error from createMRRequeststructure\n");
    message_router_response->general_status = status;
    message_router_response->size_of_additional_status = 0;
    message_router_response->reserved = 0;
    message_router_response->reply_service =
      (0x80 | g_message_router_request.service);
  } else {
    /* forward request to appropriate Object if it is registered*/
    CipMessageRouterObject *registered_object = GetRegisteredObject(
      g_message_router_request.request_path.class_id);
    if(registered_object == 0) {
      OPENER_TRACE_ERR(
        "NotifyMessageRouter: sending CIP_ERROR_OBJECT_DOES_NOT_EXIST reply, class id 0x%x is not registered\n",
        (unsigned ) g_message_router_request.request_path.class_id);
      message_router_response->general_status = kCipErrorPathDestinationUnknown; /*according to the test tool this should be the correct error flag instead of CIP_ERROR_OBJECT_DOES_NOT_EXIST;*/
      message_router_response->size_of_additional_status = 0;
      message_router_response->reserved = 0;
      message_router_response->reply_service =
        (0x80 | g_message_router_request.service);
    } else {
      /* call notify function from Object with ClassID (gMRRequest.RequestPath.ClassID)
         object will or will not make an reply into gMRResponse*/
      message_router_response->reserved = 0;
      OPENER_ASSERT(NULL != registered_object->cip_class); OPENER_TRACE_INFO(
        "NotifyMessageRouter: calling notify function of class '%s'\n",
        registered_object->cip_class->class_name);
      eip_status = NotifyClass(registered_object->cip_class,
                               &g_message_router_request,
                               message_router_response,
                               originator_address,
                               encapsulation_session);

#ifdef OPENER_TRACE_ENABLED
      if (eip_status == kEipStatusError) {
        OPENER_TRACE_ERR(
          "notifyMR: notify function of class '%s' returned an error\n",
          registered_object->cip_class->class_name);
      } else if (eip_status == kEipStatusOk) {
        OPENER_TRACE_INFO(
          "notifyMR: notify function of class '%s' returned no reply\n",
          registered_object->cip_class->class_name);
      } else {
        OPENER_TRACE_INFO(
          "notifyMR: notify function of class '%s' returned a reply\n",
          registered_object->cip_class->class_name);
      }
#endif
    }
  }
  return eip_status;
}

CipError CreateMessageRouterRequestStructure(const EipUint8 *data,
                                             size_t data_length,
                                             CipMessageRouterRequest *message_router_request)
{
  if(0 == data_length) {
    return kCipErrorNotEnoughData;
  }
  message_router_request->service = *data;
  data++;
  data_length--;
  if(0 == data_length) {
    return kCipErrorNotEnoughData;
  }
  /* agsweeney@gmail.com: validate declared EPATH byte size before decode */
  const size_t declared_path_size = (size_t) data[0] * sizeof(CipWord) + 1;
  if(declared_path_size > data_length) {
    return kCipErrorPathSizeInvalid;
  }
  const CipError electronic_key_check =
    ValidateUcmmElectronicKey(data + 1, data[0]);
  if(kCipErrorSuccess != electronic_key_check) {
    return electronic_key_check;
  }

  size_t number_of_decoded_bytes;
  const EipStatus path_result =
    DecodePaddedEPath(&(message_router_request->request_path),
                      &data,
                      declared_path_size,
                      &number_of_decoded_bytes);
  if(path_result != kEipStatusOk) {
    return kCipErrorPathSegmentError;
  }

  if(number_of_decoded_bytes != declared_path_size) {
    return kCipErrorPathSizeInvalid;
  } else {
    message_router_request->data = data;
    message_router_request->request_data_size = data_length - declared_path_size;
    return kCipErrorSuccess;
  }
}

void DeleteAllClasses(void) {
  CipMessageRouterObject *message_router_object = g_first_object; /* get pointer to head of class registration list */
  CipMessageRouterObject *message_router_object_to_delete = NULL;
  CipInstance *instance = NULL;
  CipInstance *instance_to_delete = NULL;

  while(NULL != message_router_object) {
    message_router_object_to_delete = message_router_object;
    message_router_object = message_router_object->next;

    instance = message_router_object_to_delete->cip_class->instances;
    while(NULL != instance) {
      instance_to_delete = instance;
      instance = instance->next;
      if(message_router_object_to_delete->cip_class->number_of_attributes) /* if the class has instance attributes */
      { /* then free storage for the attribute array */
        CipFree(instance_to_delete->attributes);
      }
      CipFree(instance_to_delete);
    }

    /* free meta class data*/
    CipClass *meta_class =
      message_router_object_to_delete->cip_class->class_instance.cip_class;
    CipFree(meta_class->class_name);
    CipFree(meta_class->services);
    CipFree(meta_class->get_single_bit_mask);
    CipFree(meta_class->set_bit_mask);
    CipFree(meta_class->get_all_bit_mask);
    CipFree(meta_class);

    /* free class data*/
    CipClass *cip_class = message_router_object_to_delete->cip_class;
    CipFree(cip_class->class_name);
    CipFree(cip_class->get_single_bit_mask);
    CipFree(cip_class->set_bit_mask);
    CipFree(cip_class->get_all_bit_mask);
    CipFree(cip_class->class_instance.attributes);
    CipFree(cip_class->services);
    CipFree(cip_class);
    /* free message router object */
    CipFree(message_router_object_to_delete);
  }
  g_first_object = NULL;
}
