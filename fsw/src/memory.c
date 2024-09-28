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
**    Implement the MEMORY_Class methods
**
**  Notes:
**    1. The platform configurations allow each memory size to be conditionally
**       compiled. Memory size classes are designed as child classes to 
**       memory and static functions serves as virtual method dispatch
**       functions. The conditional compilation switches are located in 
**       these functions and event messages report errors.
**    2. Parameter order convention is Address, Type, Size, Data
**    TODO: Review command function consustency with sucess/fail events and HK tlm
**
*/

/*
** Include Files:
*/

#include <string.h>

#include "memory.h"
#include "mem_size8.h"
#include "mem_size16.h"
#include "mem_size32.h"


/*******************************/
/** Local Function Prototypes **/
/*******************************/

static bool CreateCpuAddr(MEM_MGR_SymbolAddr_t *SymbolAddr, MEM_MGR_CpuAddr_Atom_t *CpuAddr);
static bool FillMemBlock(MEM_MGR_CpuAddr_Atom_t DestAddr, MEM_MGR_MemSize_Enum_t MemSize, uint32 FillData, uint32 ByteCnt);
static bool GetPspMemType(MEM_MGR_MemType_Enum_t MemType, uint32 *PspMemType, char **MemTypeStr);
static bool Peek(MEM_MGR_CpuAddr_Atom_t CpuAddr, MEM_MGR_MemType_Enum_t MemType, const char *MemTypeStr, MEM_MGR_MemSize_Enum_t MemSize);
static bool Poke(MEM_MGR_CpuAddr_Atom_t CpuAddr, MEM_MGR_MemType_Enum_t MemType, const char *MemTypeStr, MEM_MGR_MemSize_Enum_t MemSize, uint32 Data);
static bool ReadMemBlock(void *DestAddr, MEM_MGR_CpuAddr_Atom_t SrcCpuAddr, MEM_MGR_MemSize_Enum_t SrcMemSize, uint32 ByteCnt);
static bool SendDumpBufToEvent(MEM_MGR_CpuAddr_Atom_t CpuAddr, const uint8 *DumpBuf, uint32 ByteCnt);
static bool VerifyCpuAddr(MEM_MGR_CpuAddr_Atom_t CpuAddr, uint32 PspMemType, const char *MemTypeStr, MEM_MGR_MemSize_Enum_t MemSize, uint32 ByteCnt);

/**********************/
/** Global File Data **/
/**********************/

static MEMORY_Class_t *Memory = NULL;

static char MEM_TYPE_EEPROM[] = "EEPROM";
static char MEM_TYPE_RAM[]    = "RAM";
static char MEM_TYPE_UNDEF[]  = "UNDEF";

//TODO: Decide how/where to define DumpToEventBuf[]
static uint32 DumpToEventBuf[MEMORY_DUMP_TOEVENT_MAX_DWORDS];  // Defined to support 32-bit memory dumps


/******************************************************************************
** Function: MEMORY_Constructor
**
*/
void MEMORY_Constructor(MEMORY_Class_t *MemoryPtr)
{
 
   Memory = MemoryPtr;

   CFE_PSP_MemSet((void*)Memory, 0, sizeof(MEMORY_Class_t));
 
   Memory->EepromWriteEna = false;  //TODO: The hardware has not been commanded
   
   // Addr, Data, ByteCnt are zero
   Memory->CmdStatus.Function = MEM_MGR_MemFunction_NONE_PERFORMED;
   Memory->CmdStatus.Type     = MEM_MGR_MemType_UNDEF;
   Memory->CmdStatus.Size     = MEM_MGR_MemSize_UNDEF;
   
} /* End MEMORY_Constructor() */


