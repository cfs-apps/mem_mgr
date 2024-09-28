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
**    Define methods for managing files
**
**  Notes:
**    1. Command and telemetry packets are defined in EDS file filemgr.xml.
**
*/

#ifndef _mem_file_
#define _mem_file_

/*
** Includes
*/

#include "app_cfg.h"
#include "memory.h"

/***********************/
/** Macro Definitions **/
/***********************/


/*
** Event Message IDs
*/

#define MEM_FILE_CONSTRUCTOR_EID        (MEM_FILE_BASE_EID + 0)
#define MEM_FILE_DUMP_CMD_EID           (MEM_FILE_BASE_EID + 1)
#define MEM_FILE_DUMP_SYM_TBL_CMD_EID   (MEM_FILE_BASE_EID + 2)
#define MEM_FILE_LOAD_CMD_EID           (MEM_FILE_BASE_EID + 3)
#define MEM_FILE_PROCESS_LOAD_FILE_EID  (MEM_FILE_BASE_EID + 4)
#define MEM_FILE_COMPUTE_FILE_CRC_EID   (MEM_FILE_BASE_EID + 5)
#define MEM_FILE_CREATE_DUMP_FILE_EID   (MEM_FILE_BASE_EID + 6)
#define MEM_FILE_DUMP_MEM_TO_FILE_EID   (MEM_FILE_BASE_EID + 7)
#define MEM_FILE_LOAD_MEM_FROM_FILE_EID (MEM_FILE_BASE_EID + 8)
#define MEM_FILE_VALID_LOAD_FILE_EID    (MEM_FILE_BASE_EID + 9)

/**********************/
/** Type Definitions **/
/**********************/

        
/******************************************************************************
** MEM_FILE_Class
*/

typedef struct
{

   /*
   ** App Framework References
   */
   
   const INITBL_Class_t *IniTbl;
   
   /*
   ** MEM_FILE State Data
   */
      
   MEMORY_CmdStatus_t CmdStatus;

   uint16 TaskBlockCount;
   uint32 TaskBlockLimit;
   uint32 TaskBlockDelay;
   uint32 TaskPerfId;
   
   uint32 LoadBlockSize;
   uint32 DumpBlockSize;
   uint32 FillBlockSize;
   
   char   Filename[OS_MAX_PATH_LEN];
   uint8  IoBuf[MEM_FILE_IO_BLOCK_SIZE];
   
} MEM_FILE_Class_t;


/******************************************************************************
** File Structure
*/


/************************/
/** Exported Functions **/
/************************/


/******************************************************************************
** Function: MEM_FILE_Constructor
**
** Initialize the example object to a known state
**
** Notes:
**   1. This must be called prior to any other function.
**
*/
void MEM_FILE_Constructor(MEM_FILE_Class_t *MemFilePtr, const INITBL_Class_t *IniTbl);


/******************************************************************************
** Function: MEM_FILE_DumpCmd
**
*/
bool MEM_FILE_DumpCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: MEM_FILE_DumpSymTblCmd
**
*/
bool MEM_FILE_DumpSymTblCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: MEM_FILE_LoadCmd
**
*/
bool MEM_FILE_LoadCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function:  MEM_FILE_ResetStatus
**
*/
void MEM_FILE_ResetStatus(void);


#endif /* _mem_file_ */
