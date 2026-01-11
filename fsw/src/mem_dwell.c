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
**    1. TODO: Describe OO design. Think about separate DwellTlm object.
**
*/

/*
** Include Files:
*/

#include <string.h>
#include "mem_dwell.h"

/*
** Obtain pointer to arrays using Dwell IDs as indices
*/
//TODO: May just need a ID to indice macro
#define MEM_DWELL_TLM_PTR(id)  (&MemDwell->Tlm[id-1])  // Access array of MEM_MGR_DwellTlm_t using dwell IDs
#define MEM_DWELL_CTRL_PTR(id) (&MemDwell->Ctrl[id-1])


/*******************************/
/** Local Function Prototypes **/
/*******************************/

static bool AcceptDwellTableLoad(const MEM_DWELL_TBL_Dwell_t DwellTbl[MEM_MGR_DWELL_ID_CNT]);
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
      DwellTbl = MEM_DWELL_TBL_PTR(MemDwell->Tbl.Dwell,DwellId);
      DwellTlm = MEM_DWELL_TLM_PTR(DwellId);
      
      CFE_MSG_Init(CFE_MSG_PTR(DwellTlm->TelemetryHeader), CFE_SB_ValueToMsgId(DwellTbl->TopicId),
                   sizeof(MEM_MGR_DwellTlm_t));
                   
      TlmPayload = &DwellTlm->Payload;
      DwellCtrl  = MEM_DWELL_CTRL_PTR(DwellId);
      
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
      // Other errors shouldn't terminate the child task
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
**   1. DelayCnt does not need to be validated because it any uint16 value
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
      DwellTbl = MEM_DWELL_TBL_PTR(MemDwell->Tbl.Dwell,Cmd->DwellId);
      if (Cmd->EntryIndex < MEM_MGR_DWELL_ENTRIES)
      {
         MEM_DWELL_TBL_Dwell_Entry_t *Entry = &DwellTbl->Entry[Cmd->EntryIndex];
         RetStatus = MEM_DWELL_TBL_LoadEntry(Entry, Cmd->Delay, Cmd->MemSize, Cmd->SymbolAddr);
         
         if (RetStatus)
         {
            CFE_EVS_SendEvent(MEM_DWELL_LOAD_ENTRY_CMD_EID, CFE_EVS_EventType_INFORMATION,
                              "Loaded dwell table %d entry at index %d",
                              Cmd->DwellId, Cmd->EntryIndex);
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
         
         MEM_DWELL_TBL_Dwell_t      *DwellTbl   = MEM_DWELL_TBL_PTR(MemDwell->Tbl.Dwell,Cmd->DwellId);
         MEM_MGR_DwellTlm_Payload_t *TlmPayload = &(MEM_DWELL_TLM_PTR(Cmd->DwellId)->Payload);
         
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
  
   if (ValidDwellId(Cmd->DwellId,"Set dwell name"))
   {
      DwellCtrl = MEM_DWELL_CTRL_PTR(Cmd->DwellId);
      DwellTbl  = MEM_DWELL_TBL_PTR(MemDwell->Tbl.Dwell,Cmd->DwellId);

      DwellTbl->Enabled = true;
      DwellCtrl->DelayCntDown  = 1;  // Cause dwell packet to be issued on first wakeup 
      DwellCtrl->TlmDataOffset = 0;
      DwellCtrl->EntryIndex    = 0;

      CFE_EVS_SendEvent(MEM_DWELL_START_CMD_EID, CFE_EVS_EventType_INFORMATION,
                        "Enabled dwell table %d", Cmd->DwellId);
   
   } /* End if valid Dwell ID */
     
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
   
   MEM_DWELL_Ctrl_t       *DwellCtrl;
   MEM_DWELL_TBL_Dwell_t  *DwellTbl;
   
   if (ValidDwellId(Cmd->DwellId,"Set dwell name"))
   {
      DwellCtrl = MEM_DWELL_CTRL_PTR(Cmd->DwellId);
      DwellTbl  = MEM_DWELL_TBL_PTR(MemDwell->Tbl.Dwell,Cmd->DwellId);

      DwellTbl->Enabled = false;
      DwellCtrl->DelayCntDown  = 0;
      DwellCtrl->TlmDataOffset = 0;
      DwellCtrl->EntryIndex    = 0;
      
      CFE_EVS_SendEvent(MEM_DWELL_STOP_CMD_EID, CFE_EVS_EventType_INFORMATION,
                        "Disabled dwell table %d", Cmd->DwellId);

   } /* End if valid Dwell ID */

   return RetStatus;
   
} /* End MEM_DWELL_StopCmd() */


/******************************************************************************
** Function: AcceptDwellTableLoad
**
** Notes:
**   1. If this functiom returns false then the new table will not be used so
**      it contain table validation logic.
**   TODO: What validation should be done?
*/
static bool AcceptDwellTableLoad(const MEM_DWELL_TBL_Dwell_t DwellTbl[MEM_MGR_DWELL_ID_CNT])
{
   
   MEM_MGR_DwellId_Enum_t      DwellId;
   MEM_DWELL_Ctrl_t            *DwellCtrl;
   const MEM_DWELL_TBL_Dwell_t *DwellTblPtr;
   uint16  i;
   uint16  DelayCnts;
   uint16  AddrCnt;
   uint16  DataBytes;
 
   for (DwellId = MEM_MGR_DwellId_Enum_t_MIN; DwellId <= MEM_MGR_DwellId_Enum_t_MAX; DwellId++)
   {
      
      DwellTblPtr = MEM_DWELL_TBL_PTR(DwellTbl,DwellId);      
      DwellCtrl   = MEM_DWELL_CTRL_PTR(DwellId);      
      
      AddrCnt   = 0;
      DataBytes = 0;
      DelayCnts = 0;
      i = 0;
      while ((i < MEM_MGR_DWELL_ENTRIES) && MEM_DWELL_TBL_EntryMemSizeEnabled(&DwellTblPtr->Entry[i]))
      {
        AddrCnt++;
        DataBytes += DwellTblPtr->Entry[i].MemSize; //TODO use function?
        DelayCnts += DwellTblPtr->Entry[i].Delay;
        i++;
      }
      
      DwellCtrl->Tbl.DelayCnts = DelayCnts;
      DwellCtrl->Tbl.AddrCnt   = AddrCnt;
      DwellCtrl->Tbl.DataLen   = DataBytes;
      
OS_printf("DwellId %d: DelayCnts %d, AddrCnt %d, DataBytes %d\n",
          DwellId,DelayCnts,AddrCnt,DataBytes);
          
   } /* End dwell ID loop */

   return true;
   
} /* End AcceptDwellTableLoad() */


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
      DwellTbl  = MEM_DWELL_TBL_PTR(MemDwell->Tbl.Dwell,DwellId);
      DwellCtrl = MEM_DWELL_CTRL_PTR(DwellId);
      DwellTlm  = MEM_DWELL_TLM_PTR(DwellId);
//OS_printf("Table %d: Enabled %d, DelayCntDown %d, EntryIndex %d, TlmDataOffset %d\n",
//          DwellId,DwellTbl->Enabled,DwellCtrl->DelayCntDown,DwellCtrl->EntryIndex,DwellCtrl->TlmDataOffset);
      if (DwellTbl->Enabled && DwellCtrl->Tbl.DelayCnts > 0)
      {
         DwellCtrl->DelayCntDown--;

         while (DwellCtrl->DelayCntDown == 0)
         {
           
            EntryIndex = DwellCtrl->EntryIndex;
//OS_printf("EntryIndex %d: Address=%p, MemSize=%d\n",
//          EntryIndex,(void *)DwellTbl->Entry[EntryIndex].VerifiedMemory.CpuAddr,DwellTbl->Entry[EntryIndex].MemSize);
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
//OS_printf("DwellBytes %d, DwellData %d, TLM ID %d\n",DwellBytes,DwellData,DwellTlm->Payload.Id);         
       
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