/******************************************************************************
** Function: MEMORY_DisEepromWriteCmd
**
** Notes:
**   None
**
*/
bool MEMORY_DisEepromWriteCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{
 
   const MEM_MGR_DisEepromWrite_CmdPayload_t *DisEepromWriteCmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_DisEepromWrite_t);
 
   bool   RetStatus = false;
   uint32 PspStatus;


   PspStatus = CFE_PSP_EepromWriteDisable(DisEepromWriteCmd->Bank);
   if (PspStatus == CFE_PSP_SUCCESS)
   {
      RetStatus = true;
      Memory->EepromWriteEna = false;
      CFE_EVS_SendEvent(MEMORY_DIS_EEPROM_WRITE_EID, CFE_EVS_EventType_INFORMATION,
                        "Disabled writing to EEPROM bank %d", (unsigned int)DisEepromWriteCmd->Bank);
   }
   else
   {
      CFE_EVS_SendEvent(MEMORY_DIS_EEPROM_WRITE_EID, CFE_EVS_EventType_ERROR,
                        "Error disabling writes to EEPROM bank %d, status=0x%08X",
                        (unsigned int)DisEepromWriteCmd->Bank, (unsigned int)PspStatus);
   }

   return RetStatus;
    
} /* End MEMORY_DisEepromWriteCmd() */


/******************************************************************************
** Function: MEMORY_DumpToEventCmd
**
**   1. Utility functions send detailed error events and this function sends a 
**      general error event indicating the command failed.
**   2. SendDumpBufToEvent() sends the success event message containing the 
**      contents of the memory block. 
**
*/
bool MEMORY_DumpToEventCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{
 
   const MEM_MGR_DumpToEvent_CmdPayload_t *DumpToEventCmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_DumpToEvent_t);
   
   bool  RetStatus = false;
   MEMORY_VerifiedMemory_t VerifiedMemory;

   RetStatus = MEMORY_VerifyAddr(DumpToEventCmd->SymbolAddr, DumpToEventCmd->MemType, DumpToEventCmd->MemSize,
                                 DumpToEventCmd->ByteCnt, &VerifiedMemory);
   if (RetStatus == true)
   {
         
      RetStatus = ReadMemBlock(DumpToEventBuf, VerifiedMemory.CpuAddr,  
                               DumpToEventCmd->MemSize, DumpToEventCmd->ByteCnt);

      if (RetStatus == true)
      {
         RetStatus = SendDumpBufToEvent(VerifiedMemory.CpuAddr, (const uint8*)DumpToEventBuf, DumpToEventCmd->ByteCnt);
      }
      
   } /* End MEMORY_VerifyAddr()*/

   if (RetStatus == true)
   {
      Memory->CmdStatus.Function = MEM_MGR_MemFunction_DUMP_TO_EVENT;
      Memory->CmdStatus.Type     = DumpToEventCmd->MemType;
      Memory->CmdStatus.Size     = DumpToEventCmd->MemSize;
      Memory->CmdStatus.Addr     = VerifiedMemory.CpuAddr;
      Memory->CmdStatus.Data     = 0;  // TODO: Capture last data byte?
      Memory->CmdStatus.ByteCnt  = DumpToEventCmd->ByteCnt;
   }
      
   return RetStatus;
   
} /* End MEMORY_DumpToEventCmd() */


/******************************************************************************
** Function: MEMORY_EnaEepromWriteCmd
**
** Notes:
**   None
**
*/
bool MEMORY_EnaEepromWriteCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{
 
   const MEM_MGR_EnaEepromWrite_CmdPayload_t *EnaEepromWriteCmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_EnaEepromWrite_t);
 
   bool   RetStatus = false;
   uint32 PspStatus;


   PspStatus = CFE_PSP_EepromWriteEnable(EnaEepromWriteCmd->Bank);
   if (PspStatus == CFE_PSP_SUCCESS)
   {
      RetStatus = true;
      Memory->EepromWriteEna = true;
      CFE_EVS_SendEvent(MEMORY_ENA_EEPROM_WRITE_EID, CFE_EVS_EventType_INFORMATION,
                        "Enabled writing to EEPROM bank %d", (unsigned int)EnaEepromWriteCmd->Bank);
   }
   else
   {
      CFE_EVS_SendEvent(MEMORY_ENA_EEPROM_WRITE_EID, CFE_EVS_EventType_ERROR,
                        "Error enabling writes to EEPROM bank %d, status=0x%08X",
                        (unsigned int)EnaEepromWriteCmd->Bank, (unsigned int)PspStatus);
   }

   return RetStatus;
    
} /* End MEMORY_EnaEepromWriteCmd() */


