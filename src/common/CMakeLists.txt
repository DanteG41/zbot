project(zbot)

set(SOURCES hirschberg.cpp
        zconfig.cpp
        zinit.cpp
        zlogger.cpp
        zmsgbox.cpp
        zstorage.cpp
        ztbot.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
