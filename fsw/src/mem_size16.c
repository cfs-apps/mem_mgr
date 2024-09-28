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
**    Implement the MEM_SIZE16_Class methods
**
**  Notes:
**    1. From an OO design perspective this is a child class of MEMORY.
**    2. All functions operates on 16-bit data values and it is up 
**       to the caller to perform casting if needed.
**
*/

/*
** Include Files:
*/

#include <string.h>
#include "mem_size16.h"

/***********************/
/** Macro Definitions **/
/***********************/

#define MEM_MGR_OPT_INCL_MSG  "MEM_SIZE16 was not included in the MEM_MGR app. See mem_mgr_platform_cfg.h for details"


/******************************************************************************
** Function: MEM_SIZE16_FillBlock
**
*/
bool MEM_SIZE16_FillBlock(uint16 *MemAddr, uint16 FillData, uint32 ByteCnt)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE16
   
   bool   RetStatus = true;
   int32  PspStatus;
   uint32 i;

   for (i = 0; i < ByteCnt; i++)
   {
      PspStatus = CFE_PSP_MemWrite16((MEM_MGR_CpuAddr_Atom_t)MemAddr, FillData);
      if (PspStatus == CFE_PSP_SUCCESS)
      {
         MemAddr++;
      }
      else
      {
         RetStatus = false;
         CFE_EVS_SendEvent(MEM_SIZE16_READ_BLOCK_EID, CFE_EVS_EventType_ERROR,
                           "16-bit memory fill block failed at destination address %p, byte count %d, status=0x%08X",
                           (void *)MemAddr, i, (unsigned int)PspStatus);
         break; 
      }
   } /* End loop */

   return RetStatus;