/******************************************************************************
** Function: MEMORY_FillCmd
**
** Notes:
**   None
**
*/
bool MEMORY_FillCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{

   const MEM_MGR_Fill_CmdPayload_t *FillCmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_Fill_t);
 
   bool RetStatus = false;
   MEMORY_VerifiedMemory_t VerifiedMemory;
   
   RetStatus = MEMORY_VerifyAddr(FillCmd->SymbolAddr, FillCmd->MemType, FillCmd->MemSize,
                                 FillCmd->ByteCnt, &VerifiedMemory);
   if (RetStatus == true)
   {   
      RetStatus = FillMemBlock(VerifiedMemory.CpuAddr, FillCmd->MemSize, FillCmd->Data, FillCmd->ByteCnt);
      if (RetStatus == true)
      {
         Memory->CmdStatus.Function = MEM_MGR_MemFunction_FILL;
         Memory->CmdStatus.Type     = FillCmd->MemType;
         Memory->CmdStatus.Size     = FillCmd->MemSize;
         Memory->CmdStatus.Addr     = VerifiedMemory.CpuAddr;
         Memory->CmdStatus.Data     = FillCmd->Data;
         Memory->CmdStatus.ByteCnt  = FillCmd->ByteCnt;

         CFE_EVS_SendEvent(MEMORY_FILL_CMD_EID, CFE_EVS_EventType_INFORMATION,
                           "Successfully filled %d bytes of memory with %d starting at %p", 
                           (int)FillCmd->ByteCnt, FillCmd->Data, (void *)VerifiedMemory.CpuAddr);

      }
      
   } /* End MEMORY_VerifyAddr()*/
   
   return RetStatus;
   
} /* MEMORY_FillCmd() */


/******************************************************************************
** Function: MEMORY_LoadWithIntDisCmd
**
** Notes:
**   None
**
*/
bool MEMORY_LoadWithIntDisCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{
 
   const MEM_MGR_LoadWithIntDis_CmdPayload_t *LoadWithIntDisCmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_LoadWithIntDis_t);
 
   bool    RetStatus = false;
   uint32  PspStatus;
   uint32  ComputedCRC;
   MEMORY_VerifiedMemory_t VerifiedMemory;
   
   RetStatus = MEMORY_VerifyAddr(LoadWithIntDisCmd->SymbolAddr, LoadWithIntDisCmd->MemType, LoadWithIntDisCmd->MemSize,
                                 LoadWithIntDisCmd->ByteCnt, &VerifiedMemory);
   if (RetStatus == true)
   {
         
      ComputedCRC = CFE_ES_CalculateCRC(LoadWithIntDisCmd->Data, LoadWithIntDisCmd->ByteCnt, 0, LoadWithIntDisCmd->CrcType);

      if (ComputedCRC == LoadWithIntDisCmd->Crc)
      {

         PspStatus = CFE_PSP_MemCpy((void*)VerifiedMemory.CpuAddr, (void*)LoadWithIntDisCmd->Data, LoadWithIntDisCmd->ByteCnt);
         
         if (PspStatus == CFE_PSP_SUCCESS)
         {
            RetStatus = true;

            Memory->CmdStatus.Function = MEM_MGR_MemFunction_LOAD_INT_DIS;
            Memory->CmdStatus.Type     = LoadWithIntDisCmd->MemType;
            Memory->CmdStatus.Size     = LoadWithIntDisCmd->MemSize;
            Memory->CmdStatus.Addr     = VerifiedMemory.CpuAddr;
            Memory->CmdStatus.Data     = 0;  // TODO: Capture last data byte?
            Memory->CmdStatus.ByteCnt  = LoadWithIntDisCmd->ByteCnt;

            CFE_EVS_SendEvent(MEMORY_LOAD_INT_DIS_EID, CFE_EVS_EventType_INFORMATION,
                              "Load memory with interrupts disabled: Wrote %d bytes to address: %p", 
                              (int)LoadWithIntDisCmd->ByteCnt, (void *)VerifiedMemory.CpuAddr);

         }
         else
         {
            CFE_EVS_SendEvent(MEMORY_LOAD_INT_DIS_EID, CFE_EVS_EventType_ERROR,
                              "Load memory with interrupts disabled copy failed for address %p, status=0x%08X",
                              (void *)VerifiedMemory.CpuAddr, (unsigned int)PspStatus);
         }
      } /* End valid CRC */
      else
      {

         CFE_EVS_SendEvent(MEMORY_LOAD_INT_DIS_EID, CFE_EVS_EventType_ERROR,
                           "Load memory with interrupts disabled CRC failed: Expected = 0x%X Calculated = 0x%X",
                           (unsigned int)LoadWithIntDisCmd->Crc, (unsigned int)ComputedCRC);

      } /* End invalid CRC */      
   } /* End MEMORY_VerifyAddr()*/
 
   return RetStatus;
   
} /* End MEMORY_LoadWithIntDisCmd() */


