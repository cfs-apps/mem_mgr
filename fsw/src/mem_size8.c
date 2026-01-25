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
**    Implement the MEM_SIZE8_Class methods
**
**  Notes:
**    1. From an OO design perspective this is a child class of MEMORY.
**    2. All functions operates on 8-bit data values and it is up 
**       to the caller to perform casting if needed.
**
*/

/*
** Include Files:
*/

#include <string.h>
#include "mem_size8.h"

/***********************/
/** Macro Definitions **/
/***********************/

#define MEM_MGR_OPT_INCL_MSG  "MEM_SIZE8 was not included in the MEM_MGR app. See mem_mgr_platform_cfg.h for details"


/******************************************************************************
** Function: MEM_SIZE8_FillBlock
**
*/
bool MEM_SIZE8_FillBlock(uint8 *MemAddr, uint8 FillData, uint32 ByteCnt)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE8

   bool   RetStatus = true;
   int32  PspStatus;
   uint32 i;

   for (i = 0; i < ByteCnt; i++)
   {
      PspStatus = CFE_PSP_MemWrite8((MEM_MGR_CpuAddr_Atom_t)MemAddr, FillData);
      if (PspStatus == CFE_PSP_SUCCESS)
      {
         MemAddr++;
      }
      else
      {
         RetStatus = false;
         CFE_EVS_SendEvent(MEM_SIZE8_FILL_BLOCK_EID, CFE_EVS_EventType_ERROR,
                           "8-bit memory fill block failed at destination address %p, byte count %d, status=0x%08X",
                           (void *)MemAddr, i, (unsigned int)PspStatus);
         break; 
      }
   } /* End loop */

   return RetStatus;

