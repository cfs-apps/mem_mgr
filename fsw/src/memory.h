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
**    Define memory type class
**
**  Notes:
**    1. From an OO design perspective MEMORY serves as an abstract
**       virtual base class for the memory size classes.
**
*/

#ifndef _memory_
#define _memory_

/*
** Includes
*/

#include "app_cfg.h"

/***********************/
/** Macro Definitions **/
/***********************/


/*
** Event Message IDs
*/

#define MEMORY_DIS_EEPROM_WRITE_EID  (MEMORY_BASE_EID + 0)
#define MEMORY_DUMP_TO_EVENT_EID     (MEMORY_BASE_EID + 1)
#define MEMORY_ENA_EEPROM_WRITE_EID  (MEMORY_BASE_EID + 2)
#define MEMORY_FILL_CMD_EID          (MEMORY_BASE_EID + 3)
#define MEMORY_LOOKUP_SYMBOL_EID     (MEMORY_BASE_EID + 4)
#define MEMORY_LOAD_INT_DIS_EID      (MEMORY_BASE_EID + 5)
#define MEMORY_PEEK_CMD_EID          (MEMORY_BASE_EID + 6)
#define MEMORY_POKE_CMD_EID          (MEMORY_BASE_EID + 7)
#define MEMORY_CREATE_CPU_ADDR_EID   (MEMORY_BASE_EID + 8)
#define MEMORY_GET_PSP_MEM_TYPE_EID  (MEMORY_BASE_EID + 9)
#define MEMORY_VER_CPU_ADDR_EID      (MEMORY_BASE_EID + 10)


/**********************/
/** Type Definitions **/
/**********************/

// Return structure for the MEMORY_VerifyAddr()
typedef struct
{
   MEM_MGR_CpuAddr_Atom_t   CpuAddr;
   char                    *TypeStr;
      
} MEMORY_VerifiedMemory_t;


/******************************************************************************
** MEMORY_Class
*/


typedef struct
{
   MEM_MGR_MemFunction_Enum_t  Function;
   MEM_MGR_MemType_Enum_t      Type;
   MEM_MGR_MemSize_Enum_t      Size;
   MEM_MGR_CpuAddr_Atom_t      Addr;
   uint32                      Data;
   uint32                      ByteCnt; // TODO: Consider using 'units' of MemSize
      
} MEMORY_CmdStatus_t;

    
typedef struct
{
   bool EepromWriteEna;
   
   MEMORY_CmdStatus_t CmdStatus;
      
} MEMORY_Class_t;


/************************/
/** Exported Functions **/
/************************/


/******************************************************************************
** Function: MEMORY_Constructor
**
** Initialize the MEMORY object to a known state
**
** Notes:
**   1. This must be called prior to any other function.
**
*/
void MEMORY_Constructor(MEMORY_Class_t *MemoryPtr);


/******************************************************************************
** Function: MEMORY_DisEepromWriteCmd
**
** Notes:
**   None
**
*/
bool MEMORY_DisEepromWriteCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: MEMORY_DumpToEventCmd
**
*/
bool MEMORY_DumpToEventCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: MEMORY_EnaEepromWriteCmd
**
*/
bool MEMORY_EnaEepromWriteCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: MEMORY_FillCmd
**
*/
bool MEMORY_FillCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: MEMORY_LoadWithIntDisCmds
**
*/
bool MEMORY_LoadWithIntDisCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: MEMORY_LookupSymbolCmd
**
** Notes:
**   None
**
*/
bool MEMORY_LookupSymbolCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: MEMORY_PeekCmd
**
** Notes:
**   1. Peek is an old school term that means to read from a single memory
**      location. The number of bytes read depends on the 'width' (in bits)
**      of the memory acess. 
**
*/
bool MEMORY_PeekCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: MEMORY_PokeCmd
**
** Notes:
**   1. Poke is an old school term that means to write to a single memory
**      location. The number of bytes written depends on the 'width' (in bits)
**      of the memory acess. 
**
*/
bool MEMORY_PokeCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function:  MEMORY_ResetStatus
**
*/
void MEMORY_ResetStatus(void);


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
void MEMORY_SetCmdStatus(const MEMORY_CmdStatus_t *CmdStatus);


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
                       MEMORY_VerifiedMemory_t *VerifiedMemory);


#endif /* _memory_ */
