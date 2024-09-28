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
**    Implement the MEM_SIZE32_Class methods
**
**  Notes:
**    1. From an OO design perspective this is a child class of MEMORY. No
**       state data is required so there isn't a class structure or a
**       constructior defined.
**    2. All functions operates on 32-bit data values and it is up 
**       to the caller to perform casting if needed.
**
*/

/*
** Include Files:
*/

#include <string.h>
#include "mem_size32.h"

/***********************/
/** Macro Definitions **/
/***********************/

#define MEM_MGR_OPT_INCL_MSG  "MEM_SIZE32 was not included in the MEM_MGR app. See mem_mgr_platform_cfg.h for details"


/******************************************************************************
** Function: MEM_SIZE32_FillBlock
**
*/
bool MEM_SIZE32_FillBlock(uint32 *MemAddr, uint32 FillData, uint32 ByteCnt)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE32

   bool   RetStatus = true;
   int32  PspStatus;
   uint32 i;

   for (i = 0; i < ByteCnt; i++)
   {
      PspStatus = CFE_PSP_MemWrite32((MEM_MGR_CpuAddr_Atom_t)MemAddr, FillData);
      if (PspStatus == CFE_PSP_SUCCESS)
      {
         MemAddr++;
      }
      else
      {
         RetStatus = false;
         CFE_EVS_SendEvent(MEM_SIZE32_READ_BLOCK_EID, CFE_EVS_EventType_ERROR,
                           "32-bit memory fill block failed at destination address %p, byte count %d, status=0x%08X",
                           (void *)MemAddr, i, (unsigned int)PspStatus);
         break; 
      }
   } /* End loop */

   return RetStatus;