/******************************************************************************
** Function: MEMORY_LookupSymbolCmd
**
** Notes:
**   None
**
*/
bool MEMORY_LookupSymbolCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{
   
   const MEM_MGR_LookupSymbol_CmdPayload_t *LookupSymbolCmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_LookupSymbol_t);

   bool    RetStatus = false;
   int32   OsStatus;
   char    SymbolName[MEM_MGR_MAX_SYM_LEN];
   cpuaddr ResolvedAddr;


   // Copy and verify valid string from command message
   CFE_SB_MessageStringGet(SymbolName, LookupSymbolCmd->Name, NULL, sizeof(SymbolName), sizeof(LookupSymbolCmd->Name));
OS_printf("Cmd Symbol: %s, Local Symbol: %s\n",LookupSymbolCmd->Name,SymbolName);
   if (MEM_MGR_strnlen(SymbolName, MEM_MGR_MAX_SYM_LEN) == 0)
   {
      CFE_EVS_SendEvent(MEMORY_LOOKUP_SYMBOL_EID, CFE_EVS_EventType_ERROR,
                        "Lookup symbol command error, empty string");
   }
   else
   {
      OsStatus = OS_SymbolLookup(&ResolvedAddr, SymbolName);
      if (OsStatus == OS_SUCCESS)
      {
         RetStatus = true;
         CFE_EVS_SendEvent(MEMORY_LOOKUP_SYMBOL_EID, CFE_EVS_EventType_INFORMATION,
                           "Lookup symbol command: Name='%s' Addr=%p", SymbolName, (void *)ResolvedAddr);
      }
      else
      {
         CFE_EVS_SendEvent(MEMORY_LOOKUP_SYMBOL_EID, CFE_EVS_EventType_ERROR,
                           "Lookup symbol %s command error, symbolic address not resolved, status=0x%08X",
                           SymbolName, (unsigned int)OsStatus);
      }

   } /* End if non-zero synmbol name */  

   return RetStatus;

} /* End MEMORY_LookupSymbolCmd() */


/******************************************************************************
** Function: MEMORY_PeekCmd
**
** Notes:
**   1. Utility functions send detailed error events and this function sends a 
**      general error event indicating the command failed.
**   2. PeekCmd() sends the success event message containing the contents of
**      the memory location. 
**
*/
bool MEMORY_PeekCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{
 
   const MEM_MGR_Peek_CmdPayload_t *PeekCmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_Peek_t);
 
   bool RetStatus = false;
   MEMORY_VerifiedMemory_t VerifiedMemory;

   // MemSize enumeration value is used for the number of bytes parameter
   RetStatus = MEMORY_VerifyAddr(PeekCmd->SymbolAddr, PeekCmd->MemType, PeekCmd->MemSize,
                                 PeekCmd->MemSize, &VerifiedMemory);
   if (RetStatus == true)
   {
      RetStatus = Peek(VerifiedMemory.CpuAddr, PeekCmd->MemType, VerifiedMemory.TypeStr, PeekCmd->MemSize);

      if (RetStatus == false)
      {
         CFE_EVS_SendEvent(MEMORY_PEEK_CMD_EID, CFE_EVS_EventType_ERROR,
                           "Memory Manager Peek command failed for address %p", (void*)VerifiedMemory.CpuAddr);   
      }
   }
   
   return RetStatus;
    
} /* End MEMORY_PeekCmd() */


