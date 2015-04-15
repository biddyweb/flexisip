
set(FLEXISIP_LIBS ${ORTP_LIBRARIES} ${SOFIA_SIP_UA_LIBRARIES})

set(FLEXISIP_INCLUDES)

set(FLEXISIP_SOURCES
			main.cc
			flexisip-exception.cc flexisip-exception.hh 
			agent.cc agent.hh 
			common.cc common.hh 
			sdp-modifier.hh  sdp-modifier.cc 
			callstore.hh  callstore.cc 
			forkcontext.hh forkcontext.cc 
			callcontext-mediarelay.hh  callcontext-mediarelay.cc 
			forkcallcontext.hh  forkcallcontext.cc 
			forkmessagecontext.hh  forkmessagecontext.cc 
			forkbasiccontext.cc forkbasiccontext.hh 
			registrardb-internal.cc registrardb-internal.hh registrardb.cc registrardb.hh 
			recordserializer-c.cc recordserializer.hh 
			recordserializer-json.cc cJSON.c cJSON.h 
			etchosts.cc etchosts.hh 
			lpconfig.cc lpconfig.h 
			configmanager.cc configmanager.hh
			event.cc event.hh 
			transaction.cc transaction.hh 
			module.cc module.hh 
			monitor.cc monitor.hh 
			entryfilter.cc entryfilter.hh 
			stun.cc stun.hh 
			stun/stun.c stun/stun_udp.c stun/flexisip_stun.h stun/flexisip_stun_udp.h 
			mediarelay.cc mediarelay.hh 
			authdb.hh authdb.cc authdb-file.cc 
			module-sanitychecker.cc 
			module-garbage-in.cc 
			module-forward.cc 
			module-gatewayadapter.cc 
			module-contact-route-inserter.cc 
			module-nat-helper.cc 
			module-registrar.cc 
			module-router.cc lateforkapplier.hh 
			module-transcode.cc 
			module-statistics-collector.cc 
			module-mediarelay.cc 
			module-auth.cc 
			module-loadbalancer.cc 
			dos-protection.cc dos-protection.hh 
			expressionparser.cc expressionparser.hh 
			sipattrextractor.cc sipattrextractor.hh 
			h264iframefilter.cc h264iframefilter.hh 
			telephone-event-filter.cc telephone-event-filter.hh 
			log/logmanager.cc log/logmanager.hh 
			eventlogs/eventlogs.cc eventlogs/eventlogs.hh 
			contact-masquerader.cc contact-masquerader.hh 
			uac-register.cc uac-register.hh 
			module-redirect.cc
# TODO: GIT VERSION FILE
)

if(ENABLE_ODB)
	#list(APPEND FLEXISIP_LIBS ${ODB_LIBRARIES})
	#list(APPEND FLEXISIP_INCLUDES ${ODB_INCLUDE_DIRS})
	#list(APPEND FLEXISIP_SOURCES eventlogs/eventlogsdb.cc eventlogs/eventlogsdb.hh)
	# TODO: generate the cxx and hxx with ODB
#eventlogs/eventlogsdb-odb.hxx eventlogs/eventlogsdb-odb.cxx: eventlogs/eventlogsdb.hh
#	odb $(SOFIA_CFLAGS) $(ORTP_CFLAGS) --std c++11 -d mysql --output-dir eventlogs --generate-query --generate-schema --schema-format embedded $<
endif()


if(TOTO)
	list(APPEND FLEXISIP_SOURCES LOL)
	list(APPEND FLEXISIP_LIBS ${TOTO_LIBRARIES} )
	list(APPEND FLEXISIP_INCLUDES ${TOTO_INCLUDE_DIRS})
endif()
  

if(ENABLE_SNMP)
	list(APPEND FLEXISIP_SOURCES snmp-agent.cc snmp-agent.h)
	list(APPEND FLEXISIP_LIBS ${NET_SNMP_LIBRARIES} )
	list(APPEND FLEXISIP_INCLUDES ${NET_SNMP_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/mib )
endif()

if(ENABLE_ODBC)
	add_definitions(-DENABLE_ODBC)
	list(APPEND FLEXISIP_SOURCES authdb-odbc.cc)
	list(APPEND FLEXISIP_LIBS ${ODBC_LIBRARIES} )
	list(APPEND FLEXISIP_INCLUDES ${ODBC_INCLUDE_DIRS})
endif()

if(ENABLE_TRANSCODER)
	list(APPEND FLEXISIP_SOURCES callcontext-transcoder.cc callcontext-transcoder.hh )
	list(APPEND FLEXISIP_LIBS ${MEDIASTREAMER2_LIBRARIES} )
	list(APPEND FLEXISIP_INCLUDES ${MEDIASTREAMER2_INCLUDE_DIRS})
endif()

if(ENABLE_DATEHANDLER)
	list(APPEND FLEXISIP_SOURCES module-datehandler.cc)
endif()

if(ENABLE_HIREDIS)
	list(APPEND FLEXISIP_SOURCES registrardb-redis-async.cc registrardb-redis.hh registrardb-redis-sofia-event.h)
	list(APPEND FLEXISIP_LIBS ${HIREDIS_LIBRARIES} )
	list(APPEND FLEXISIP_INCLUDES ${HIREDIS_INCLUDE_DIRS})
	add_definitions(-DENABLE_REDIS)
endif()

if(ENABLE_PROTOBUF)
	# TODO: generate the protobuf wrapper
	# protoc --proto_path=$(builddir) --cpp_out=$(builddir) $(pb_files)
	set(PROTOBUF_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/recordserializer-protobuf.proto)

	list(APPEND FLEXISIP_SOURCES recordserializer-protobuf.cc)
	list(APPEND FLEXISIP_LIBS ${PROTOBUF_LIBRARIES} )
	list(APPEND FLEXISIP_INCLUDES ${PROTOBUF_INCLUDE_DIRS})
	add_definitions(-DENABLE_PROTOBUF)
endif()

if(ENABLE_PRESENCE)
	list(APPEND FLEXISIP_SOURCES module-presence.cc)
	list(APPEND FLEXISIP_LIBS ${BELLESIP_LIBRARIES} ${XercesLibrary} )
	list(APPEND FLEXISIP_INCLUDES ${BELLESIP_INCLUDE_DIRS})
	add_definitions(-DBELLE_SIP_USE_STL ${BELLE_SIP_CFLAGS})
endif()

if(ENABLE_PUSHNOTIFICATION)
	list(APPEND FLEXISIP_SOURCES module-pushnotification.cc 
		pushnotification/pushnotification.cc pushnotification/pushnotification.hh 
		pushnotification/pushnotificationservice.cc pushnotification/pushnotificationservice.hh
		pushnotification/pushnotificationclient.cc pushnotification/pushnotificationclient.hh)
	list(APPEND FLEXISIP_LIBS ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
	list(APPEND FLEXISIP_INCLUDES ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})
endif()

message(STATUS "Flexisip LIBS : ${FLEXISIP_LIBS}")
message(STATUS "Flexisip INCLUDES : ${FLEXISIP_INCLUDES}")

add_executable( flexisip ${FLEXISIP_SOURCES} )
target_link_libraries(flexisip ${FLEXISIP_LIBS})
target_include_directories(flexisip PUBLIC ${FLEXISIP_INCLUDES})