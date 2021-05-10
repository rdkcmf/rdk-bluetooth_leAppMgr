##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2018 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
SUBDIRS = 

INCLUDE_FILES =

AM_CPPFLAGS = -Wall -Werror -I${top_srcdir}/include -I${top_srcdir}/rpc ${RPC_FLAGS} -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmbus

AM_LDFLAGS = -ldbus-1  -lIARMBus
bin_PROGRAMS = btrLeMgrTest
btrLeMgrTest_SOURCES = btrLeMgr_test.cpp
