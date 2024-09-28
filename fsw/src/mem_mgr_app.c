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
**    Implement the mem_mgr application
**
**  Notes:
**   1. This app is a refactor of NASA's Memory Manager and Memory
**      Dwell apps. They are combined into one app, redesigned using
**      Basecamp's OO framework based app design and using child
**      tasks for memory dwell functions and file-based memory
**      management commands. 
**
*/


/*
** Includes
*/

#include <string.h>
#include "mem_mgr_app.h"
#include "mem_mgr_eds_cc.h"

/***********************/
/** Macro Definitions **/
/***********************/

/* Convenience macros */
#define  INITBL_OBJ    (&(MemMgr.IniTbl))
#define  CMDMGR_OBJ    (&(MemMgr.CmdMgr))


/*******************************/
/** Local Function Prototypes **/
/*******************************/

static int32 InitApp(void);
static int32 ProcessCommands(void);
static void SendStatusTlm(void);


/**********************/
/** File Global Data **/
/**********************/

/* 
** Must match DECLARE ENUM() declaration in app_cfg.h
** Defines "static INILIB_CfgEnum_t IniCfgEnum"
*/
DEFINE_ENUM(Config,APP_CONFIG)  


/*****************/
/** Global Data **/
/*****************/

MEM_MGR_Class_t  MemMgr;


/******************************************************************************
** Function: MEM_MGR_AppMain
**
*/
void MEM_MGR_AppMain(void)
{

   uint32 RunStatus = CFE_ES_RunStatus_APP_ERROR;
   
   CFE_EVS_Register(NULL, 0, CFE_EVS_NO_FILTER);

   if (InitApp() == CFE_SUCCESS)      /* Performs initial CFE_ES_PerfLogEntry() call */
   {
      RunStatus = CFE_ES_RunStatus_APP_RUN; 
   }
   
   /*
   ** Main process loop
   */
   while (CFE_ES_RunLoop(&RunStatus))
   {
      
      RunStatus = ProcessCommands();  /* Pends indefinitely & manages CFE_ES_PerfLogEntry() calls */
      
   } /* End CFE_ES_RunLoop */

   CFE_ES_WriteToSysLog("MEM_MGR App terminating, run status = 0x%08X\n", RunStatus);   /* Use SysLog, events may not be working */

   CFE_EVS_SendEvent(MEM_MGR_EXIT_EID, CFE_EVS_EventType_CRITICAL, "MEM_MGR App terminating, run status = 0x%08X", RunStatus);

   CFE_ES_ExitApp(RunStatus);  /* Let cFE kill the task (and any child tasks) */

} /* End of MEM_MGR_AppMain() */


/******************************************************************************
** Function: MEM_MGR_NoOpCmd
**
*/
bool MEM_MGR_NoOpCmd(void* ObjDataPtr, const CFE_MSG_Message_t *MsgPtr)
{

   CFE_EVS_SendEvent (MEM_MGR_NOOP_EID, CFE_EVS_EventType_INFORMATION,
                      "No operation command received for MEM_MGR App version %d.%d.%d",
                      MEM_MGR_MAJOR_VER, MEM_MGR_MINOR_VER, MEM_MGR_PLATFORM_REV);

   return true;


} /* End MEM_MGR_NoOpCmd() */


/******************************************************************************
** Function: MEM_MGR_ResetAppCmd
**
** Notes:
**   1. Framework objects require an object reference since they are
**      reentrant. Applications use the singleton pattern and store a
**      reference pointer to the object data during construction.
*/
bool MEM_MGR_ResetAppCmd(void* ObjDataPtr, const CFE_MSG_Message_t *MsgPtr)
{

   CMDMGR_ResetStatus(CMDMGR_OBJ);
   MEMORY_ResetStatus(); 
   MEM_FILE_ResetStatus(); 
   
   return true;

} /* End MEM_MGR_ResetAppCmd() */