/******************************************************************************
** Function: MEMORY_PokeCmd
**
** Notes:
**   1. Utility functions send detailed error events and this function sends a 
**      general error event indicating the command failed.
**
*/
bool MEMORY_PokeCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{
 
   const MEM_MGR_Poke_CmdPayload_t *PokeCmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_Poke_t);
 
   bool RetStatus = false;
   MEMORY_VerifiedMemory_t VerifiedMemory;

   // MemSize enumeration value is used for the number of bytes parameter
   RetStatus = MEMORY_VerifyAddr(PokeCmd->SymbolAddr, PokeCmd->MemType, PokeCmd->MemSize,
                                 PokeCmd->MemSize, &VerifiedMemory);
   if (RetStatus == true)
   {
      RetStatus = Poke(VerifiedMemory.CpuAddr, PokeCmd->MemType, VerifiedMemory.TypeStr, PokeCmd->MemSize, PokeCmd->Data);
      if (RetStatus != true)
      {
         CFE_EVS_SendEvent(MEMORY_POKE_CMD_EID, CFE_EVS_EventType_ERROR,
                           "Memory Manager Poke command failed for address %p", (void*)VerifiedMemory.CpuAddr);     
      }
   }
   
   return RetStatus;
    
} /* End MEMORY_PokeCmd() */


/******************************************************************************
** Function:  MEMORY_ResetStatus
**
*/
void MEMORY_ResetStatus(void)
{
   //TODO: Should anything be reset?
   
} /* End MEMORY_ResetStatus() */


/******************************************************************************
** Function: MEMORY_SetCmdStatus
**
** Notes:
**   1. This is used by objects that a 'uses a' MEMORY object relationship.
**      They use memory child objects to perform memory operations for commands
**      and then call this function to update the MEMORY command status. This
**      approach overdesigning an elaborate and complicated OO design in C.  
**
*/
void MEMORY_SetCmdStatus(const MEMORY_CmdStatus_t *CmdStatus)
{
   
   memcpy(&Memory->CmdStatus, CmdStatus, sizeof(MEMORY_CmdStatus_t));
   
} /* MEMORY_SetCmdStatus() */


/******************************************************************************
** Function: MEMORY_VerifyAddr
**
** Notes:
**   1. This is the top-level address verification function that is called by
**      command functions.
**
*/
bool MEMORY_VerifyAddr(MEM_MGR_SymbolAddr_t SymbolAddr, MEM_MGR_MemType_Enum_t MemType,
                       MEM_MGR_MemSize_Enum_t MemSize, uint32 ByteCnt, 
                       MEMORY_VerifiedMemory_t *VerifiedMemory)
{
   
   bool                  RetStatus = false;
   MEM_MGR_SymbolAddr_t  LocalSymbolAddr;
   uint32                PspMemType;

   VerifiedMemory->CpuAddr = 0;
   VerifiedMemory->TypeStr = MEM_TYPE_UNDEF;
   
   // Create local SymbolAddr copy since it may get modified
   LocalSymbolAddr = SymbolAddr;

   RetStatus = CreateCpuAddr(&LocalSymbolAddr, &(VerifiedMemory->CpuAddr));
   if (RetStatus == true)
   {
      
      RetStatus = GetPspMemType(MemType, &PspMemType, &(VerifiedMemory->TypeStr));
      if (RetStatus == true)
      {
         
         RetStatus = VerifyCpuAddr(VerifiedMemory->CpuAddr, PspMemType, VerifiedMemory->TypeStr, MemSize, ByteCnt);

      } /* End if got PSP mem type */

   } /* End if created CPU address */

   return RetStatus;
   
} /* End MEMORY_VerifyAddr() */


/******************************************************************************
** Function: CreateCpuAddr
**
** Notes:
**   1. Callers assumes error events are sent containing details of the error.
**
*/
static bool CreateCpuAddr(MEM_MGR_SymbolAddr_t *SymbolAddr, MEM_MGR_CpuAddr_Atom_t *CpuAddr)
{

   bool  RetStatus = false;
   int32 OsStatus;

   // NULL terminate SymbolName as precaution since orginated from ground command
   SymbolAddr->Name[MEM_MGR_MAX_SYM_LEN - 1] = '\0';

   // If SymbolName string is NULL then use Offset as the absolute address
   if (MEM_MGR_strnlen(SymbolAddr->Name, MEM_MGR_MAX_SYM_LEN) == 0)
   {
      *CpuAddr = SymbolAddr->Offset;
      RetStatus = true;
   }
   else
   {
      // If SymbolName string is not NULL then use offset is applied to symbol address
      OsStatus = OS_SymbolLookup(CpuAddr, SymbolAddr->Name);
      if (OsStatus == OS_SUCCESS)
      {
         *CpuAddr += SymbolAddr->Offset;
         RetStatus = true;
      }
      else
      {
         CFE_EVS_SendEvent(MEMORY_CREATE_CPU_ADDR_EID, CFE_EVS_EventType_ERROR,
                          "OS symbol lookup failed for %s, status=%d",
                           SymbolAddr->Name, OsStatus);
      }
   } /* End if non-null symbol name */
    
   return RetStatus;

} /* End CreateCpuAddr() */


