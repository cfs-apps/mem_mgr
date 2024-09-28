/*
**  Copyright 2022 bitValence, Inc.
**  All Rights Reserved.
**
**  This program is free software; you can modify and/or redistribute it
**  under the terms of the GNU Affero General Public License
**  as published by the Free Software Foundation; version 3 with
**  attribution addendums as found in the LICENSE.txt
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU Affero General Public License for more details.
**
**  Purpose:
**    Define configurations for the MEM_MGR application
**
**  Notes:
**   1. These configurations should have an application scope and define
**      parameters that shouldn't need to change across deployments. If
**      a change is made to this file or any other app source file during
**      a deployment then the definition of the MEM_MGR_REV
**      macro in mem_mgr_platform_cfg.h should be updated.
**
*/
#ifndef _app_cfg_
#define _app_cfg_

/*
** Includes
*/

#include "app_c_fw.h"
#include "mem_mgr_platform_cfg.h"
#include "mem_mgr_eds_typedefs.h"


/******************************************************************************
** Versions
**
** 1.0 - Initial version, compatible with cFE Caelum
*/

#define  MEM_MGR_MAJOR_VER   1
#define  MEM_MGR_MINOR_VER   0


/******************************************************************************
** Init JSON file declarations
**
** - The MEM_FILE parameters are like FILE_MGR's file managenment parameters
**   so an effort has been made to keep the names and functions similar. The
**   term 'block' (vs other terms like segment) refers to a logically
**   contiguous set of bytes regardless of whether they are accessed in a
**   file or directly in memory.
*/

#define CFG_APP_CFE_NAME        APP_CFE_NAME
#define CFG_APP_PERF_ID         APP_PERF_ID

#define CFG_APP_CMD_PIPE_NAME   APP_CMD_PIPE_NAME
#define CFG_APP_CMD_PIPE_DEPTH  APP_CMD_PIPE_DEPTH

#define CFG_MEM_MGR_CMD_TOPICID          MEM_MGR_CMD_TOPICID
#define CFG_MEM_MGR_SEND_STATUS_TOPICID  BC_SCH_4_SEC_TOPICID        // Use different CFG_ name instead of BC_SCH_4_SEC_TOPICID to localize impact if rate changes 
#define CFG_MEM_MGR_STATUS_TLM_TOPICID   MEM_MGR_STATUS_TLM_TOPICID

#define CFG_MEM_FILE_LOAD_BLOCK_SIZE   MEM_FILE_LOAD_BLOCK_SIZE      // See MEM_FILE_IO_BLOCK_SIZE comments below
#define CFG_MEM_FILE_DUMP_BLOCK_SIZE   MEM_FILE_DUMP_BLOCK_SIZE      // See MEM_FILE_IO_BLOCK_SIZE comments below
#define CFG_MEM_FILE_FILL_BLOCK_SIZE   MEM_FILE_FILL_BLOCK_SIZE      // See MEM_FILE_IO_BLOCK_SIZE comments below

#define CFG_MEM_FILE_CFE_HDR_DESCR     MEM_FILE_CFE_HDR_DESCR
#define CFG_MEM_FILE_CFE_HDR_SUBTYPE   MEM_FILE_CFE_HDR_SUBTYPE
#define CFG_MEM_FILE_TASK_BLOCK_LIMIT  MEM_FILE_TASK_BLOCK_LIMIT
#define CFG_MEM_FILE_TASK_BLOCK_DELAY  MEM_FILE_TASK_BLOCK_DELAY

#define CFG_MEM_FILE_CHILD_NAME        MEM_FILE_CHILD_NAME
#define CFG_MEM_FILE_CHILD_STACK_SIZE  MEM_FILE_CHILD_STACK_SIZE
#define CFG_MEM_FILE_CHILD_PRIORITY    MEM_FILE_CHILD_PRIORITY
#define CFG_MEM_FILE_CHILD_PERF_ID     MEM_FILE_CHILD_PERF_ID

#define CFG_MEM_TLM_CHILD_NAME         MEM_TLM_CHILD_NAME
#define CFG_MEM_TLM_CHILD_STACK_SIZE   MEM_TLM_CHILD_STACK_SIZE
#define CFG_MEM_TLM_CHILD_PRIORITY     MEM_TLM_CHILD_PRIORITY
      


