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
**    TODO: Review command function consistency with sucess/fail events and HK tlm
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
static bool FillMemBlock(MEM_MGR_CpuAddr_Atom_t DstAddr, MEM_MGR_MemSize_Enum_t MemSize, uint32 FillData, uint32 ByteCnt);
static bool GetPspMemType(MEM_MGR_MemType_Enum_t MemType, uint32 *PspMemType, const char **MemTypeStr);
static bool ReadMemBlock(void *DstAddr, MEM_MGR_CpuAddr_Atom_t SrcCpuAddr, MEM_MGR_MemSize_Enum_t SrcMemSize, uint32 ByteCnt);
static bool SendDumpBufToEvent(MEM_MGR_CpuAddr_Atom_t CpuAddr, const uint8 *DumpBuf, uint32 ByteCnt);
static bool VerifyCpuAddr(MEM_MGR_CpuAddr_Atom_t CpuAddr, uint32 PspMemType, const char *MemTypeStr, MEM_MGR_MemSize_Enum_t MemSize, uint32 ByteCnt);

/**********************/
/** Global File Data **/
/**********************/

static MEMORY_Class_t *Memory = NULL;

// MEM_MGR_MemType_Enum_t
static const char *MemTypeStr[] =
{
   "UNDEF",   // 0 = MEM_MGR_MemType_UNDEF
   "RAM",     // 1 = MEM_MGR_MemType_RAM
   "NONVOL",  // 2 = MEM_MGR_MemType_NONVOL
   "INVALID"  // 3 = Outisde enumeration range
};

