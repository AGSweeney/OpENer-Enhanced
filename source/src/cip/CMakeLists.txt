#######################################
# CIP library                         #
#######################################

#######################################
# Add common includes                 #
#######################################
opener_common_includes()

#######################################
# Add platform-specific includes      #
#######################################
opener_platform_support("INCLUDES")

set( CIP_SRC appcontype.c cipassembly.c cipclass3connection.c cipcommon.c cipconnectionobject.c cipconnectionmanager.c cipdlr.c ciperror.h cipethernetlink.c cipidentity.c cipioconnection.c cipmessagerouter.c ciptcpipinterface.c ciptypes.h cipepath.c cipelectronickey.c cipstring.c cipstringi.c cipqos.c ciptypes.c)

if(OPENER_LLDP)
  list(APPEND CIP_SRC ciplldpmanagement.c ciplldpmanagement.h ciplldpdatatable.c ciplldpdatatable.h)
endif()

if(CIP_SECURITY_OBJECTS)
  list(APPEND CIP_SRC
    cip_security/cipsecurity.c
    cip_security/cipsecurity.h
    cip_security/ethernetipsecurity.c
    cip_security/ethernetipsecurity.h
    cip_security/certificatemanagement.c
    cip_security/certificatemanagement.h
  )
endif()

add_library( CIP ${CIP_SRC} )

if( OPENER_INSTALL_AS_LIB )
  target_link_libraries(CIP ENET_ENCAP)
  install(TARGETS CIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
  install(DIRECTORY ${CIP_SRC_DIR}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
   )
endif()