/******************************************************************************
** Function: InitApp
**
*/
static int32 InitApp(void)
{

   int32 Status = APP_C_FW_CFS_ERROR;
   

   /*
   ** Initialize objects 
   */
   
   if (INITBL_Constructor(INITBL_OBJ, MEM_MGR_INI_FILENAME, &IniCfgEnum))
   {
   
      MemMgr.PerfId  = INITBL_GetIntConfig(INITBL_OBJ, CFG_APP_PERF_ID);
      CFE_ES_PerfLogEntry(MemMgr.PerfId);

      MemMgr.CmdMid         = CFE_SB_ValueToMsgId(INITBL_GetIntConfig(INITBL_OBJ, CFG_MEM_MGR_CMD_TOPICID));
      MemMgr.SendStatusMid  = CFE_SB_ValueToMsgId(INITBL_GetIntConfig(INITBL_OBJ, CFG_MEM_MGR_SEND_STATUS_TOPICID));
      
      /*
      ** Constuct app's contained objects
      */

      MEMORY_Constructor(&MemMgr.Memory);
      MEM_FILE_Constructor(&MemMgr.MemFile, INITBL_OBJ);
      
      /*
      ** Initialize app level interfaces
      */
      
      CFE_SB_CreatePipe(&MemMgr.CmdPipe, INITBL_GetIntConfig(INITBL_OBJ, CFG_APP_CMD_PIPE_DEPTH), INITBL_GetStrConfig(INITBL_OBJ, CFG_APP_CMD_PIPE_NAME));  
      CFE_SB_Subscribe(MemMgr.CmdMid,        MemMgr.CmdPipe);
      CFE_SB_Subscribe(MemMgr.SendStatusMid, MemMgr.CmdPipe);

      CMDMGR_Constructor(CMDMGR_OBJ);
      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_NOOP_CC,  NULL, MEM_MGR_NoOpCmd,     0);
      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_RESET_CC, NULL, MEM_MGR_ResetAppCmd, 0);

      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_PEEK_CC,              NULL, MEMORY_PeekCmd,           sizeof(MEM_MGR_Peek_CmdPayload_t));      
      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_POKE_CC,              NULL, MEMORY_PokeCmd,           sizeof(MEM_MGR_Poke_CmdPayload_t));
      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_LOAD_WITH_INT_DIS_CC, NULL, MEMORY_LoadWithIntDisCmd, sizeof(MEM_MGR_LoadWithIntDis_CmdPayload_t));
      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_DUMP_TO_EVENT_CC,     NULL, MEMORY_DumpToEventCmd,    sizeof(MEM_MGR_DumpToEvent_CmdPayload_t));
      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_FILL_CC,              NULL, MEMORY_FillCmd,           sizeof(MEM_MGR_Fill_CmdPayload_t));
      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_LOOKUP_SYMBOL_CC,     NULL, MEMORY_LookupSymbolCmd,   sizeof(MEM_MGR_LookupSymbol_CmdPayload_t));
      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_ENA_EEPROM_WRITE_CC,  NULL, MEMORY_EnaEepromWriteCmd, sizeof(MEM_MGR_EnaEepromWrite_CmdPayload_t));
      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_DIS_EEPROM_WRITE_CC,  NULL, MEMORY_DisEepromWriteCmd, sizeof(MEM_MGR_DisEepromWrite_CmdPayload_t));

      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_LOAD_FROM_FILE_CC,       NULL, MEM_FILE_LoadCmd,       sizeof(MEM_MGR_LoadFromFile_CmdPayload_t));
      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_DUMP_TO_FILE_CC,         NULL, MEM_FILE_DumpCmd,       sizeof(MEM_MGR_DumpToFile_CmdPayload_t));
      CMDMGR_RegisterFunc(CMDMGR_OBJ, MEM_MGR_DUMP_SYM_TBL_TO_FILE_CC, NULL, MEM_FILE_DumpSymTblCmd, sizeof(MEM_MGR_DumpSymTblToFile_CmdPayload_t));
      
      /*
      ** Initialize app messages 
      */

      CFE_MSG_Init(CFE_MSG_PTR(MemMgr.StatusTlm.TelemetryHeader), 
                   CFE_SB_ValueToMsgId(INITBL_GetIntConfig(INITBL_OBJ, CFG_MEM_MGR_STATUS_TLM_TOPICID)),
                   sizeof(MEM_MGR_StatusTlm_t));

      /*
      ** Application startup event message
      */
      CFE_EVS_SendEvent(MEM_MGR_INIT_APP_EID, CFE_EVS_EventType_INFORMATION,
                        "MEM_MGR App Initialized. Version %d.%d.%d",
                        MEM_MGR_MAJOR_VER, MEM_MGR_MINOR_VER, MEM_MGR_PLATFORM_REV);

      Status = CFE_SUCCESS; 

   } /* End if INITBL constructed */
   
   return(Status);

} /* End of InitApp() */


