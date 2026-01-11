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
**    Define memory dwell class that manages the creation of dwell telemetry
**    packets that contain from meory locations defined in dwell tables.
**
**  Notes:
**    1. MEM_DWELL_TBL defines what is contained in each dwell telemetry 
**       packet. The MEM_DWELL class defines data that is needed to manage
**       each dwell telemetry message.  
**
*/

#ifndef _mem_dwell_
#define _mem_dwell_

/*
** Includes
*/

#include "app_cfg.h"
#include "memory.h"
#include "mem_dwell_tbl.h"

/***********************/
/** Macro Definitions **/
/***********************/


/*
** Event Message IDs
*/

#define MEM_DWELL_CONSTRUCTOR_EID     (MEM_DWELL_BASE_EID + 0)
#define MEM_DWELL_INVALID_ID_EID      (MEM_DWELL_BASE_EID + 1)
#define MEM_DWELL_INVALID_NAME_EID    (MEM_DWELL_BASE_EID + 2)
#define MEM_DWELL_SET_NAME_CMD_EID    (MEM_DWELL_BASE_EID + 3)
#define MEM_DWELL_START_CMD_EID       (MEM_DWELL_BASE_EID + 4)
#define MEM_DWELL_STOP_CMD_EID        (MEM_DWELL_BASE_EID + 5)
#define MEM_DWELL_LOAD_ENTRY_CMD_EID  (MEM_DWELL_BASE_EID + 6)
#define MEM_DWELL_EXECUTE_ERR_EID     (MEM_DWELL_BASE_EID + 7)


/**********************/
/** Type Definitions **/
/**********************/

// TODO: Make length, bytelen, etc consistent
typedef struct
{
   uint16 DelayCnts;   // Sum of dwell entry delays
   uint16 AddrCnt;     // Number of dwell addresses in dwell message
   uint16 DataLen;     // Sum of dwell entry MemSizes in bytes
   
} MEM_DWELL_Ctrl_Tbl_t;


typedef struct
{
   uint16 DelayCntDown;      // Current execution coundown value 
   uint16 TlmDataOffset;     // Byte offset for where to write the next data value 
   uint16 EntryIndex;        // Index of current entry being processed
   
   MEM_DWELL_Ctrl_Tbl_t Tbl; // Data derived from table parameters when the table is loaded
         
} MEM_DWELL_Ctrl_t;


/******************************************************************************
** MEM_DWELL_Class
*/

typedef struct
{

   /*
   ** App Framework References
   */
   
   const INITBL_Class_t *IniTbl;
   
   /*
   ** MEM_DWELL MEM State Data
   */

   
   uint32           PerfId;
   CFE_SB_PipeId_t  ExecPipe;
   CFE_SB_MsgId_t   ExecMid;
   
   MEM_MGR_DwellTlm_t  Tlm[MEM_MGR_DWELL_ID_CNT];
   MEM_DWELL_Ctrl_t    Ctrl[MEM_MGR_DWELL_ID_CNT];
   
   /*
   ** Contained Objects
   */

   MEM_DWELL_TBL_Class_t  Tbl;
   
} MEM_DWELL_Class_t;



/************************/
/** Exported Functions **/
/************************/


/******************************************************************************
** Function: MEM_DWELL_Constructor
**
** Initialize the MEM_DWELL to a known state
**
** Notes:
**   1. This must be called prior to any other function.
**
*/
void MEM_DWELL_Constructor(MEM_DWELL_Class_t *MemDwellPtr,
                           const INITBL_Class_t *IniTbl,
                           TBLMGR_Class_t *TblMgr);


/******************************************************************************
** Function: MEM_DWELL_ChildTask
** 
** Notes:
**   1.  This is a CHILDMGR callback function and the function signature must
**       match CHILDMGR_TaskCallback_t.
**
*/
bool MEM_DWELL_ChildTask(CHILDMGR_Class_t* ChildMgr);


/******************************************************************************
** Function: MEM_DWELL_LoadEntryCmd
**
*/
bool MEM_DWELL_LoadEntryCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function:  MEM_DWELL_ResetStatus
**
*/
void MEM_DWELL_ResetStatus(void);


/******************************************************************************
** Function: MEM_DWELL_SetNameCmd
**
*/
bool MEM_DWELL_SetNameCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: MEM_DWELL_StartCmd
**
*/
bool MEM_DWELL_StartCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: MEM_DWELL_StopCmd
**
*/
bool MEM_DWELL_StopCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);



#endif /* _mem_dwell_ */