#else
   CFE_EVS_SendEvent(MEM_SIZE32_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE32_FillBlock() */


/******************************************************************************
** Function: MEM_SIZE32_Peek
**
*/
bool MEM_SIZE32_Peek(uint32 *MemAddr, uint32 *Data)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE32
   
   bool    RetStatus = false;
   int32   PspStatus;

   PspStatus = CFE_PSP_MemRead32((MEM_MGR_CpuAddr_Atom_t)MemAddr, Data);
   if (PspStatus == CFE_PSP_SUCCESS)
   {
      RetStatus = true;
   }
   else
   {
      *Data = 0; 
      CFE_EVS_SendEvent(MEM_SIZE32_PEEK_EID, CFE_EVS_EventType_ERROR,
                        "32-bit memory peek(read) failed for address %p, status=0x%08X",
                        (void *)MemAddr, (unsigned int)PspStatus);
   }

   return RetStatus;
#else
   CFE_EVS_SendEvent(MEM_SIZE32_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE32_Peek() */


/******************************************************************************
** Function: MEM_SIZE32_Poke
**
** Notes:
**   1. Assumes MemType has been verified so no need to report invalid value 
**
*/
bool MEM_SIZE32_Poke(uint32 *MemAddr, MEM_MGR_MemType_Enum_t MemType, const char *MemTypeStr, uint32 Data)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE32
   
   bool   RetStatus = false;
   int32  PspStatus;

   switch (MemType)
   {
      case MEM_MGR_MemType_NONVOL:
         PspStatus = CFE_PSP_EepromWrite32((MEM_MGR_CpuAddr_Atom_t)MemAddr, Data);
         break;
      case MEM_MGR_MemType_RAM:
         PspStatus = CFE_PSP_MemWrite32((MEM_MGR_CpuAddr_Atom_t)MemAddr, Data);
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
      CFE_EVS_SendEvent(MEM_SIZE32_POKE_EID, CFE_EVS_EventType_ERROR,
                        "32-bit %s memory poke(write) failed for address %p, status=0x%08X",
                        MemTypeStr, (void *)MemAddr, (unsigned int)PspStatus);
   }

   return RetStatus;
#else
   CFE_EVS_SendEvent(MEM_SIZE32_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE32_Poke() */


/******************************************************************************
** Function: MEM_SIZE32_ReadBlock
**
*/
bool MEM_SIZE32_ReadBlock(const uint32 *MemAddr, uint32* DestAddr, uint32 ByteCnt)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE32

   bool   RetStatus = true;
   int32  PspStatus;
   uint32 i;

   for (i = 0; i < ByteCnt; i++)
   {
      PspStatus = CFE_PSP_MemRead32((MEM_MGR_CpuAddr_Atom_t)MemAddr, DestAddr);
      if (PspStatus == CFE_PSP_SUCCESS)
      {
         MemAddr++;
         DestAddr++;
      }
      else
      {
         RetStatus = false;
         CFE_EVS_SendEvent(MEM_SIZE32_READ_BLOCK_EID, CFE_EVS_EventType_ERROR,
                           "32-bit memory block read failed at source address %p, destination address %p, byte count %d, status=0x%08X",
                           (void *)MemAddr, (void *)DestAddr, i, (unsigned int)PspStatus);
         break; 
      }
   } /* End loop */

   return RetStatus;
#else
   CFE_EVS_SendEvent(MEM_SIZE32_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE32_ReadBlock() */



/******************************************************************************
** Function: MEM_SIZE32_VerifyCpuAddr
**
*/
bool MEM_SIZE32_VerifyCpuAddr(uint32 *MemAddr, uint32 PspMemType, const char* MemTypeStr, uint32 ByteCnt)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE32

   bool  RetStatus = false;
   int32 PspStatus;

   if ((long unsigned int)MemAddr % sizeof(uint32) == 0)
   {
      PspStatus = CFE_PSP_MemValidateRange((MEM_MGR_CpuAddr_Atom_t)MemAddr, ByteCnt, PspMemType);
      if (PspStatus == CFE_PSP_SUCCESS)
      {
         RetStatus = true;
      }
      else
      {
         CFE_EVS_SendEvent(MEM_SIZE32_VER_CPU_ADDR_EID, CFE_EVS_EventType_ERROR,
                           "32-bit %s memory address %p failed PSP validation, status=0x%08X",
                           MemTypeStr, (void *)MemAddr, (unsigned int)PspStatus);
      }
      
   } /* End if valid alignment */
   else
   {
      CFE_EVS_SendEvent(MEM_SIZE32_VER_CPU_ADDR_EID, CFE_EVS_EventType_ERROR,
                        "32-bit %s memory address %p not 32 bit aligned",
                        MemTypeStr, (void *)MemAddr);
   }
   
   return RetStatus;
#else
   CFE_EVS_SendEvent(MEM_SIZE32_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE32_VerifyCpuAddr() */


/******************************************************************************
** Function: MEM_SIZE32_WriteBlock
**
*/
bool MEM_SIZE32_WriteBlock(uint32 *MemAddr, const uint32 *SrcAddr, uint32 ByteCnt)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE32

   bool   RetStatus = true;
   int32  PspStatus;
   uint32 i;

   for (i = 0; i < ByteCnt; i++)
   {
      PspStatus = CFE_PSP_MemWrite32((MEM_MGR_CpuAddr_Atom_t)MemAddr, *SrcAddr);
      if (PspStatus == CFE_PSP_SUCCESS)
      {
         SrcAddr++;
         MemAddr++;
      }
      else
      {
         RetStatus = false;
         CFE_EVS_SendEvent(MEM_SIZE32_READ_BLOCK_EID, CFE_EVS_EventType_ERROR,
                           "32-bit memory block write failed at source address %p, destination address %p, byte count %d, status=0x%08X",
                           (void *)SrcAddr, (void *)MemAddr, i, (unsigned int)PspStatus);
         break; 
      }
   } /* End loop */

   return RetStatus;
#else
   CFE_EVS_SendEvent(MEM_SIZE32_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif  
} /* End MEM_SIZE32_WriteBlock() */