/******************************************************************************
** Function: FillMemBlock
**
** Notes:
**   None
**
*/
static bool FillMemBlock(MEM_MGR_CpuAddr_Atom_t DestAddr, MEM_MGR_MemSize_Enum_t MemSize,
                         uint32 FillData, uint32 ByteCnt)
{

   bool   RetStatus = false;
   int32  PspStatus;
   
   switch (MemSize)
   {
      case MEM_MGR_MemSize_8:
         RetStatus = MEM_SIZE8_FillBlock((uint8*)DestAddr, (uint8)FillData, ByteCnt);
         break;
      case MEM_MGR_MemSize_16:
         RetStatus = MEM_SIZE16_FillBlock((uint16*)DestAddr, (uint16)FillData, ByteCnt/2);
         break;
      case MEM_MGR_MemSize_32:
         RetStatus = MEM_SIZE32_FillBlock((uint32*)DestAddr, FillData, ByteCnt/4);
         break;
      case MEM_MGR_MemSize_VOID:
         PspStatus = CFE_PSP_MemSet((void*)DestAddr, (uint8)FillData, ByteCnt);
         RetStatus = (PspStatus == CFE_PSP_SUCCESS);
         //TODO: Event
         break;
      default:
         //TODO: Event
         break;
   } /* End mem size switch */

   return RetStatus;
    
} /* End FillMemBlock() */


/******************************************************************************
** Function: GetPspMemType
**
** Notes:
**   1. Convert MEM_MGR's memory type definitions into PSP memory type
**      defnition
**
*/
static bool GetPspMemType(MEM_MGR_MemType_Enum_t MemType, uint32 *PspMemType, char **MemTypeStr)
{
   bool   RetStatus = false;
   
   *PspMemType = CFE_PSP_MEM_INVALID;
   *MemTypeStr = MEM_TYPE_UNDEF;

   switch (MemType)
   {
      case MEM_MGR_MemType_NONVOL:
         RetStatus   = true;
         *PspMemType = CFE_PSP_MEM_EEPROM;
         *MemTypeStr = MEM_TYPE_EEPROM;
         break;
      case MEM_MGR_MemType_RAM:
         RetStatus   = true;
         *PspMemType = CFE_PSP_MEM_RAM;
         *MemTypeStr = MEM_TYPE_RAM;
         break;
      default:
         CFE_EVS_SendEvent(MEMORY_GET_PSP_MEM_TYPE_EID, CFE_EVS_EventType_ERROR,
                          "Invalid memory type %u received", MemType);      
         break;
   } /* End mem type switch */

   return RetStatus;

} /* End GetPspMemType() */


/******************************************************************************
** Function: Peek
**
** Notes:
**   1. After all command validation is performed, this function is called to
**      do the memory peek and sends the command's success event message
**   2. From an OO design perspective this is a virtual function dispatcher
**
*/
static bool Peek(MEM_MGR_CpuAddr_Atom_t CpuAddr, MEM_MGR_MemType_Enum_t MemType, 
                 const char *MemTypeStr, MEM_MGR_MemSize_Enum_t MemSize)
{

   bool   RetStatus = false;
   uint32 Data      = 0;
   uint32 ByteCnt   = 0;

   //TODO: Report invalid memory types for peeks
   switch (MemSize)
   {
      case MEM_MGR_MemSize_8:
         ByteCnt = 1;
         RetStatus = MEM_SIZE8_Peek((uint8*)CpuAddr, (uint8*)&Data);
         break;
      case MEM_MGR_MemSize_16:
         ByteCnt = 2;
         RetStatus = MEM_SIZE16_Peek((uint16*)CpuAddr, (uint16*)&Data);
         break;
      case MEM_MGR_MemSize_32:
         ByteCnt = 4;
         RetStatus = MEM_SIZE32_Peek((uint32*)CpuAddr, &Data);
         break;
      default:
         break;
   } /* End mem size switch */
   
   //TODO: Set peek status in the main command function. Make all commands consistent. Think about sucess event message  
   if (RetStatus == true)
   {
      Memory->CmdStatus.Function = MEM_MGR_MemFunction_PEEK;
      Memory->CmdStatus.Type     = MemType;
      Memory->CmdStatus.Size     = MemSize;
      Memory->CmdStatus.Addr     = CpuAddr;
      Memory->CmdStatus.Data     = Data;
      Memory->CmdStatus.ByteCnt  = ByteCnt;      

      CFE_EVS_SendEvent(MEMORY_PEEK_CMD_EID, CFE_EVS_EventType_INFORMATION,
                        "Peek %s Cmd: Addr=%p, Bytes=%u, Data=0x%08X",
                        MemTypeStr, (void*)CpuAddr, ByteCnt, Data);
   }
   
   return RetStatus;
    
} /* End Peek() */


