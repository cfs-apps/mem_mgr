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
**    Implement the MEM_DWELL_Class methods
**
**  Notes:
**    1. TODO: Describe OO design. Think about separate DwellTlm object.
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

static bool AcceptDwellTableLoad(const MEM_DWELL_TBL_Dwell_t DwellTbl[MEM_MGR_DWELL_ID_CNT]);
static void ConfigDwellCtrl(MEM_MGR_DwellId_Enum_t DwellId, bool Enable);
static bool EnableDwellCtrl(const MEM_DWELL_Ctrl_t *DwellCtrl);
static bool GenerateDwellTlm(void);
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

   MEM_MGR_DwellId_Enum_t     DwellId;
   MEM_DWELL_TBL_Dwell_t      *DwellTbl;
   MEM_MGR_DwellTlm_t         *DwellTlm;
   MEM_MGR_DwellTlm_Payload_t *TlmPayload;
   MEM_DWELL_Ctrl_t           *DwellCtrl;

   
   MemDwell = MemDwellPtr;
   CFE_PSP_MemSet((void*)MemDwell, 0, sizeof(MEM_DWELL_Class_t));
   MemDwell->IniTbl = IniTbl;

   MEM_DWELL_TBL_Constructor(&MemDwell->Tbl,AcceptDwellTableLoad);
   
   TBLMGR_RegisterTblWithDef(TblMgr, MEM_DWELL_TBL_NAME,
                             MEM_DWELL_TBL_LoadCmd, MEM_DWELL_TBL_DumpCmd,  
                             INITBL_GetStrConfig(IniTbl, CFG_MEM_DWELL_TBL_LOAD_FILE));

   /*
   ** Initialize dwell telemetry messages 
   ** - AcceptDwellTableLoad() computes and loads the control data structure
   */
   for (DwellId = MEM_MGR_DwellId_Enum_t_MIN; DwellId <= MEM_MGR_DwellId_Enum_t_MAX; DwellId++)
   {
      DwellTbl = MEM_DWELL_VAR_PTR(MemDwell->Tbl.Dwell,DwellId);
      DwellTlm = MEM_DWELL_VAR_PTR(MemDwell->Tlm,DwellId);
      
      CFE_MSG_Init(CFE_MSG_PTR(DwellTlm->TelemetryHeader), CFE_SB_ValueToMsgId(DwellTbl->TopicId),
                   sizeof(MEM_MGR_DwellTlm_t));
                   
      TlmPayload = &DwellTlm->Payload;
      DwellCtrl  = MEM_DWELL_VAR_PTR(MemDwell->Ctrl,DwellId);
      
      TlmPayload->Id = DwellTbl->Id;
      strncpy(TlmPayload->Name,DwellTbl->Name,MEM_MGR_DWELL_NAME_LEN);
      
      TlmPayload->Delay        = DwellCtrl->Tbl.DelayCnts;
      TlmPayload->AddressCnt   = DwellCtrl->Tbl.AddrCnt;
      TlmPayload->DataByteLen  = DwellCtrl->Tbl.DataLen;      
      
   } /* End dwell ID loop */
   
   MemDwell->PerfId  = INITBL_GetIntConfig(IniTbl, CFG_MEM_DWELL_CHILD_PERF_ID);
   MemDwell->ExecMid = CFE_SB_ValueToMsgId(INITBL_GetIntConfig(IniTbl, CFG_MEM_DWELL_EXEC_TOPICID));

   CFE_SB_CreatePipe(&MemDwell->ExecPipe, INITBL_GetIntConfig(IniTbl, CFG_MEM_DWELL_PIPE_DEPTH),
                     INITBL_GetStrConfig(IniTbl, CFG_MEM_DWELL_PIPE_NAME));  
   CFE_SB_Subscribe(MemDwell->ExecMid, MemDwell->ExecPipe);
   
} /* End MEM_DWELL_Constructor() */


