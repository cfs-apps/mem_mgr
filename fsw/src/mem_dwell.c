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
**    Implement the MEM_MGR_Class methods
**
**  Notes:
**    1. TODO: Describe OO design
**
*/

/*
** Include Files:
*/

#include <string.h>
#include "mem_dwell.h"


/*******************************/
/** Local Function Prototypes **/
/*******************************/

static bool ComputeByteCnt(MEM_MGR_MemSize_Enum_t MemSize,uint16 *ByteCnt);
static bool ValidDwellId(uint16 Id, const char *CmdStr);
static bool ValidDwellName(const char *Name, const char *CmdStr);

/**********************/
/** Global File Data **/
/**********************/

static MEM_DWELL_Class_t *MemDwell = NULL;


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
                           TBLMGR_Class_t *TblMgr)
{
   
   MemDwell = MemDwellPtr;

   CFE_PSP_MemSet((void*)MemDwell, 0, sizeof(MEM_DWELL_Class_t));

   MemDwell->IniTbl = IniTbl;

   MEM_DWELL_TBL_Constructor(&MemDwell->Tbl);


   TBLMGR_RegisterTblWithDef(TblMgr, MEM_DWELL_TBL_NAME,
                             MEM_DWELL_TBL_LoadCmd, MEM_DWELL_TBL_DumpCmd,  
                             INITBL_GetStrConfig(IniTbl, CFG_DWELL_TBL_LOAD_FILE));

                             
} /* End MEM_DWELL_Constructor() */


/******************************************************************************
** Function: MEM_DWELL_Execute
**
** Notes:
**   1. DelayCnt does not need to be validated because it any uint16 value
**      is valid.
** 
*/
bool MEM_DWELL_Execute(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{
   
   uint16 DwellId;
   MEM_DWELL_TBL_Dwell_t *DwellTbl;
   
   for (DwellId = MEM_MGR_DwellId_Enum_t_MIN; DwellId < MEM_MGR_DwellId_Enum_t_MAX; DwellId++)
   {
      DwellTbl = MEM_DWELL_TBL_PTR(MemDwell->Tbl.Dwell,DwellId);
      OS_printf("Dwell ID = %d\n",DwellTbl->Id);
      
   } /* End dwell ID loop */
   
   return true;
   
} /* End MEM_DWELL_Execute() */


/******************************************************************************
** Function: MEM_DWELL_LoadEntryCmd
**
** Notes:
**   1. DelayCnt does not need to be validated because it any uint16 value
**      is valid.
** 
*/
bool MEM_DWELL_LoadEntryCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{

   const MEM_MGR_LoadDwellEntry_CmdPayload_t *Cmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_LoadDwellEntry_t);
   
   bool    RetStatus = false;
   uint16  ByteCnt;
   MEMORY_VerifiedMemory_t VerifiedMemory;
   MEM_DWELL_TBL_Dwell_t *DwellTbl;

   // Utility functions send error events
   if (ValidDwellId(Cmd->DwellId,"Load dwell entry"))
   {
      DwellTbl = MEM_DWELL_TBL_PTR(MemDwell->Tbl.Dwell,Cmd->DwellId);
      if (Cmd->EntryIndex < MEM_MGR_DWELL_ENTRIES)
      {
         if (ComputeByteCnt(Cmd->MemSize,&ByteCnt))
         {
            if (MEMORY_VerifyAddr(Cmd->SymbolAddr, MEM_MGR_MemType_RAM, Cmd->MemSize,
                                  ByteCnt, &VerifiedMemory))
            {
               MEM_DWELL_TBL_Dwell_Entry_t *Entry = &DwellTbl->Entry[Cmd->EntryIndex];
               Entry->Length     = ByteCnt;
               Entry->Delay      = Cmd->Delay;
               Entry->SymbolAddr = Cmd->SymbolAddr;
               
               RetStatus = true;
               CFE_EVS_SendEvent(MEM_DWELL_SET_NAME_CMD_EID, CFE_EVS_EventType_INFORMATION,
                                 "Loaded dwell table %d entry at index %d",
                                 Cmd->DwellId, Cmd->EntryIndex);
            }
         }
      }/* End valid entry index */
      else
      {
         CFE_EVS_SendEvent(MEM_DWELL_LOAD_ENTRY_CMD_EID, CFE_EVS_EventType_INFORMATION,
                           "Load dwell entry command rejected, entry index %d exceeds maximum index of %d",
                           Cmd->EntryIndex, (MEM_MGR_DWELL_ENTRIES-1));         
      }
   } /* End valid dwell ID */
      
   return RetStatus;
   
} /* End MEM_DWELL_LoadEntryCmd() */