/******************************************************************************
** Function: Poke
**
** Notes:
**   1. After all command validation is performed, this function is called to
**      do the memory poke and sends the command's success event message
**   2. From an OO design perspective this is a virtual function dispatcher
**
*/
static bool Poke(MEM_MGR_CpuAddr_Atom_t CpuAddr, MEM_MGR_MemType_Enum_t MemType, 
                 const char *MemTypeStr, MEM_MGR_MemSize_Enum_t MemSize, uint32 Data)
{

   bool    RetStatus = false;
   uint32  ByteCnt = 0;

   //TODO: Report invalid memory types for pokes
   switch (MemSize)
   {
      case MEM_MGR_MemSize_8:
         ByteCnt = 1;
         RetStatus = MEM_SIZE8_Poke((uint8*)CpuAddr, MemType, MemTypeStr, (uint8)Data);
         break;
      case MEM_MGR_MemSize_16:
         ByteCnt = 2;
         RetStatus = MEM_SIZE16_Poke((uint16*)CpuAddr, MemType, MemTypeStr, (uint16)Data);
         break;
      case MEM_MGR_MemSize_32:
         ByteCnt = 4;
         RetStatus = MEM_SIZE32_Poke((uint32*)CpuAddr, MemType, MemTypeStr, Data);
         break;
      default:
         break;
   } /* End mem size switch */
   
   if (RetStatus == true)
   {
      Memory->CmdStatus.Function  = MEM_MGR_MemFunction_PEEK;
      Memory->CmdStatus.Type      = MemType;
      Memory->CmdStatus.Size      = MemSize;
      Memory->CmdStatus.Addr      = CpuAddr;
      Memory->CmdStatus.Data      = Data;
      Memory->CmdStatus.ByteCnt   = ByteCnt;      

      CFE_EVS_SendEvent(MEMORY_PEEK_CMD_EID, CFE_EVS_EventType_INFORMATION,
                        "Poke %s Cmd: Addr=%p, Bytes=%u, Data=0x%08X",
                        MemTypeStr, (void*)CpuAddr, ByteCnt, Data);
   }
   
   return RetStatus;
    
} /* End Poke() */


/******************************************************************************
** Function: ReadMemBlock
**
** Notes:
**   1. Copy a block of memory from a memory type/size to a local RAM buffer.
**      This function is typically used for commanded memory types/sizes.
**   2. From an OO design perspective this is a virtual function dispatcher
**
*/
static bool ReadMemBlock(void *DestAddr, MEM_MGR_CpuAddr_Atom_t SrcCpuAddr,  
                         MEM_MGR_MemSize_Enum_t SrcMemSize, uint32 ByteCnt)
{

   bool   RetStatus = false;
   int32  PspStatus;
   
   switch (SrcMemSize)
   {
      case MEM_MGR_MemSize_8:
         RetStatus = MEM_SIZE8_ReadBlock((uint8*)SrcCpuAddr, (uint8*)DestAddr, ByteCnt);
         break;
      case MEM_MGR_MemSize_16:
         RetStatus = MEM_SIZE16_ReadBlock((uint16*)SrcCpuAddr, (uint16*)DestAddr, ByteCnt/2);
         break;
      case MEM_MGR_MemSize_32:
         RetStatus = MEM_SIZE32_ReadBlock((uint32*)SrcCpuAddr, (uint32*)DestAddr, ByteCnt/4);
         break;
      case MEM_MGR_MemSize_VOID:
         PspStatus = CFE_PSP_MemCpy((void*)DestAddr, (void*)SrcCpuAddr, ByteCnt);
         RetStatus = (PspStatus == CFE_PSP_SUCCESS);
         //TODO: Event
         break;
      default:
         //TODO: Event
         break;
   } /* End mem size switch */

   return RetStatus;
    
} /* End ReadMemBlock() */


