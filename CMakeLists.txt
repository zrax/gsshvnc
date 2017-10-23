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

cmake_minimum_required(VERSION 3.2)
project(gsshvnc)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK_VNC REQUIRED gvnc-1.0 gtk-vnc-2.0)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)

include_directories(${GTK_VNC_INCLUDE_DIRS})
include_directories(${GTKMM_INCLUDE_DIRS})

set(gsshvnc_HEADERS
    vncdisplaymm.h
    vncgrabsequencemm.h
)

set(gsshvnc_SOURCES
    gvncviewer.cpp
    vncdisplaymm.cpp
    vncgrabsequencemm.cpp
)

add_executable(gsshvnc ${gsshvnc_HEADERS} ${gsshvnc_SOURCES})
target_link_libraries(gsshvnc ${GTK_VNC_LIBRARIES} ${GTKMM_LIBRARIES})