#define APP_CONFIG(XX) \
   XX(APP_CFE_NAME,char*) \
   XX(APP_PERF_ID,uint32) \
   XX(APP_CMD_PIPE_NAME,char*) \
   XX(APP_CMD_PIPE_DEPTH,uint32) \
   XX(MEM_MGR_CMD_TOPICID,uint32) \
   XX(BC_SCH_4_SEC_TOPICID,uint32) \
   XX(MEM_MGR_STATUS_TLM_TOPICID,uint32) \
   XX(MEM_FILE_LOAD_BLOCK_SIZE,uint32) \
   XX(MEM_FILE_DUMP_BLOCK_SIZE,uint32) \
   XX(MEM_FILE_FILL_BLOCK_SIZE,uint32) \
   XX(MEM_FILE_CFE_HDR_DESCR,char*) \
   XX(MEM_FILE_CFE_HDR_SUBTYPE,uint32) \
   XX(MEM_FILE_TASK_BLOCK_LIMIT,uint32) \
   XX(MEM_FILE_TASK_BLOCK_DELAY,uint32) \
   XX(MEM_FILE_CHILD_NAME,char*) \
   XX(MEM_FILE_CHILD_STACK_SIZE,uint32) \
   XX(MEM_FILE_CHILD_PRIORITY,uint32) \
   XX(MEM_FILE_CHILD_PERF_ID,uint32) \
   XX(MEM_TLM_CHILD_NAME,char*) \
   XX(MEM_TLM_CHILD_STACK_SIZE,uint32) \
   XX(MEM_TLM_CHILD_PRIORITY,uint32)

DECLARE_ENUM(Config,APP_CONFIG)


/******************************************************************************
** Event Macros
**
** Define the base event message IDs used by each object/component used by the
** application. There are no automated checks to ensure an ID range is not
** exceeded so it is the developer's responsibility to verify the ranges. 
*/

#define MEM_MGR_BASE_EID     (APP_C_FW_APP_BASE_EID +  0)
#define MEMORY_BASE_EID      (APP_C_FW_APP_BASE_EID + 20)
#define MEM_SIZE8_BASE_EID   (APP_C_FW_APP_BASE_EID + 30)
#define MEM_SIZE16_BASE_EID  (APP_C_FW_APP_BASE_EID + 40)
#define MEM_SIZE32_BASE_EID  (APP_C_FW_APP_BASE_EID + 50)
#define MEM_FILE_BASE_EID    (APP_C_FW_APP_BASE_EID + 60)


/******************************************************************************
** Example Object Table Macros
*/

#define EXOBJTBL_JSON_MAX_OBJ          10
#define EXOBJTBL_JSON_FILE_MAX_CHAR  2000 

/*
**
 *
 * This macro defines the maximum number of bytes that can be dumped
 * in an event message string based upon the setting of the
 * CFE_MISSION_EVS_MAX_MESSAGE_LENGTH configuration parameter.
 *
 * The event message format is:
 *    Message head "Memory Dump: "             13 characters
 *    Message body "0xFF "                      5 characters per dump byte
 *    Message tail "from address: 0xFFFFFFFF"  33 characters including NUL on 64-bit system
 */
#define MEMORY_DUMP_TOEVENT_HDR_STR      "Memory Dump: "
#define MEMORY_DUMP_TOEVENT_TRAILER_STR  "from address: %p"
#define MEMORY_DUMP_TOEVENT_MAX_BYTES    ((CFE_MISSION_EVS_MAX_MESSAGE_LENGTH - (13 + 33)) / 5)

    /*
    ** Allocate a dump buffer. It's declared this way to ensure it stays
    ** longword aligned since MM_MAX_DUMP_INEVENT_BYTES can be adjusted
    ** by changing the maximum event message string size.
    */

#define MEMORY_DUMP_TOEVENT_MAX_DWORDS   ((MEMORY_DUMP_TOEVENT_MAX_BYTES+3)/4)
  
/**
 * \brief Dump in an event scratch string size
 *
 * This macro defines the size of the scratch buffer used to build
 * the dump in event message string. Set it to the size of the
 * largest piece shown above including room for a NUL terminator.
 */
#define MEMORY_DUMP_TOEVENT_TEMP_CHARS 36  // TODO: Consider simple def like CFE_MISSION_EVS_MAX_MESSAGE_LENGTH - MEMORY_DUMP_TOEVENT_HDR_STR.  What is really being protected with a tight definition? Create risk of buffer overflow if get it wrong

/*
** This defines the maximum buffer size used for loads, dumps and fill commands. This buffer is used by the child task 
** and is part of the CPU load balancing scheme. A single buffer is defined because so the worst case child task CPU
** loading is bounded.
**
** The JSON init file provides individual load, dump and fill parameter definitions. This allows a finer leven of
** tuning. MEM_FILE_TASK_BLOCK_CNT, MEM_FILE_TASK_BLOCK_DELAY and MEM_FILE_CHILD_PRIORITY also impact performance. 
**
*/
#define MEM_FILE_IO_BLOCK_SIZE 2048

/******************************************************************************
** Function: MEM_MGR_strnlen
**
** Notes:
**   TODO: Original MM uses this function but it's local to 
**   TODO: os-shared-common.h in Basecamp's cFS
**
*/
static inline size_t MEM_MGR_strnlen(const char *s, size_t maxlen)
{
    const char *end = (const char *)memchr(s, 0, maxlen);
    if (end != NULL)
    {
        /* actual length of string is difference */
        maxlen = end - s;
    }
    return maxlen;
}

#endif /* _app_cfg_ */