/******************************************************************************
** Function: SendDumpBufToEvent
**
** Notes:
**   1. Build and send the event message containing the dump data
**   2. Refer to app_cfg.h's macro definition comments for 
**
*/
static bool SendDumpBufToEvent(MEM_MGR_CpuAddr_Atom_t CpuAddr, const uint8 *DumpBuf, uint32 ByteCnt)
{

   bool   RetStatus = false;

   const char  EventHdrStr[] = MEMORY_DUMP_TOEVENT_HDR_STR;
   static char EventStr[CFE_MISSION_EVS_MAX_MESSAGE_LENGTH];
   char        TempStr[MEMORY_DUMP_TOEVENT_TEMP_CHARS];
   int32       EventStrTotalLen = 0;
   uint8*      EventStrBytePtr;
   uint32      i;
      
  
   strncpy(EventStr, EventHdrStr, sizeof(EventStr));
   EventStrTotalLen = MEM_MGR_strnlen(EventStr, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH);

   EventStrBytePtr = (uint8*)DumpToEventBuf;
   for (i=0; i < ByteCnt; i++)
   {
      // No need to check snprintf return, CFE_SB_MessageStringGet() handles safe concatenation & prevents overflow
      snprintf(TempStr, MEMORY_DUMP_TOEVENT_TEMP_CHARS, "0x%02X ", *EventStrBytePtr);
      CFE_SB_MessageStringGet(&EventStr[EventStrTotalLen], TempStr, NULL,sizeof(EventStr)-EventStrTotalLen, sizeof(TempStr));
      EventStrTotalLen = MEM_MGR_strnlen(EventStr, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH);
      EventStrBytePtr++;
   } 
   
   /* 
   ** Append tail
   ** This adds up to 33 characters depending on pointer representation including the NUL terminator
   ** SAD: No need to check snprintf return; CFE_SB_MessageStringGet() handles safe concatenation and
   ** prevents overflow
   */
   snprintf(TempStr, MEMORY_DUMP_TOEVENT_TEMP_CHARS, MEMORY_DUMP_TOEVENT_TRAILER_STR, (void *)CpuAddr);
   CFE_SB_MessageStringGet(&EventStr[EventStrTotalLen], TempStr, NULL,
                           sizeof(EventStr) - EventStrTotalLen, sizeof(TempStr));

   CFE_EVS_SendEvent(MEMORY_DUMP_TO_EVENT_EID, CFE_EVS_EventType_INFORMATION, "%s", EventStr);

   return RetStatus;

} /* End SendDumpBufToEvent() */


/******************************************************************************
** Function: VerifyCpuAddr
**
** Notes:
**   1. Callers assumes error events are sent containing details of the error
**   2. From an OO design perspective this is a virtual function dispatcher
**
*/
static bool VerifyCpuAddr(MEM_MGR_CpuAddr_Atom_t CpuAddr, uint32 PspMemType,
                          const char *MemTypeStr, MEM_MGR_MemSize_Enum_t MemSize, uint32 ByteCnt)
{
   bool   RetStatus = false;

   switch (MemSize)
   {
      case MEM_MGR_MemSize_8:
         RetStatus = MEM_SIZE8_VerifyCpuAddr((uint8*)CpuAddr, PspMemType, MemTypeStr, ByteCnt);
         break;
      case MEM_MGR_MemSize_16:
         RetStatus = MEM_SIZE16_VerifyCpuAddr((uint16*)CpuAddr, PspMemType, MemTypeStr, ByteCnt);
         break;
      case MEM_MGR_MemSize_32:
         RetStatus = MEM_SIZE32_VerifyCpuAddr((uint32*)CpuAddr, PspMemType, MemTypeStr, ByteCnt);
         break;
      default:
         break;
   } /* End mem size switch */


   return RetStatus;

} /* End VerifyCpuAddr() */
 