/******************************************************************************
** Function: MEM_DWELL_ChildTask
**
** Notes:
**   1. Returning false causes the child task to terminate.
**   2. The following data sources are used to control the generation of dwell
**      telemetry messages:
**        MemDwell->DwellTbl     Parameters defined in JSON dwell table 
**        MemDwell->Ctrl[].Tbl   Parameters derived from JSON defined parameters
**                               These are computed when the table is loaded or
**                               jammed (updated via command)
**        MemDwell->Ctrl[].State Dynamic variables that 
** 
*/
bool MEM_DWELL_ChildTask(CHILDMGR_Class_t* ChildMgr)
{
   bool   RetStatus = false;
   int32  SysStatus;

   CFE_SB_Buffer_t  *SbBufPtr;
   CFE_SB_MsgId_t   MsgId = CFE_SB_INVALID_MSG_ID;

   CFE_ES_PerfLogExit(MemDwell->PerfId);
   SysStatus = CFE_SB_ReceiveBuffer(&SbBufPtr, MemDwell->ExecPipe, CFE_SB_PEND_FOREVER);
   CFE_ES_PerfLogEntry(MemDwell->PerfId);

   if (SysStatus == CFE_SUCCESS)
   {
      // Non-SB errors shouldn't terminate the child task
      RetStatus = true;
      
      SysStatus = CFE_MSG_GetMsgId(&SbBufPtr->Msg, &MsgId);

      if (SysStatus == CFE_SUCCESS)
      {
         if (CFE_SB_MsgId_Equal(MsgId, MemDwell->ExecMid))
         {
            GenerateDwellTlm();
         } 
         else
         {   
            CFE_EVS_SendEvent(MEM_DWELL_EXECUTE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Received unexpected packet on memory dwell execute pipe, MID = 0x%08X", 
                              CFE_SB_MsgIdToValue(MsgId));
         }

      } /* End if got message ID */
   } /* End if received buffer */
   
   return RetStatus;
   
} /* End MEM_DWELL_ChildTask() */


/******************************************************************************
** Function: MEM_DWELL_LoadEntryCmd
**
** Notes:
**   1. DelayCnt does not need to be validated because any uint16 value
**      is valid.
** 
*/
bool MEM_DWELL_LoadEntryCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{

   const MEM_MGR_LoadDwellEntry_CmdPayload_t *Cmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_LoadDwellEntry_t);
   
   bool    RetStatus = false;
   MEM_DWELL_TBL_Dwell_t *DwellTbl;

   // Utility functions send error events
   if (ValidDwellId(Cmd->DwellId,"Load dwell entry"))
   {
      DwellTbl = MEM_DWELL_VAR_PTR(MemDwell->Tbl.Dwell,Cmd->DwellId);
      if (Cmd->EntryIndex < MEM_MGR_DWELL_ENTRIES)
      {
         MEM_DWELL_TBL_Dwell_Entry_t *Entry = &DwellTbl->Entry[Cmd->EntryIndex];
         RetStatus = MEM_DWELL_TBL_LoadEntry(Entry, Cmd->Delay, Cmd->MemSize, Cmd->SymbolAddr);
         
         if (RetStatus)
         {
            CFE_EVS_SendEvent(MEM_DWELL_LOAD_ENTRY_CMD_EID, CFE_EVS_EventType_INFORMATION,
                              "Loaded dwell table %d entry at index %d",
                              Cmd->DwellId, Cmd->EntryIndex);

            if (DwellTbl->Enabled)
            {
               ConfigDwellCtrl(Cmd->DwellId, true);
               CFE_EVS_SendEvent(MEM_DWELL_LOAD_ENTRY_CMD_EID, CFE_EVS_EventType_INFORMATION,
                                 "Dwell is active so current telemetry message generation reset to include the new entry.");
            }
         } /* End if valid EntryIndex */
         else
         {
            CFE_EVS_SendEvent(MEM_DWELL_LOAD_ENTRY_CMD_EID, CFE_EVS_EventType_ERROR,
                              "Load memory dwell table entry command failed. See accompanying event for details");
         }
      }/* End valid entry index */
      else
      {
         CFE_EVS_SendEvent(MEM_DWELL_LOAD_ENTRY_CMD_EID, CFE_EVS_EventType_ERROR,
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

   // Nothing to reset
   
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
         
         MEM_DWELL_TBL_Dwell_t      *DwellTbl   = MEM_DWELL_VAR_PTR(MemDwell->Tbl.Dwell,Cmd->DwellId);
         MEM_MGR_DwellTlm_Payload_t *TlmPayload = &(MEM_DWELL_VAR_PTR(MemDwell->Tlm,Cmd->DwellId)->Payload);
                                                  
         strncpy(DwellTbl->Name,Cmd->Name,MEM_MGR_DWELL_NAME_LEN);
         strncpy(TlmPayload->Name,Cmd->Name,MEM_MGR_DWELL_NAME_LEN);
         
         RetStatus = true;
         CFE_EVS_SendEvent(MEM_DWELL_SET_NAME_CMD_EID, CFE_EVS_EventType_INFORMATION,
                           "Set dwell table %d name to %s", Cmd->DwellId, Cmd->Name);

      } /* End if valid name */
   } /* End if valid ID */

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
   MEM_DWELL_Ctrl_t       *DwellCtrl;
   MEM_DWELL_TBL_Dwell_t  *DwellTbl;
    
   if (ValidDwellId(Cmd->DwellId,"Start dwell"))
   {
   
      DwellTbl  = MEM_DWELL_VAR_PTR(MemDwell->Tbl.Dwell,Cmd->DwellId);

      if (DwellTbl->Enabled)
      {
         CFE_EVS_SendEvent(MEM_DWELL_START_CMD_EID, CFE_EVS_EventType_INFORMATION,
                           "Dwell table %d was already enabled; Command had no effect", Cmd->DwellId);      
      }
      else
      {
         DwellCtrl = MEM_DWELL_VAR_PTR(MemDwell->Ctrl,Cmd->DwellId); 
         
         if (EnableDwellCtrl(DwellCtrl))
         {
            ConfigDwellCtrl(Cmd->DwellId, true);
            CFE_EVS_SendEvent(MEM_DWELL_START_CMD_EID, CFE_EVS_EventType_INFORMATION,
                              "Enabled dwell table %d", Cmd->DwellId);
         }
         else
         {
            RetStatus = false;
            CFE_EVS_SendEvent(MEM_DWELL_START_CMD_EID, CFE_EVS_EventType_ERROR,
                              "Start dwell table %d rejected; one or more zero values: DelayCnts %d, AddrCnt %d, DataBytes %d", 
                              Cmd->DwellId,DwellCtrl->Tbl.DelayCnts,DwellCtrl->Tbl.AddrCnt,DwellCtrl->Tbl.DataLen);
         }
      } /* End if dwell disabled */

   } /* End if valid Dwell ID */
     
   return RetStatus;
   
} /* End MEM_DWELL_StartCmd() */


