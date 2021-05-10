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
AM_CXXFLAGS = -Wall -Werror -I${top_srcdir}/include -I${top_srcdir}/include/btrmgrif -I${top_srcdir}/rpc -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmbus -I${top_srcdir}/rpc -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/wdmp-c  -I$(PKG_CONFIG_SYSROOT_DIR)${includedir} $(BREAKPAD_CFLAGS)

AM_CXXFLAGS += -I${top_srcdir}/rpc -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmbus -I${top_srcdir}/rpc 

AM_CXXFLAGS += "-std=c++11"
if WITH_RDK_LOGGER
AM_CXXFLAGS += $(RDK_LOGGER_FLAG)
endif 

AM_LDFLAGS = -lBTMgr $(CURL_LIBS)
AM_LDFLAGS += -ldbus-1  -lIARMBus
AM_LDFLAGS += $(LIBCJSON_LIBS) $(BREAKPAD_LFLAGS)
AM_LDFLAGS += -luuid

if WITH_RDK_LOGGER
AM_LDFLAGS += -lrdkloggers
endif

if BTR_LOCAL_OAUTH_SUPPORT
AM_CPPFLAGS = -DBTR_LOCAL_OAUTH_SUPPORT
#AM_LDFLAGS += -lssl -lcrypto
endif

AM_LDFLAGS += -lssl -lcrypto -lrfcapi -lpthread
#lib_LTLIBRARIES= libbtrLeMgr.la 
#libbtrLeMgr_la_SOURCES = btrLeMgr.cpp btrLeMgr_utils.cpp btrmgrif/btrMgrIf.cpp 

bin_PROGRAMS = btrLeAppMgr 
#btrLeAppMgr_LDADD = btrLeMgr.la
btrLeAppMgr_SOURCES = btrmgrif/btrMgrIf.cpp btrLeMgr_utils.cpp btrLeMgrBase.cpp btrTileMgr.cpp btrLeMgr.cpp ${top_srcdir}/rpc/lemgr_iarm_ext_if.cpp ${top_srcdir}/rpc/lemgr_iarm_int_if.cpp
include_HEADERS = $(top_srcdir)/rpc/lemgr_iarm_interface.h