// MEM_MGR_MemSize_Enum_t
static const char *MemSizeStr[] =
{
   "UNDEF",   // 0 = MEM_MGR_MemSize_UNDEF
   "8",       // 1 = MEM_MGR_MemType_8
   "16",      // 2 = MEM_MGR_MemType_16
   "INVALID", // 3 = Unused
   "32",      // 4 = MEM_MGR_MemType_32
   "VOID"     // 5 = MEM_MGR_MemType_VOID
};

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
         
      RetStatus = ReadMemBlock(Memory->DumpToEventBuf, VerifiedMemory.CpuAddr,  
                               DumpToEventCmd->MemSize, DumpToEventCmd->ByteCnt);

      if (RetStatus == true)
      {
         RetStatus = SendDumpBufToEvent(VerifiedMemory.CpuAddr, (const uint8*)Memory->DumpToEventBuf, DumpToEventCmd->ByteCnt);
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
   else
   {
      CFE_EVS_SendEvent(MEMORY_DUMP_TO_EVENT_EID, CFE_EVS_EventType_ERROR,
                        "Dump memory to an event message command failed. See accompanying event for details");
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
                           "Successfully filled %d bytes of memory with 0x%08X starting at %p", 
                           (int)FillCmd->ByteCnt, FillCmd->Data, (void *)VerifiedMemory.CpuAddr);

      }
      else
      {
         CFE_EVS_SendEvent(MEMORY_FILL_CMD_EID, CFE_EVS_EventType_ERROR,
                           "Fill memory command failed. See accompanying event for details");
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
                              "Load memory with interrupts disabled complete: Wrote %d bytes to address: %p", 
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
                           "Load memory with interrupts disabled CRC failed: Expected = 0x%08X Calculated = 0x%08X",
                           (unsigned int)LoadWithIntDisCmd->Crc, (unsigned int)ComputedCRC);

      } /* End invalid CRC */      
   } /* End MEMORY_VerifyAddr()*/
   else
   {
      CFE_EVS_SendEvent(MEMORY_LOAD_INT_DIS_EID, CFE_EVS_EventType_ERROR,
                        "Load memory with interrupts command failed. See accompanying event for details");
   }
 
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
                           "Lookup symbol command complete: Name='%s' Addr=%p", SymbolName, (void *)ResolvedAddr);
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
 
   bool   RetStatus = false;
   uint32 Data=0;
   uint8  ByteCnt;
   MEMORY_VerifiedMemory_t VerifiedMemory;

   // MemSize enumeration value is used for the number of bytes parameter
   RetStatus = MEMORY_VerifyAddr(PeekCmd->SymbolAddr, PeekCmd->MemType, PeekCmd->MemSize,
                                 PeekCmd->MemSize, &VerifiedMemory);
   if (RetStatus == true)
   {
      ByteCnt = MEMORY_Read(VerifiedMemory, PeekCmd->MemSize, &Data);
      if (ByteCnt > 0)
      {
         RetStatus = true;
         Memory->CmdStatus.Function = MEM_MGR_MemFunction_PEEK;
         Memory->CmdStatus.Type     = PeekCmd->MemType;
         Memory->CmdStatus.Size     = PeekCmd->MemSize;
         Memory->CmdStatus.Addr     = VerifiedMemory.CpuAddr;
         Memory->CmdStatus.Data     = Data;
         Memory->CmdStatus.ByteCnt  = ByteCnt;      

         CFE_EVS_SendEvent(MEMORY_PEEK_CMD_EID, CFE_EVS_EventType_INFORMATION,
                           "Peek %s memory command complete: Addr=%p, Bytes=%u, Data=0x%08X",
                           VerifiedMemory.TypeStr, (void*)VerifiedMemory.CpuAddr,
                           ByteCnt, Data);
      }
      else
      {
         CFE_EVS_SendEvent(MEMORY_PEEK_CMD_EID, CFE_EVS_EventType_ERROR,
                           "Peek memory command failed for address %p",
                           (void*)VerifiedMemory.CpuAddr);   
      }
   } /* End if valid address */
   else
   {
      CFE_EVS_SendEvent(MEMORY_PEEK_CMD_EID, CFE_EVS_EventType_ERROR,
                        "Peek memory command failed. See accompanying event for details");
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
 
   bool  RetStatus = false;
   uint8 ByteCnt;
   MEMORY_VerifiedMemory_t VerifiedMemory;

   // MemSize enumeration value is used for the number of bytes parameter
   RetStatus = MEMORY_VerifyAddr(PokeCmd->SymbolAddr, PokeCmd->MemType, PokeCmd->MemSize,
                                 PokeCmd->MemSize, &VerifiedMemory);
   if (RetStatus == true)
   {
      ByteCnt = MEMORY_Write(VerifiedMemory, PokeCmd->MemType, VerifiedMemory.TypeStr,
                             PokeCmd->MemSize, PokeCmd->Data);
      if (ByteCnt > 0)
      {
         RetStatus = true;
         Memory->CmdStatus.Function  = MEM_MGR_MemFunction_POKE;
         Memory->CmdStatus.Type      = PokeCmd->MemType;
         Memory->CmdStatus.Size      = PokeCmd->MemSize;
         Memory->CmdStatus.Addr      = VerifiedMemory.CpuAddr;
         Memory->CmdStatus.Data      = PokeCmd->Data;
         Memory->CmdStatus.ByteCnt   = ByteCnt;      

         CFE_EVS_SendEvent(MEMORY_POKE_CMD_EID, CFE_EVS_EventType_INFORMATION,
                           "Poke %s memory command complete: Addr=%p, Bytes=%u, Data=0x%08X",
                           VerifiedMemory.TypeStr, (void*)VerifiedMemory.CpuAddr,
                           ByteCnt, PokeCmd->Data);
      }
      else
      {   

         CFE_EVS_SendEvent(MEMORY_POKE_CMD_EID, CFE_EVS_EventType_ERROR,
                           "Poke memory command failed for address %p",
                           (void*)VerifiedMemory.CpuAddr);     
      }
   } /* End if valid address */
   else
   {
      CFE_EVS_SendEvent(MEMORY_POKE_CMD_EID, CFE_EVS_EventType_ERROR,
                        "Poke memory command failed. See accompanying event for details");
   }
   
   return RetStatus;
    
} /* End MEMORY_PokeCmd() */


/******************************************************************************
** Function: MEMORY_Read
**
** Notes:
**   1. After all command validation is performed, this function is called to
**      do the memory read.
**   2. From an OO design perspective this is a virtual function dispatcher
**      for memory types.
**
*/
uint8 MEMORY_Read(MEMORY_VerifiedMemory_t VerifiedMemory, MEM_MGR_MemSize_Enum_t MemSize,
                  uint32 *Data)
{

   uint32  ByteCnt = 0;
   bool    ValidRead = false;
   
   switch (MemSize)
   {
      case MEM_MGR_MemSize_8:
         ByteCnt = 1;
         ValidRead = MEM_SIZE8_Read((uint8*)VerifiedMemory.CpuAddr, (uint8*)Data);
         break;
      case MEM_MGR_MemSize_16:
         ByteCnt = 2;
         ValidRead = MEM_SIZE16_Read((uint16*)VerifiedMemory.CpuAddr, (uint16*)Data);
         break;
      case MEM_MGR_MemSize_32:
         ByteCnt = 4;
         ValidRead = MEM_SIZE32_Read((uint32*)VerifiedMemory.CpuAddr, Data);
         break;
      default:
         ValidRead = true; // Avoid read error event
         CFE_EVS_SendEvent(MEMORY_READ_EID, CFE_EVS_EventType_ERROR,
                           "Memory read invalid memory size %d; see EDS MemSize definition",
                           MemSize);
                              
         break;
   } /* End mem size switch */
   
   if (!ValidRead)
   {
      CFE_EVS_SendEvent(MEMORY_READ_EID, CFE_EVS_EventType_ERROR,
                        "Unsuccessful %d byte memory read from %s",
                        MemSize, VerifiedMemory.TypeStr);       
   }
   
   return ByteCnt;
    
} /* End MEMORY_Read() */