/******************************************************************************
** Function: MEM_DWELL_StopCmd
**
*/
bool MEM_DWELL_StopCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{

   const MEM_MGR_DwellId_CmdPayload_t *Cmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_StopDwell_t);
   bool  RetStatus = true;
   
   MEM_DWELL_TBL_Dwell_t  *DwellTbl;
   
   if (ValidDwellId(Cmd->DwellId,"Stop dwell"))
   {
      DwellTbl  = MEM_DWELL_VAR_PTR(MemDwell->Tbl.Dwell,Cmd->DwellId);

      if (DwellTbl->Enabled)
      {
         CFE_EVS_SendEvent(MEM_DWELL_STOP_CMD_EID, CFE_EVS_EventType_INFORMATION,
                           "Disabled dwell table %d", Cmd->DwellId);
      }
      else
      {
         CFE_EVS_SendEvent(MEM_DWELL_STOP_CMD_EID, CFE_EVS_EventType_INFORMATION,
                           "Dwell table %d was already disabled; Control variables reset", Cmd->DwellId);      
      }
      
      ConfigDwellCtrl(Cmd->DwellId, false);
      
   } /* End if valid Dwell ID */

   return RetStatus;
   
} /* End MEM_DWELL_StopCmd() */


/******************************************************************************
** Function: AcceptDwellTableLoad
**
** Notes:
**   1. If false is returned then the new table will not be used
**   2. If true is returned then the table will be used. This is the last stage
**      of table acceptance therefore if a table is enabled/disabled then
**      appropriate processing can be performed. 
*/
static bool AcceptDwellTableLoad(const MEM_DWELL_TBL_Dwell_t DwellTbl[MEM_MGR_DWELL_ID_CNT])
{
   bool RetStatus = true;
   MEM_MGR_DwellId_Enum_t      DwellId;
   MEM_DWELL_Ctrl_t            *DwellCtrl;
   const MEM_DWELL_TBL_Dwell_t *DwellTblPtr;
   uint16  i;
   uint16  DelayCnts;
   uint16  AddrCnt;
   uint16  DataBytes;
 
   for (DwellId = MEM_MGR_DwellId_Enum_t_MIN; DwellId <= MEM_MGR_DwellId_Enum_t_MAX; DwellId++)
   {
      
      DwellTblPtr = MEM_DWELL_VAR_PTR(DwellTbl,DwellId);      
      DwellCtrl   = MEM_DWELL_VAR_PTR(MemDwell->Ctrl,DwellId);      
      
      AddrCnt   = 0;
      DataBytes = 0;
      DelayCnts = 0;
      i = 0;
      while ((i < MEM_MGR_DWELL_ENTRIES) && MEM_DWELL_TBL_EntryMemSizeEnabled(&DwellTblPtr->Entry[i]))
      {
        AddrCnt++;
        DataBytes += DwellTblPtr->Entry[i].MemSize;
        DelayCnts += DwellTblPtr->Entry[i].Delay;
        i++;
      }
      
      DwellCtrl->Tbl.DelayCnts = DelayCnts;
      DwellCtrl->Tbl.AddrCnt   = AddrCnt;
      DwellCtrl->Tbl.DataLen   = DataBytes;

      if (DwellTblPtr->Enabled)
      {
         if (EnableDwellCtrl(DwellCtrl))
         {
            ConfigDwellCtrl(DwellId, DwellTblPtr->Enabled);
            CFE_EVS_SendEvent(MEM_DWELL_ACCEPT_TBL_EID, CFE_EVS_EventType_INFORMATION,
                              "Dwell table %d enabled: DelayCnts %d, AddrCnt %d, DataBytes %d", 
                              DwellId, DelayCnts, AddrCnt, DataBytes);
         }
         else
         {
            RetStatus = false;
            CFE_EVS_SendEvent(MEM_DWELL_ACCEPT_TBL_EID, CFE_EVS_EventType_ERROR,
                              "Dwell table %d rejected; one or more zero values: DelayCnts %d, AddrCnt %d, DataBytes %d", 
                              DwellId, DelayCnts, AddrCnt, DataBytes);
         }
      } /* End if dwell enabled */
      else
      {
         if (!EnableDwellCtrl(DwellCtrl))
         {
            CFE_EVS_SendEvent(MEM_DWELL_ACCEPT_TBL_EID, CFE_EVS_EventType_INFORMATION,
                              "Dwell table %d warning; This table can't be enabled", 
                              DwellId);            
         }      
      } /* End if dwell disabled */
      
   } /* End dwell ID loop */

   return RetStatus;
   
} /* End AcceptDwellTableLoad() */