/******************************************************************************
** Function: ProcessCommands
**
** 
*/
static int32 ProcessCommands(void)
{
   
   int32  RetStatus = CFE_ES_RunStatus_APP_RUN;
   int32  SysStatus;

   CFE_SB_Buffer_t  *SbBufPtr;
   CFE_SB_MsgId_t   MsgId = CFE_SB_INVALID_MSG_ID;

   CFE_ES_PerfLogExit(MemMgr.PerfId);
   SysStatus = CFE_SB_ReceiveBuffer(&SbBufPtr, MemMgr.CmdPipe, CFE_SB_PEND_FOREVER);
   CFE_ES_PerfLogEntry(MemMgr.PerfId);

   if (SysStatus == CFE_SUCCESS)
   {
      SysStatus = CFE_MSG_GetMsgId(&SbBufPtr->Msg, &MsgId);

      if (SysStatus == CFE_SUCCESS)
      {

         if (CFE_SB_MsgId_Equal(MsgId, MemMgr.CmdMid))
         {
            CMDMGR_DispatchFunc(CMDMGR_OBJ, &SbBufPtr->Msg);
         } 
         else if (CFE_SB_MsgId_Equal(MsgId, MemMgr.SendStatusMid))
         {   
            SendStatusTlm();
         }
         else
         {   
            CFE_EVS_SendEvent(MEM_MGR_INVALID_MID_EID, CFE_EVS_EventType_ERROR,
                              "Received invalid command packet, MID = 0x%08X", 
                              CFE_SB_MsgIdToValue(MsgId));
         }

      } /* End if got message ID */
   } /* End if received buffer */
   else
   {
      RetStatus = CFE_ES_RunStatus_APP_ERROR;
   } 

   return RetStatus;
   
} /* End ProcessCommands() */


/******************************************************************************
** Function: SendStatusTlm
**
*/
static void SendStatusTlm(void)
{

   MEM_MGR_StatusTlm_Payload_t *Payload = &MemMgr.StatusTlm.Payload;

   /*
   ** Framework Data
   */
   
   Payload->ValidCmdCnt   = MemMgr.CmdMgr.ValidCmdCnt;
   Payload->InvalidCmdCnt = MemMgr.CmdMgr.InvalidCmdCnt;

   Payload->EepromWriteEna  = MemMgr.Memory.EepromWriteEna;
   Payload->LastMemFunction = MemMgr.Memory.CmdStatus.Function;
   Payload->LastMemAddr     = MemMgr.Memory.CmdStatus.Addr;
   Payload->LastMemType     = MemMgr.Memory.CmdStatus.Type;
   Payload->LastMemSize     = MemMgr.Memory.CmdStatus.Size;
   Payload->LastMemByteCnt  = MemMgr.Memory.CmdStatus.ByteCnt;
   
   strncpy(Payload->LastMemFilename,MemMgr.MemFile.Filename,OS_MAX_PATH_LEN);
   
   CFE_SB_TimeStampMsg(CFE_MSG_PTR(MemMgr.StatusTlm.TelemetryHeader));
   CFE_SB_TransmitMsg(CFE_MSG_PTR(MemMgr.StatusTlm.TelemetryHeader), true);

} /* End SendStatusTlm() */