#else
   CFE_EVS_SendEvent(MEM_SIZE8_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE8_FillBlock() */


/******************************************************************************
** Function: MEM_SIZE8_Read
**
*/
bool MEM_SIZE8_Read(uint8 *MemAddr, uint8 *Data)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE8

   bool   RetStatus = false;
   int32  PspStatus;

   PspStatus = CFE_PSP_MemRead8((MEM_MGR_CpuAddr_Atom_t)MemAddr, Data);
   if (PspStatus == CFE_PSP_SUCCESS)
   {
      RetStatus = true;
   }
   else
   {
      CFE_EVS_SendEvent(MEM_SIZE8_READ_EID, CFE_EVS_EventType_ERROR,
                        "8-bit memory read failed for address %p, status=0x%08X",
                        (void *)MemAddr, (unsigned int)PspStatus);
   }

   return RetStatus;

#else
   CFE_EVS_SendEvent(MEM_SIZE8_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE8_Read() */


/******************************************************************************
** Function: MEM_SIZE8_ReadBlock
**
*/
bool MEM_SIZE8_ReadBlock(uint8 *DstAddr, uint8 **MemAddr, uint32 ByteCnt)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE8

   bool   RetStatus = true;
   int32  PspStatus;
   uint32 i;
   uint8  *SrcAddr = *MemAddr;

   for (i = 0; i < ByteCnt; i++)
   {
      PspStatus = CFE_PSP_MemRead8((MEM_MGR_CpuAddr_Atom_t)SrcAddr, DstAddr);
      if (PspStatus == CFE_PSP_SUCCESS)
      {
         SrcAddr++;
         DstAddr++;
      }
      else
      {
         RetStatus = false;
         CFE_EVS_SendEvent(MEM_SIZE8_READ_BLOCK_EID, CFE_EVS_EventType_ERROR,
                           "8-bit memory block read failed at src addr %p, dest addr %p, byte count %d, status=0x%08X",
                           (void *)SrcAddr, (void *)DstAddr, i, (unsigned int)PspStatus);
         break; 
      }
   } /* End loop */

   *MemAddr = SrcAddr;
   
   return RetStatus;

#else
   CFE_EVS_SendEvent(MEM_SIZE8_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE8_ReadBlock() */


/******************************************************************************
** Function: MEM_SIZE8_VerifyCpuAddr
**
*/
bool MEM_SIZE8_VerifyCpuAddr(uint8 *MemAddr, uint32 PspMemType, const char *MemTypeStr, uint32 ByteCnt)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE8

   bool  RetStatus = true;
   int32 PspStatus;

   PspStatus = CFE_PSP_MemValidateRange((MEM_MGR_CpuAddr_Atom_t)MemAddr, ByteCnt, PspMemType);
   if (PspStatus != CFE_PSP_SUCCESS)
   {
      RetStatus = false;
      CFE_EVS_SendEvent(MEM_SIZE8_VER_CPU_ADDR_EID, CFE_EVS_EventType_ERROR,
                        "8-bit %s memory address %p failed PSP validation, status=0x%08X",
                        MemTypeStr, (void *)MemAddr, (unsigned int)PspStatus);
   }

   return RetStatus;

#else
   CFE_EVS_SendEvent(MEM_SIZE8_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE8_VerifyCpuAddr() */


/******************************************************************************
** Function: MEM_SIZE8_Write
**
** Notes:
**   1. Assumes MemType has been verified so no need to report invalid value 
**
*/
bool MEM_SIZE8_Write(uint8 *MemAddr, MEM_MGR_MemType_Enum_t MemType, const char *MemTypeStr, uint8 Data)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE8
 
   bool   RetStatus = false;

   int32  PspStatus;

   switch (MemType)
   {
      case MEM_MGR_MemType_NONVOL:
         PspStatus = CFE_PSP_EepromWrite8((MEM_MGR_CpuAddr_Atom_t)MemAddr, Data);
         break;
      case MEM_MGR_MemType_RAM:
         PspStatus = CFE_PSP_MemWrite8((MEM_MGR_CpuAddr_Atom_t)MemAddr, Data);
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
      CFE_EVS_SendEvent(MEM_SIZE8_WRITE_EID, CFE_EVS_EventType_ERROR,
                        "8-bit %s memory write failed for address %p, status=0x%08X",
                        MemTypeStr, (void *)MemAddr, (unsigned int)PspStatus);
   }
   
   return RetStatus;

#else
   CFE_EVS_SendEvent(MEM_SIZE8_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE8_Write() */


/******************************************************************************
** Function: MEM_SIZE8_WriteBlock
**
*/
bool MEM_SIZE8_WriteBlock(uint8 **MemAddr, const uint8 *SrcAddr, uint32 ByteCnt)
{
#if defined MEM_MGR_OPT_INCL_MEM_SIZE8
   
   bool   RetStatus = true;
   int32  PspStatus;
   uint32 i;
   uint8  *DstAddr = *MemAddr;
   
   for (i = 0; i < ByteCnt; i++)
   {
      PspStatus = CFE_PSP_MemWrite8((MEM_MGR_CpuAddr_Atom_t)DstAddr, *SrcAddr);
      if (PspStatus == CFE_PSP_SUCCESS)
      {
         SrcAddr++;
         DstAddr++;
      }
      else
      {
         RetStatus = false;
         CFE_EVS_SendEvent(MEM_SIZE8_WRITE_BLOCK_EID, CFE_EVS_EventType_ERROR,
                           "8-bit memory write block failed at src addr %p, dest addr %p, byte count %d, status=0x%08X",
                           (void *)SrcAddr, (void *)DstAddr, i, (unsigned int)PspStatus);
         break; 
      }
   } /* End loop */
   
   *MemAddr = DstAddr;
   
   return RetStatus;

#else
   CFE_EVS_SendEvent(MEM_SIZE8_OPT_INCL_EID, CFE_EVS_EventType_ERROR, MEM_MGR_OPT_INCL_MSG);
   return false;
#endif
} /* End MEM_SIZE8_WriteBlock() */