/******************************************************************************
** Function: ConfigDwellCtrl
**
*/
static void ConfigDwellCtrl(MEM_MGR_DwellId_Enum_t DwellId, bool Enable)
{
   
   MEM_DWELL_TBL_Dwell_t  *DwellTbl  = MEM_DWELL_VAR_PTR(MemDwell->Tbl.Dwell,DwellId);
   MEM_DWELL_Ctrl_t       *DwellCtrl = MEM_DWELL_VAR_PTR(MemDwell->Ctrl,DwellId);
   MEM_MGR_DwellTlm_t     *DwellTlm  = MEM_DWELL_VAR_PTR(MemDwell->Tlm,DwellId);

   DwellTbl->Enabled = Enable;
      
   // A 1 causes enabled dwell packet to be issued on first wakeup
   DwellCtrl->DelayCntDown  = Enable? 1 : 0;
   DwellCtrl->TlmDataOffset = 0;
   DwellCtrl->EntryIndex    = 0;
   
   CFE_PSP_MemSet((void*)&DwellTlm->Payload.Data, 0, MEM_MGR_DWELL_DATA_BUF_LEN);

} /* End ConfigDwellCtrl() */


/******************************************************************************
** Function: EnableDwellCtrl
**
** Determine whether the control parameters are valid for enabling a dwell.
**
*/
static bool EnableDwellCtrl(const MEM_DWELL_Ctrl_t *DwellCtrl)
{
   
   return (DwellCtrl->Tbl.DelayCnts != 0 && 
           DwellCtrl->Tbl.AddrCnt   != 0 &&
           DwellCtrl->Tbl.DataLen   != 0);
          
} /* End EnableDwellCtrl() */