/******************************************************************************
** Function:  MEMORY_ResetStatus
**
*/
void MEMORY_ResetStatus(void)
{
   
   CFE_PSP_MemSet((void*)&Memory->CmdStatus, 0, sizeof(MEMORY_CmdStatus_t));

   Memory->CmdStatus.Function = MEM_MGR_MemFunction_NONE_PERFORMED;
   Memory->CmdStatus.Type     = MEM_MGR_MemType_UNDEF;
   Memory->CmdStatus.Size     = MEM_MGR_MemSize_UNDEF;

} /* End MEMORY_ResetStatus() */


/******************************************************************************
** Function: MEMORY_SetCmdStatus
**
** Notes:
**   1. This is used by objects that a 'uses a' MEMORY object relationship.
**      They use memory child objects to perform memory operations for commands
**      and then call this function to update the MEMORY command status.
**
*/
void MEMORY_SetCmdStatus(const MEMORY_CmdStatus_t *CmdStatus)
{
   
   memcpy(&Memory->CmdStatus, CmdStatus, sizeof(MEMORY_CmdStatus_t));
   
} /* MEMORY_SetCmdStatus() */


/******************************************************************************
** Function: MEMORY_SizeStr
**
** Notes:
**   1. Returns a pointer to a string for each enumeration in 
**      MEM_MGR_MemSize_Enum_t.
**   2. Enumeration names describes bit size and the enumeration value defines
**      number of bytes, except for UNDEF and VOID
**
*/
const char *MEMORY_SizeStr(MEM_MGR_MemSize_Enum_t MemSize)
{
   uint8 i = 3; // Unused enumeration value that is used to report invalid definitions
   
   if (MemSize >= MEM_MGR_MemSize_Enum_t_MIN && MemSize <= MEM_MGR_MemSize_Enum_t_MAX)
      i = MemSize;
   
   return MemSizeStr[i];
   
} /* MEMORY_SizeStr() */


/******************************************************************************
** Function: MEMORY_TypeStr
**
** Notes:
**   1. Returns a pointer to a string for each enumeration in
**      MEM_MGR_MemType_Enum_t  
**
*/
const char *MEMORY_TypeStr(MEM_MGR_MemType_Enum_t MemType)
{
   uint8 i = 0;
   
   if (MemType >= MEM_MGR_MemType_Enum_t_MIN && MemType <= MEM_MGR_MemType_Enum_t_MAX)
      i = MemType - 1;
  
   return MemTypeStr[i];
   
} /* MEMORY_TypeStr() */


