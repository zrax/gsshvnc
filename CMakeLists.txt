# This file is part of gsshvnc.
#
# gsshvnc is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# gsshvnc is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with gsshvnc.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.1)
project(gsshvnc)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# GCC 8 introduces these warnings which break gtkmm/sigc++
if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-cast-function-type -Wno-parentheses")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GVNC REQUIRED gvnc-1.0)
pkg_check_modules(GTK_VNC REQUIRED gtk-vnc-2.0)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)

if(NOT WIN32)
    add_definitions(-DUSE_LIBSECRET)
    pkg_check_modules(SECRET REQUIRED libsecret-1)
endif()

if(NOT GTK_VNC_VERSION VERSION_LESS "0.7.0")
    add_definitions(-DGTK_VNC_HAVE_SMOOTH_SCALING)
endif()

include_directories(${GTK_VNC_INCLUDE_DIRS} ${GVNC_INCLUDE_DIRS})
include_directories(${GTKMM_INCLUDE_DIRS})
if (NOT WIN32)
    include_directories(${SECRET_INCLUDE_DIRS})
endif()

pkg_check_modules(GVNCPULSE gvncpulse-1.0)
if(GVNCPULSE_FOUND)
    add_definitions(-DHAVE_PULSEAUDIO)
    include_directories(${GVNCPULSE_INCLUDE_DIRS})
endif()

pkg_check_modules(LIBSSH REQUIRED libssh)
pkg_check_modules(LIBSSH_THREADS libssh_threads)
include_directories(${LIBSSH_INCLUDE_DIRS})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(gsshvnc_HEADERS
    appsettings.h
    credstorage.h
    sshtunnel.h
    vncconnectdialog.h
    vncdisplaymm.h
    vncgrabsequencemm.h
)

set(gsshvnc_SOURCES
    gsshvnc.cpp
    appsettings.cpp
    credstorage.cpp
    sshtunnel.cpp
    vncconnectdialog.cpp
    vncdisplaymm.cpp
    vncgrabsequencemm.cpp
)

add_executable(gsshvnc WIN32 ${gsshvnc_HEADERS} ${gsshvnc_SOURCES})
target_link_libraries(gsshvnc ${GTK_VNC_LDFLAGS} ${GVNC_LDFLAGS})
target_link_libraries(gsshvnc ${GTKMM_LDFLAGS})
if(NOT WIN32)
    target_link_libraries(gsshvnc ${SECRET_LDFLAGS})
endif()
if(GVNCPULSE_FOUND)
    target_link_libraries(gsshvnc ${GVNCPULSE_LDFLAGS})
endif()
target_link_libraries(gsshvnc ${LIBSSH_LDFLAGS})
if(LIBSSH_THREADS_FOUND)
    target_link_libraries(gsshvnc ${LIBSSH_THREADS_LDFLAGS})
endif()
target_link_libraries(gsshvnc Threads::Threads)
if(WIN32)
    target_link_libraries(gsshvnc ws2_32)
endif()

if(WIN32)
    target_sources(gsshvnc PRIVATE win32/gsshvnc.rc)
endif()

install(TARGETS gsshvnc RUNTIME DESTINATION bin)
