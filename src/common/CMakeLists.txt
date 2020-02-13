project(zbot)

set(SOURCES hirschberg.cpp
	zconfig.cpp
	zinit.cpp
	zlogger.cpp
	zmsgbox.cpp
	zstorage.cpp
	ztbot.cpp
	defaultconfig.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_link_libraries(${PROJECT_NAME} TgBot)
install(TARGETS ${PROJECT_NAME}
	LIBRARY DESTINATION lib)
