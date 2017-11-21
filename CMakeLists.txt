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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GVNC REQUIRED gvnc-1.0)
pkg_check_modules(GTK_VNC REQUIRED gtk-vnc-2.0)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)

if(NOT GTK_VNC_VERSION VERSION_LESS "0.7.0")
    add_definitions(-DGTK_VNC_HAVE_SMOOTH_SCALING)
endif()

include_directories(${GTK_VNC_INCLUDE_DIRS} ${GVNC_INCLUDE_DIRS})
include_directories(${GTKMM_INCLUDE_DIRS})

pkg_check_modules(GVNCPULSE gvncpulse-1.0)
if(GVNCPULSE_FOUND)
    add_definitions(-DHAVE_PULSEAUDIO)
    include_directories(${GVNCPULSE_INCLUDE_DIRS})
endif()

pkg_check_modules(LIBSSH REQUIRED libssh libssh_threads)
include_directories(${LIBSSH_INCLUDE_DIRS})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(gsshvnc_HEADERS
    appsettings.h
    sshtunnel.h
    vncconnectdialog.h
    vncdisplaymm.h
    vncgrabsequencemm.h
)

set(gsshvnc_SOURCES
    gsshvnc.cpp
    appsettings.cpp
    sshtunnel.cpp
    vncconnectdialog.cpp
    vncdisplaymm.cpp
    vncgrabsequencemm.cpp
)

add_executable(gsshvnc WIN32 ${gsshvnc_HEADERS} ${gsshvnc_SOURCES})
target_link_libraries(gsshvnc ${GTK_VNC_LIBRARIES} ${GVNC_LIBRARIES})
target_link_libraries(gsshvnc ${GTKMM_LIBRARIES})
if(GVNCPULSE_FOUND)
    target_link_libraries(gsshvnc ${GVNCPULSE_LIBRARIES})
endif()
target_link_libraries(gsshvnc ${LIBSSH_LIBRARIES})
target_link_libraries(gsshvnc Threads::Threads)
if(WIN32)
    target_link_libraries(gsshvnc ws2_32)
endif()

if(WIN32)
    find_program(ICOTOOL NAMES icotool)
    if(NOT ICOTOOL)
        message(FATAL_ERROR "icotool (from icoutils) is required on Windows")
    endif()

    find_path(
        ICON_THEME_PATH
        NAMES index.theme
        PATH_SUFFIXES share/icons/Adwaita
    )
    if (NOT ICON_THEME_PATH)
        message(FATAL_ERROR "Could not find an Adwaita-compatible icon theme")
    endif()

    add_custom_command(
        OUTPUT gsshvnc.ico
        COMMAND ${ICOTOOL} -c -o gsshvnc.ico
            ${ICON_THEME_PATH}/16x16/apps/preferences-desktop-remote-desktop.png
            ${ICON_THEME_PATH}/24x24/apps/preferences-desktop-remote-desktop.png
            ${ICON_THEME_PATH}/32x32/apps/preferences-desktop-remote-desktop.png
            ${ICON_THEME_PATH}/48x48/apps/preferences-desktop-remote-desktop.png
            ${ICON_THEME_PATH}/256x256/apps/preferences-desktop-remote-desktop.png
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
    add_custom_target(gsshvnc_icon DEPENDS gsshvnc.ico)
    add_dependencies(gsshvnc gsshvnc_icon)
    target_sources(gsshvnc PUBLIC win32/gsshvnc.rc)
endif()

install(TARGETS gsshvnc RUNTIME DESTINATION bin)