/******************************************************************************
** Function:  MEM_DWELL_ResetStatus
**
*/
void MEM_DWELL_ResetStatus(void)
{
   
   
} /* End MEM_DWELL_ResetStatus() */


/******************************************************************************
** Function: MEM_DWELL_SetNameCmd
**
*/
bool MEM_DWELL_SetNameCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{

   const MEM_MGR_SetDwellName_CmdPayload_t *Cmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_SetDwellName_t);
   bool  RetStatus = false;

   // Utility functions send error events
   if (ValidDwellId(Cmd->DwellId,"Set dwell name"))
   {
      if (ValidDwellName(Cmd->Name,"Set dwell name"))
      {
         MEM_DWELL_TBL_Dwell_t *DwellTbl = MEM_DWELL_TBL_PTR(MemDwell->Tbl.Dwell,Cmd->DwellId);
         strncpy(DwellTbl->Name,Cmd->Name,MEM_MGR_DWELL_NAME_LEN);
         RetStatus = true;
         CFE_EVS_SendEvent(MEM_DWELL_SET_NAME_CMD_EID, CFE_EVS_EventType_INFORMATION,
                           "Set dwell table %d name to %s", Cmd->DwellId, Cmd->Name);
      }
   }

   return RetStatus;
   
} /* End MEM_DWELL_SetNameCmd() */


/******************************************************************************
** Function: MEM_DWELL_StartCmd
**
*/
bool MEM_DWELL_StartCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{

   const MEM_MGR_DwellId_CmdPayload_t *Cmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_StartDwell_t);
   bool  RetStatus = true;
   
   MEM_MGR_DwellId_Enum_t DwellId = Cmd->DwellId;
   OS_printf("DwellId = %d\n",(uint8)DwellId);
   
   return RetStatus;
   
} /* End MEM_DWELL_StartCmd() */


/******************************************************************************
** Function: MEM_DWELL_StopCmd
**
** Note:
**   1. 
*/
bool MEM_DWELL_StopCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{

   const MEM_MGR_DwellId_CmdPayload_t *Cmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_StopDwell_t);
   bool  RetStatus = true;
   
   MEM_MGR_DwellId_Enum_t DwellId = Cmd->DwellId;
   OS_printf("DwellId = %d\n",(uint8)DwellId);
   
   return RetStatus;
   
} /* End MEM_DWELL_StopCmd() */


/******************************************************************************
** Function: ComputeByteCnt
**
*/
static bool ComputeByteCnt(MEM_MGR_MemSize_Enum_t MemSize,uint16 *ByteCnt)
{
   
   bool RetStatus = true;

   *ByteCnt = 0;
   switch (MemSize)
   {
      case MEM_MGR_MemSize_8:
         *ByteCnt = 1;
         break;
      case MEM_MGR_MemSize_16:
         *ByteCnt = 2;
         break;
      case MEM_MGR_MemSize_32:
         *ByteCnt = 4;
         break;
      default:
         RetStatus = false;
         break;
   } /* End mem size switch */

   return RetStatus;
      
} /* End if ComputeByteCnt() */


/******************************************************************************
** Function: ValidDwellId
**
*/
static bool ValidDwellId(uint16 Id, const char *CmdStr)
{
   bool RetStatus = true;
   
   if (!(Id >= MEM_MGR_DwellId_Enum_t_MIN && Id <= MEM_MGR_DwellId_Enum_t_MAX))
   {
      RetStatus = false;
      CFE_EVS_SendEvent(MEM_DWELL_INVALID_ID_EID, CFE_EVS_EventType_ERROR,
                        "%s command rejected, invalid ID %d. ID must be between %d and %d inclusively", 
                        CmdStr, Id, MEM_MGR_DwellId_Enum_t_MIN, MEM_MGR_DwellId_Enum_t_MAX);      
   }

   return RetStatus;
   
} /* End ValidDwellId() */


/******************************************************************************
** Function: ValidDwellName
**
** Note:
**   1. A string length of zero is valid.
**
*/
static bool ValidDwellName(const char *Name, const char *CmdStr)
{
   bool   RetStatus = true;
   uint16 CharCnt;
   
   for (CharCnt=0; CharCnt < MEM_MGR_DWELL_NAME_LEN; CharCnt++)
   {
      if (Name[CharCnt] == '\0') break;
   }

   if (CharCnt >= MEM_MGR_DWELL_NAME_LEN)
   {
      RetStatus = false;
      CFE_EVS_SendEvent(MEM_DWELL_INVALID_NAME_EID, CFE_EVS_EventType_ERROR,
                        "%s command rejected, name length greater than %d", 
                        CmdStr, MEM_MGR_DWELL_NAME_LEN);
   }

   return RetStatus;
   
} /* End ValidDwellName() */
