project(zbot)

set(SOURCES hirschberg.cpp
	zconfig.cpp
	zinit.cpp
	zworker.cpp
	zlogger.cpp
	zmsgbox.cpp
	zstorage.cpp
	ztbot.cpp
	zzabbix.cpp
	defaultconfig.cpp
	zmessagehistory.cpp
	zinstantsender.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_link_libraries(${PROJECT_NAME} TgBot)
install(TARGETS ${PROJECT_NAME}
	LIBRARY DESTINATION lib)