#else
   CFE_EVS_SendEvent(MEM_SIZE16_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE16_FillBlock() */


/******************************************************************************
** Function: MEM_SIZE16_Peek
**
*/
bool MEM_SIZE16_Peek(uint16 *MemAddr, uint16 *Data)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE16
   
   bool    RetStatus = false;
   int32   PspStatus;

   PspStatus = CFE_PSP_MemRead16((MEM_MGR_CpuAddr_Atom_t)MemAddr, Data);
   if (PspStatus == CFE_PSP_SUCCESS)
   {
      RetStatus = true;
   }
   else
   {
      *Data = 0; 
      CFE_EVS_SendEvent(MEM_SIZE16_PEEK_EID, CFE_EVS_EventType_ERROR,
                        "16-bit memory peek(read) failed for address %p, status=0x%08X",
                        (void *)MemAddr, (unsigned int)PspStatus);
   }

   return RetStatus;
#else
   CFE_EVS_SendEvent(MEM_SIZE16_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE16_Peek() */


/******************************************************************************
** Function: MEM_SIZE16_Poke
**
** Notes:
**   1. Assumes MemType has been verified so no need to report invalid value 
**
*/
bool MEM_SIZE16_Poke(uint16 *MemAddr, MEM_MGR_MemType_Enum_t MemType, const char *MemTypeStr, uint16 Data)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE16
   
   bool   RetStatus = false;
   int32  PspStatus;

   switch (MemType)
   {
      case MEM_MGR_MemType_NONVOL:
         PspStatus = CFE_PSP_EepromWrite16((MEM_MGR_CpuAddr_Atom_t)MemAddr, Data);
         break;
      case MEM_MGR_MemType_RAM:
         PspStatus = CFE_PSP_MemWrite16((MEM_MGR_CpuAddr_Atom_t)MemAddr, Data);
         break;
      default:
         break;
   } /* End mem type switch */

   if (PspStatus == CFE_PSP_SUCCESS)
   {
      RetStatus = true;
   }
   else
   {
      CFE_EVS_SendEvent(MEM_SIZE16_POKE_EID, CFE_EVS_EventType_ERROR,
                        "16-bit %s memory poke(write) failed for address %p, status=0x%08X",
                        MemTypeStr, (void *)MemAddr, (unsigned int)PspStatus);
   }

   return RetStatus;
#else
   CFE_EVS_SendEvent(MEM_SIZE16_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE16_Poke() */


/******************************************************************************
** Function: MEM_SIZE16_ReadBlock
**
*/
bool MEM_SIZE16_ReadBlock(const uint16 *MemAddr, uint16 *DestAddr, uint32 ByteCnt)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE16
   
   bool   RetStatus = true;
   int32  PspStatus;
   uint32 i;

   for (i = 0; i < ByteCnt; i++)
   {
      PspStatus = CFE_PSP_MemRead16((MEM_MGR_CpuAddr_Atom_t)MemAddr, DestAddr);
      if (PspStatus == CFE_PSP_SUCCESS)
      {
         MemAddr++;
         DestAddr++;
      }
      else
      {
         RetStatus = false;
         CFE_EVS_SendEvent(MEM_SIZE16_READ_BLOCK_EID, CFE_EVS_EventType_ERROR,
                           "16-bit memory block read failed at source address %p, destination address %p, byte count %d, status=0x%08X",
                           (void *)MemAddr, (void *)DestAddr, i, (unsigned int)PspStatus);
         break; 
      }
   } /* End loop */

   return RetStatus;
#else
   CFE_EVS_SendEvent(MEM_SIZE16_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE16_ReadBlock() */


/******************************************************************************
** Function: MEM_SIZE16_VerifyCpuAddr
**
*/
bool MEM_SIZE16_VerifyCpuAddr(uint16 *MemAddr, uint32 PspMemType, const char *MemTypeStr, uint32 ByteCnt)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE16
   
   bool  RetStatus = false;
   int32 PspStatus;

   if ((long unsigned int)MemAddr % sizeof(uint16) == 0)
   {
      PspStatus = CFE_PSP_MemValidateRange((MEM_MGR_CpuAddr_Atom_t)MemAddr, ByteCnt, PspMemType);
      if (PspStatus == CFE_PSP_SUCCESS)
      {
         RetStatus = true;
      }
      else
      {
         CFE_EVS_SendEvent(MEM_SIZE16_VER_CPU_ADDR_EID, CFE_EVS_EventType_ERROR,
                           "16-bit %s memory address %p failed PSP validation, status=0x%08X",
                           MemTypeStr, (void *)MemAddr, (unsigned int)PspStatus);
      }
      
   } /* End if valid alignment */
   else
   {
      CFE_EVS_SendEvent(MEM_SIZE16_VER_CPU_ADDR_EID, CFE_EVS_EventType_ERROR,
                        "16-bit %s memory address %p not 16 bit aligned",
                        MemTypeStr, (void *)MemAddr);
   }
   
   return RetStatus;
#else
   CFE_EVS_SendEvent(MEM_SIZE16_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE16_VerifyCpuAddr() */


/******************************************************************************
** Function: MEM_SIZE16_WriteBlock
**
*/
bool MEM_SIZE16_WriteBlock(uint16 *MemAddr, const uint16 *SrcAddr, uint32 ByteCnt)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE16
   
   bool   RetStatus = true;
   int32  PspStatus;
   uint32 i;

   for (i = 0; i < ByteCnt; i++)
   {
      PspStatus = CFE_PSP_MemWrite16((MEM_MGR_CpuAddr_Atom_t)MemAddr, *SrcAddr);
      if (PspStatus == CFE_PSP_SUCCESS)
      {
         SrcAddr++;
         MemAddr++;
      }
      else
      {
         RetStatus = false;
         CFE_EVS_SendEvent(MEM_SIZE16_WRITE_BLOCK_EID, CFE_EVS_EventType_ERROR,
                           "16-bit memory write block failed at src addr %p, dest addr %p, byte count %d, status=0x%08X",
                           (void *)SrcAddr, (void *)MemAddr, i, (unsigned int)PspStatus);
         break; 
      }
   } /* End loop */

   return RetStatus;

#else
   CFE_EVS_SendEvent(MEM_SIZE16_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE16_WriteBlock() */