/******************************************************************************
** Function: MEMORY_VerifyAddr
**
** Notes:
**   1. This is the top-level address verification function that is called by
**      command functions.
**   1. Callers assume this functions sends error events and this function
**      assumes the functions called send error events.
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
   VerifiedMemory->TypeStr = MEMORY_TypeStr(MEM_MGR_MemType_UNDEF);
   
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
** Function: MEMORY_Write
**
** Notes:
**   1. After all command validation is performed, this function is called to
**      do the memory write
**   2. From an OO design perspective this is a virtual function dispatcher
**      for memory types.
**
*/
uint8 MEMORY_Write(MEMORY_VerifiedMemory_t VerifiedMemory, MEM_MGR_MemType_Enum_t MemType, 
                   const char *MemTypeStr, MEM_MGR_MemSize_Enum_t MemSize, uint32 Data)
{

   uint32  ByteCnt = 0;
   bool    ValidWrite = false;

   switch (MemSize)
   {
      case MEM_MGR_MemSize_8:
         ByteCnt = 1;
         ValidWrite = MEM_SIZE8_Write((uint8*)VerifiedMemory.CpuAddr, MemType, MemTypeStr, (uint8)Data);
         break;
      case MEM_MGR_MemSize_16:
         ByteCnt = 2;
         ValidWrite = MEM_SIZE16_Write((uint16*)VerifiedMemory.CpuAddr, MemType, MemTypeStr, (uint16)Data);
         break;
      case MEM_MGR_MemSize_32:
         ByteCnt = 4;
         ValidWrite = MEM_SIZE32_Write((uint32*)VerifiedMemory.CpuAddr, MemType, MemTypeStr, Data);
         break;
      default:
         ValidWrite = true; // Avoid write error event
         CFE_EVS_SendEvent(MEMORY_WRITE_EID, CFE_EVS_EventType_ERROR,
                           "Memory write invalid memory size %d; see EDS MemSize definition",
                           MemSize);             
         break;
   } /* End mem size switch */
   
   if (!ValidWrite)
   {
      CFE_EVS_SendEvent(MEMORY_WRITE_EID, CFE_EVS_EventType_ERROR,
                        "Unsuccessful %d byte memory write to %s",
                        MemSize, VerifiedMemory.TypeStr);       
   }
   
   return ByteCnt;
    
} /* End MEMORY_Write() */


