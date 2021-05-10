/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#ifndef __BTR_LE_MGR_LOGGER_H__
#define __BTR_LE_MGR_LOGGER_H__


#define PREFIX(format)  "%d\t: [%s] " format

#ifdef RDK_LOGGER_ENABLED

#include "rdk_debug.h"

#define LOG_ERROR(format, ...)            RDK_LOG(RDK_LOG_ERROR,  "LOG.RDK.BTRLEMGR", format, __VA_ARGS__)
#define LOG_WARN(format,  ...)            RDK_LOG(RDK_LOG_WARN,   "LOG.RDK.BTRLEMGR", format, __VA_ARGS__)
#define LOG_INFO(format,  ...)            RDK_LOG(RDK_LOG_INFO,   "LOG.RDK.BTRLEMGR", format, __VA_ARGS__)
#define LOG_DEBUG(format, ...)            RDK_LOG(RDK_LOG_DEBUG,  "LOG.RDK.BTRLEMGR", format, __VA_ARGS__)
#define LOG_TRACE(format, ...)            RDK_LOG(RDK_LOG_TRACE1, "LOG.RDK.BTRLEMGR", format, __VA_ARGS__)

#else // #ifdef RDK_LOGGER_ENABLED

#include <stdio.h>

#define LOG_ERROR(format, ...)            fprintf(stderr, format, __VA_ARGS__)
#define LOG_WARN(format,  ...)            fprintf(stderr, format, __VA_ARGS__)
#define LOG_INFO(format,  ...)            fprintf(stderr, format, __VA_ARGS__)
#define LOG_DEBUG(format, ...)            fprintf(stderr, format, __VA_ARGS__)
#define LOG_TRACE(format, ...)            fprintf(stderr, format, __VA_ARGS__)

#endif // #ifdef RDK_LOGGER_ENABLED


#define BTRLEMGRLOG_ERROR(format, ...)       LOG_ERROR(PREFIX(format), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define BTRLEMGRLOG_WARN(format,  ...)       LOG_WARN(PREFIX(format),  __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define BTRLEMGRLOG_INFO(format,  ...)       LOG_INFO(PREFIX(format),  __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define BTRLEMGRLOG_DEBUG(format, ...)       LOG_DEBUG(PREFIX(format), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define BTRLEMGRLOG_TRACE(format, ...)       LOG_TRACE(PREFIX(format), __LINE__, __FUNCTION__, ##__VA_ARGS__)


#endif /* __BTR_LE_MGR_LOGGER_H__ */