/******************************************************************************
** Function: GenerateDwellTlm
**
** Notes:
**   1. The following fields are loaded when a dwell table load is accepted:
**      DwellId, Name, Delay, AddressCnt, DataByteLen
**
*/
static bool GenerateDwellTlm(void)
{
   
   MEM_MGR_DwellId_Enum_t  DwellId;
   MEM_DWELL_TBL_Dwell_t   *DwellTbl;
   MEM_DWELL_Ctrl_t        *DwellCtrl;
   MEM_MGR_DwellTlm_t      *DwellTlm;
   uint16  EntryIndex;
   uint16  DwellBytes;
   uint32  DwellData;

   for (DwellId = MEM_MGR_DwellId_Enum_t_MIN; DwellId <= MEM_MGR_DwellId_Enum_t_MAX; DwellId++)
   {
      DwellTbl  = MEM_DWELL_VAR_PTR(MemDwell->Tbl.Dwell,DwellId);
      DwellCtrl = MEM_DWELL_VAR_PTR(MemDwell->Ctrl,DwellId);
      DwellTlm  = MEM_DWELL_VAR_PTR(MemDwell->Tlm,DwellId);
      
      // Used the "ERR" event becuase it's filtered
      CFE_EVS_SendEvent(MEM_DWELL_EXECUTE_ERR_EID, CFE_EVS_EventType_DEBUG,
                        "GenerateDwellTlm table %d: Enabled %d, DelayCntDown %d, EntryIndex %d, TlmDataOffset %d", 
                        DwellId,DwellTbl->Enabled,DwellCtrl->DelayCntDown,DwellCtrl->EntryIndex,DwellCtrl->TlmDataOffset);
      
      if (DwellTbl->Enabled && DwellCtrl->Tbl.DelayCnts > 0)
      {
         DwellCtrl->DelayCntDown--;

         while (DwellCtrl->DelayCntDown == 0)
         {
           
            EntryIndex = DwellCtrl->EntryIndex;

            // A zero address with a valid MemSize should not pass table validation 
            if (DwellTbl->Entry[EntryIndex].VerifiedMemory.CpuAddr != 0)
            {
               DwellBytes = MEMORY_Read(DwellTbl->Entry[EntryIndex].VerifiedMemory,
                                        DwellTbl->Entry[EntryIndex].MemSize, &DwellData);
            }
            else
            {
               DwellTbl->Enabled = false;
               CFE_EVS_SendEvent(MEM_DWELL_EXECUTE_ERR_EID, CFE_EVS_EventType_ERROR,
                                 "Dwell table %d disabled with critical error. Entry index %d is within address count %d and contains an invalid address", 
                                 DwellId, EntryIndex, DwellCtrl->Tbl.AddrCnt);               
            }    
       
            if (DwellBytes > 0)
            {
               memcpy(&DwellTlm->Payload.Data[DwellCtrl->TlmDataOffset], &DwellData, DwellBytes);
               DwellCtrl->TlmDataOffset += DwellBytes;
            }
            else
            {
               // Maintain offset so ground can interpret the data
               // Send error event message and stay in loop to manage counters

               DwellCtrl->TlmDataOffset +=  DwellTbl->Entry[EntryIndex].MemSize;
               CFE_EVS_SendEvent(MEM_DWELL_EXECUTE_ERR_EID, CFE_EVS_EventType_ERROR,
                                 "Error reading memory for dwell table %d at entry index %d", 
                                 DwellId, EntryIndex);
            }
                      
            if (EntryIndex == DwellCtrl->Tbl.AddrCnt-1)
            {

               // Completed telemetry message, send it and reset counters
               CFE_SB_TimeStampMsg(CFE_MSG_PTR(DwellTlm->TelemetryHeader));
               CFE_SB_TransmitMsg(CFE_MSG_PTR(DwellTlm->TelemetryHeader), true);
               DwellCtrl->EntryIndex    = 0;
               DwellCtrl->TlmDataOffset = 0;
            
            }
            else
            {
               DwellCtrl->EntryIndex++;
               
            } /* End if still filling telemetry message */
            
            DwellCtrl->DelayCntDown = DwellTbl->Entry[EntryIndex].Delay;
            
         } /* End while Countdown == 0 */
      } /* End if dwell enabled */
   } /* End dwell ID loop */
   
   return true;
   
} /* End GenerateDwellTlm() */


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
