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
**    Define memory size 8 class
**
**  Notes:
**    1. From an OO design perspective this is a child class of MEMORY. No
**       state data is required so there isn't a class structure or a
**       constructior defined.
**    2. All functions operates on 8-bit data values and it is up 
**       to the caller to perform casting if needed.
**
*/

#ifndef _mem_size8_
#define _mem_size8_

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

#define MEM_SIZE8_OPT_INCL_EID      (MEM_SIZE8_BASE_EID + 0)
#define MEM_SIZE8_FILL_BLOCK_EID    (MEM_SIZE8_BASE_EID + 1)
#define MEM_SIZE8_PEEK_EID          (MEM_SIZE8_BASE_EID + 2)
#define MEM_SIZE8_POKE_EID          (MEM_SIZE8_BASE_EID + 3)
#define MEM_SIZE8_READ_BLOCK_EID    (MEM_SIZE8_BASE_EID + 4)
#define MEM_SIZE8_WRITE_BLOCK_EID   (MEM_SIZE8_BASE_EID + 5)
#define MEM_SIZE8_VER_CPU_ADDR_EID  (MEM_SIZE8_BASE_EID + 6)


/************************/
/** Exported Functions **/
/************************/


/******************************************************************************
** Function: MEM_SIZE8_FillBlock
**
*/
bool MEM_SIZE8_FillBlock(uint8 *MemAddr, uint8 FillData, uint32 ByteCnt);


/******************************************************************************
** Function: MEM_SIZE8_Peek
**
*/
bool MEM_SIZE8_Peek(uint8 *MemAddr, uint8 *Data);


/******************************************************************************
** Function: MEM_SIZE8_Poke
**
** Notes:
**   1. Assumes MemType has been verified so no need to report invalid value 
**
*/
bool MEM_SIZE8_Poke(uint8 *MemAddr, MEM_MGR_MemType_Enum_t MemType, const char *MemTypeStr, uint8 Data);


/******************************************************************************
** Function: MEM_SIZE8_ReadBlock
**
*/
bool MEM_SIZE8_ReadBlock(const uint8 *MemAddr, uint8 *DestAddr, uint32 ByteCnt);


/******************************************************************************
** Function: MEM_SIZE8_VerifyCpuAddr
**
*/
bool MEM_SIZE8_VerifyCpuAddr(uint8 *MemAddr, uint32 PspMemType, const char *MemTypeStr, uint32 ByteCnt);


/******************************************************************************
** Function: MEM_SIZE8_WriteBlock
**
*/
bool MEM_SIZE8_WriteBlock(uint8 *MemAddr, const uint8 *SrcAddr, uint32 ByteCnt);


#endif /* _mem_size8_ */
