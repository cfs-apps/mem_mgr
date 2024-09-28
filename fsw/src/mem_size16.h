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
**    Define memory size 16 class
**
**  Notes:
**    1. From an OO design perspective this is a child class of MEMORY.
**    2. All functions operates on 16-bit data values and it is up 
**       to the caller to perform casting if needed.
**
*/

#ifndef _mem_size16_
#define _mem_size16_

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

#define MEM_SIZE16_OPT_INCL_EID      (MEM_SIZE16_BASE_EID + 0)
#define MEM_SIZE16_FILL_BLOCK_EID    (MEM_SIZE16_BASE_EID + 1)
#define MEM_SIZE16_PEEK_EID          (MEM_SIZE16_BASE_EID + 2)
#define MEM_SIZE16_POKE_EID          (MEM_SIZE16_BASE_EID + 3)
#define MEM_SIZE16_READ_BLOCK_EID    (MEM_SIZE16_BASE_EID + 4)
#define MEM_SIZE16_WRITE_BLOCK_EID   (MEM_SIZE16_BASE_EID + 5)
#define MEM_SIZE16_VER_CPU_ADDR_EID  (MEM_SIZE16_BASE_EID + 6)


/************************/
/** Exported Functions **/
/************************/


/******************************************************************************
** Function: MEM_SIZE16_FillBlock
**
*/
bool MEM_SIZE16_FillBlock(uint16 *MemAddr, uint16 FillData, uint32 ByteCnt);


/******************************************************************************
** Function: MEM_SIZE16_Peek
**
*/
bool MEM_SIZE16_Peek(uint16 *MemAddr, uint16 *Data);


/******************************************************************************
** Function: MEM_SIZE16_Poke
**
** Notes:
**   1. Assumes MemType has been verified so no need to report invalid value 
**
*/
bool MEM_SIZE16_Poke(uint16 *MemAddr, MEM_MGR_MemType_Enum_t MemType, const char *MemTypeStr, uint16 Data);


/******************************************************************************
** Function: MEM_SIZE16_ReadBlock
**
*/
bool MEM_SIZE16_ReadBlock(const uint16 *MemAddr, uint16 *DestAddr, uint32 ByteCnt);


/******************************************************************************
** Function: MEM_SIZE16_VerifyCpuAddr
**
*/
bool MEM_SIZE16_VerifyCpuAddr(uint16 *MemAddr, uint32 PspMemType, const char *MemTypeStr, uint32 ByteCnt);


/******************************************************************************
** Function: MEM_SIZE16_WriteBlock
**
*/
bool MEM_SIZE16_WriteBlock(uint16 *MemAddr, const uint16 *SrcAddr, uint32 ByteCnt);


#endif /* _mem_size16_ */
