/*
** Copyright 2022 bitValence, Inc.
** All Rights Reserved.
**
** This program is free software; you can modify and/or redistribute it
** under the terms of the GNU Affero General Public License
** as published by the Free Software Foundation; version 3 with
** attribution addendums as found in the LICENSE.txt
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Affero General Public License for more details.
**
** Purpose:
**   Manage memory dwell JSON table
**
** Notes:
**    1. Use the Singleton design pattern. A pointer to the table object
**       is passed to the constructor and saved for all other operations.
**       This is a table-specific file so it doesn't need to be re-entrant.
**    2. Tightly coupled to MEM_MGR_DWELL_ID_CNT so code changes are required
**       if that parameter changes. 
**
*/

#ifndef _mem_dwell_tbl_
#define _mem_dwell_tbl_

/*
** Includes
*/

#include "app_c_fw.h"
#include "app_cfg.h"

/***********************/
/** Macro Definitions **/
/***********************/


#define JSON_MAX_KW_LEN  16  // Maximum length of short keywords

#define MEM_DWELL_TBL_PTR(var,id) (&var[id-1]) // Access array of MEM_DWELL_TBL_Dwell_t using dwell IDs


/*
** Event Message IDs
*/

#define MEM_DWELL_TBL_LOAD_EID        (MEM_DWELL_TBL_BASE_EID + 0)
#define MEM_DWELL_TBL_VALID_EID       (MEM_DWELL_TBL_BASE_EID + 1)
#define MEM_DWELL_TBL_DUMP_EID        (MEM_DWELL_TBL_BASE_EID + 2)
#define MEM_DWELL_TBL_LOAD_ENTRY_EID  (MEM_DWELL_TBL_BASE_EID + 3) //TODO: Need?

/**********************/
/** Type Definitions **/
/**********************/


/******************************************************************************
** Message Table -  Local table copy used for table loads
** 
*/

typedef struct
{
   
   uint16  Delay;
   MEM_MGR_SymbolAddr_t    SymbolAddr;
   MEM_MGR_MemSize_Enum_t  MemSize;
   MEMORY_VerifiedMemory_t VerifiedMemory;  // Address derived/validated from Symbol Address + Offset

} MEM_DWELL_TBL_Dwell_Entry_t;


typedef struct
{
   uint16  Id;
   uint16  TopicId;
   char    EnaStr[JSON_MAX_KW_LEN];
   bool    Enabled;
   MEM_MGR_DwellName_String_t   Name;  
   MEM_DWELL_TBL_Dwell_Entry_t Entry[MEM_MGR_DWELL_ENTRIES];

} MEM_DWELL_TBL_Dwell_t;

/*
** Table load callback function
*/
typedef bool (*MEM_DWELL_TBL_OwnerAcceptFunc_t)(const MEM_DWELL_TBL_Dwell_t DwellTbl[MEM_MGR_DWELL_ID_CNT]);

typedef struct
{

   /*
   ** Table parameter data
   */
   
   MEM_DWELL_TBL_Dwell_t            Dwell[MEM_MGR_DWELL_ID_CNT];
   MEM_DWELL_TBL_OwnerAcceptFunc_t  OwnerAcceptFunc;

   /*
   ** Standard CJSON table data
   */
   
   bool         Loaded;   /* Has entire table been loaded? */
   uint16       LastLoadCnt;
   
   size_t       JsonObjCnt;
   char         JsonBuf[MEM_DWELL_TBL_JSON_FILE_MAX_CHAR];   
   size_t       JsonFileLen;
   
} MEM_DWELL_TBL_Class_t;


/************************/
/** Exported Functions **/
/************************/

/******************************************************************************
** Function: MEM_DWELL_TBL_Constructor
**
** Initialize the Memory Dwell Table object.
**
** Notes:
**   1. This must be called prior to any other function.
**   2. The local table data is not populated. This is done when the table is 
**      registered with the app framework table manager.
*/
void MEM_DWELL_TBL_Constructor(MEM_DWELL_TBL_Class_t *ObjPtr,
                               MEM_DWELL_TBL_OwnerAcceptFunc_t OwnerAcceptFunc);


/******************************************************************************
** Function: MEM_DWELL_TBL_DumpCmd
**
** Command to dump the table.
**
** Notes:
**  1. Function signature must match TBLMGR_DumpTblFuncPtr_t.
**
*/
bool MEM_DWELL_TBL_DumpCmd(osal_id_t FileHandle);


/******************************************************************************
** Function: MEM_DWELL_TBL_EntryMemSizeEnabled
**
** Notes:
**   1. Use MemSize to determine whether an entry is enabled.
** 
*/
bool MEM_DWELL_TBL_EntryMemSizeEnabled(const MEM_DWELL_TBL_Dwell_Entry_t *Entry);


/******************************************************************************
** Function: MEM_DWELL_TBL_GetDwellPtr
**
** Notes:
**   None
*/
MEM_DWELL_TBL_Dwell_t *MEM_DWELL_TBL_GetDwellPtr(MEM_MGR_DwellId_Enum_t DwellId);


/******************************************************************************
** Function: MEM_DWELL_TBL_LoadCmd
**
** Command to load the table.
**
** Notes:
**  1. Function signature must match TBLMGR_LoadTblFuncPtr_t.
**  2. Can assume valid table file name because this is a callback from 
**     the app framework table manager that has verified the file exists.
**
*/
bool MEM_DWELL_TBL_LoadCmd(APP_C_FW_TblLoadOptions_Enum_t LoadType, const char *Filename);


/******************************************************************************
** Function: MEM_DWELL_TBL_LoadEntry
**
** Notes:
**  1. Validates and loads a dwell entry
**
*/
bool MEM_DWELL_TBL_LoadEntry(MEM_DWELL_TBL_Dwell_Entry_t *Entry,
                             uint16 Delay,
                             MEM_MGR_MemSize_Enum_t MemSize,
                             MEM_MGR_SymbolAddr_t SymbolAddr);

/******************************************************************************
** Function: MEM_DWELL_TBL_ResetStatus
**
** Reset counters and status flags to a known reset state.  The behavior of
** the table manager should not be impacted. The intent is to clear counters
** and flags to a known default state for telemetry.
**
** Notes:
**   1. See the MEM_DWELL_TBL_Class_t definition for the affected data.
**
*/
void MEM_DWELL_TBL_ResetStatus(void);


#endif /* _mem_dwell_tbl_ */