/******************************************************************************
** Function: CreateCpuAddr
**
** Notes:
**   1. Callers assume error events are sent containing details of the error.
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
      *CpuAddr  = SymbolAddr->Offset;
      RetStatus = true;
   }
   else
   {
      // If SymbolName string is not NULL then add offset to symbol address
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
**   1. Callers assume this functions sends error events and this function
**      assumes the MEM_SIZE*_FillBlock() functions send error events.
**
*/
static bool FillMemBlock(MEM_MGR_CpuAddr_Atom_t DstAddr, MEM_MGR_MemSize_Enum_t MemSize,
                         uint32 FillData, uint32 ByteCnt)
{

   bool   RetStatus = false;
   int32  PspStatus;
   
   switch (MemSize)
   {
      case MEM_MGR_MemSize_8:
         RetStatus = MEM_SIZE8_FillBlock((uint8*)DstAddr, (uint8)FillData, ByteCnt);
         break;
      case MEM_MGR_MemSize_16:
         RetStatus = MEM_SIZE16_FillBlock((uint16*)DstAddr, (uint16)FillData, ByteCnt/2);
         break;
      case MEM_MGR_MemSize_32:
         RetStatus = MEM_SIZE32_FillBlock((uint32*)DstAddr, FillData, ByteCnt/4);
         break;
      case MEM_MGR_MemSize_VOID:
         PspStatus = CFE_PSP_MemSet((void*)DstAddr, (uint8)FillData, ByteCnt);
         RetStatus = (PspStatus == CFE_PSP_SUCCESS);
         if (!RetStatus)
         {
            CFE_EVS_SendEvent(MEMORY_FILL_MEM_BLOCK_EID, CFE_EVS_EventType_ERROR,
                              "Void memory fill block failed at destination address %p, byte count %d, status=0x%08X",
                              (void *)DstAddr, ByteCnt, (unsigned int)PspStatus);  
         }
         break;
      default:
         CFE_EVS_SendEvent(MEMORY_FILL_MEM_BLOCK_EID, CFE_EVS_EventType_ERROR,
                           "Fill memory block invalid memory size %u; see EDS MemSize definition",
                           MemSize);
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
static bool GetPspMemType(MEM_MGR_MemType_Enum_t MemType, uint32 *PspMemType, const char **MemTypeStr)
{
   bool   RetStatus = false;
   
   *PspMemType = CFE_PSP_MEM_INVALID;
   *MemTypeStr = MEMORY_TypeStr(MEM_MGR_MemType_UNDEF);

   switch (MemType)
   {
      case MEM_MGR_MemType_NONVOL:
         RetStatus   = true;
         *PspMemType = CFE_PSP_MEM_EEPROM;
         *MemTypeStr = MEMORY_TypeStr(MEM_MGR_MemType_NONVOL);
         break;
      case MEM_MGR_MemType_RAM:
         RetStatus   = true;
         *PspMemType = CFE_PSP_MEM_RAM;
         *MemTypeStr = MEMORY_TypeStr(MEM_MGR_MemType_RAM);
         break;
      default:
         CFE_EVS_SendEvent(MEMORY_GET_PSP_MEM_TYPE_EID, CFE_EVS_EventType_ERROR,
                          "Invalid memory type %u; see EDS MemType definition",
                          MemType);      
         break;
   } /* End mem type switch */

   return RetStatus;

} /* End GetPspMemType() */


/******************************************************************************
** Function: ReadMemBlock
**
** Notes:
**   1. Copy a block of memory from a memory type/size to a local RAM buffer.
**      This function is typically used for commanded memory types/sizes.
**   2. Callers assume this functions sends error events and this function
**      assumes the MEM_SIZE*_ReadBlock() functions send error events.
**   3. From an OO design perspective this is a virtual function dispatcher
**      for memory types.
**
*/
static bool ReadMemBlock(void *DstAddr, MEM_MGR_CpuAddr_Atom_t SrcCpuAddr,  
                         MEM_MGR_MemSize_Enum_t SrcMemSize, uint32 ByteCnt)
{

   bool   RetStatus = false;
   int32  PspStatus;
   void   *SrcAddr = (uint8 *)SrcCpuAddr;
   void   **SrcAddrPtr = &SrcAddr;
   
   switch (SrcMemSize)
   {
      case MEM_MGR_MemSize_8:
         RetStatus = MEM_SIZE8_ReadBlock((uint8*)DstAddr, (uint8**)SrcAddrPtr, ByteCnt);
         break;
      case MEM_MGR_MemSize_16:
         RetStatus = MEM_SIZE16_ReadBlock((uint16*)DstAddr, (uint16**)SrcAddrPtr, ByteCnt/2);
         break;
      case MEM_MGR_MemSize_32:
         RetStatus = MEM_SIZE32_ReadBlock((uint32*)DstAddr, (uint32**)SrcAddrPtr, ByteCnt/4);
         break;
      case MEM_MGR_MemSize_VOID:
         PspStatus = CFE_PSP_MemCpy((void*)DstAddr, (void*)SrcAddrPtr, ByteCnt);
         SrcAddr = (uint8*)SrcAddr + ByteCnt;
         RetStatus = (PspStatus == CFE_PSP_SUCCESS);
         if (!RetStatus)
         {
            CFE_EVS_SendEvent(MEMORY_READ_MEM_BLOCK_EID, CFE_EVS_EventType_ERROR,
                           "Void memory read block failed at destination address %p, byte count %d, status=0x%08X",
                           (void *)DstAddr, ByteCnt, (unsigned int)PspStatus);
         }
         break;
      default:
         CFE_EVS_SendEvent(MEMORY_READ_MEM_BLOCK_EID, CFE_EVS_EventType_ERROR,
                           "Memory read block invalid memory size %d; see EDS MemSize definition",
                           SrcMemSize);
         break;
   } /* End mem size switch */
   
   return RetStatus;
    
} /* End ReadMemBlock() */


/******************************************************************************
** Function: SendDumpBufToEvent
**
** Notes:
**   1. Build and send the event message containing the dump data
**   2. Refer to app_cfg.h's comments for a description of the event string
**      macros.
**
*/
static bool SendDumpBufToEvent(MEM_MGR_CpuAddr_Atom_t CpuAddr, const uint8 *DumpBuf, uint32 ByteCnt)
{

   bool   RetStatus = true;

   const char  EventHdrStr[] = MEMORY_DUMP_TOEVENT_HDR_STR;
   static char EventStr[CFE_MISSION_EVS_MAX_MESSAGE_LENGTH];
   char        TempStr[MEMORY_DUMP_TOEVENT_TEMP_CHARS];
   int32       EventStrTotalLen = 0;
   uint8*      EventStrBytePtr;
   uint32      i;
      
  
   strncpy(EventStr, EventHdrStr, sizeof(EventStr));
   EventStrTotalLen = MEM_MGR_strnlen(EventStr, CFE_MISSION_EVS_MAX_MESSAGE_LENGTH);

   EventStrBytePtr = (uint8*)Memory->DumpToEventBuf;
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
   ** No need to check snprintf return; CFE_SB_MessageStringGet() handles safe concatenation and
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
**   1. Callers assume this functions sends error events and this function
**      assumes the MEM_SIZE*_VerifyCpuAddr() functions send error events.
**   2. From an OO design perspective this is a virtual function dispatcher
**      for memory types.
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
         CFE_EVS_SendEvent(MEMORY_VERIFY_CPU_ADDR_EID, CFE_EVS_EventType_ERROR,
                           "Verify CPU address invalid memory size %u; see EDS MemSize definition",
                           MemSize);
         break;
   } /* End mem size switch */


   return RetStatus;

} /* End VerifyCpuAddr() */
 