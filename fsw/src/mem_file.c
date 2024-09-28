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
**    Implement the FILE_Class methods
**
**  Notes:
**    1. TODO: Describe OO design
**    2. MEM_FILE_DumpSymTblCmd() doesn't operate on memory but it is
**       included in this class so it runs in the context of the child
**       task that performs potentially long duration file operations.
**
*/

/*
** Include Files:
*/

#include <string.h>
#include "mem_file.h"
#include "mem_size8.h"
#include "mem_size16.h"
#include "mem_size32.h"

#define FILE_PRI_HDR_BYTES  sizeof(CFE_FS_Header_t)
#define FILE_SEC_HDR_BYTES  sizeof(MEM_MGR_SecFileHdr_t)
#define FILE_HDR_BYTES      (FILE_PRI_HDR_BYTES+FILE_SEC_HDR_BYTES)

/*******************************/
/** Local Function Prototypes **/
/*******************************/

static bool ComputeFileCrc(const char *Filename, osal_id_t FileHandle, APP_C_FW_CrcUint8_Enum_t CrcType, uint32 *Crc);
static bool CreateDumpFile(const char *Filename, osal_id_t FileHandle, const MEM_MGR_SecFileHdr_t *SecFileHdr, MEM_MGR_CpuAddr_Atom_t SrcCpuAddr);
static bool DumpMemToFile(MEM_MGR_CpuAddr_Atom_t SrcCpuAddr, osal_id_t FileHandle, const char *Filename, MEM_MGR_MemSize_Enum_t MemSize, uint32 ByteCnt);
static bool LoadMemFromFile(MEM_MGR_CpuAddr_Atom_t DestAddr, osal_id_t FileHandle, const char *Filename, MEM_MGR_MemSize_Enum_t MemSize, uint32 ByteCnt);
static bool ProcessLoadFile(const char *Filename, osal_id_t FileHandle, MEM_MGR_SecFileHdr_t *SecFileHdr, MEM_MGR_CpuAddr_Atom_t *CpuAddr);
static bool ValidLoadFile(const char *Filename, osal_id_t FileHandle, const MEM_MGR_SecFileHdr_t *SecFileHdr);


/**********************/
/** Global File Data **/
/**********************/

static MEM_FILE_Class_t *MemFile = NULL;


/******************************************************************************
** Function: MEM_FILE_Constructor
**
** Notes:
**   1. The memory instance pointer allows MEM_FILE to access MEMORY's command
**      status variables. Breaks some encapsulation rules but     
**
*/
void MEM_FILE_Constructor(MEM_FILE_Class_t *MemFilePtr, const INITBL_Class_t *IniTbl)
{
 
   MemFile = MemFilePtr;

   CFE_PSP_MemSet((void*)MemFile, 0, sizeof(MEM_FILE_Class_t));

   MemFile->IniTbl = IniTbl;

   MemFile->TaskBlockLimit = INITBL_GetIntConfig(IniTbl, CFG_MEM_FILE_TASK_BLOCK_LIMIT);
   MemFile->TaskBlockDelay = INITBL_GetIntConfig(IniTbl, CFG_MEM_FILE_TASK_BLOCK_DELAY);
   MemFile->TaskPerfId     = INITBL_GetIntConfig(IniTbl, CFG_MEM_FILE_CHILD_PERF_ID);

   MemFile->LoadBlockSize = INITBL_GetIntConfig(IniTbl, CFG_MEM_FILE_LOAD_BLOCK_SIZE);
   if (MemFile->LoadBlockSize > MEM_FILE_IO_BLOCK_SIZE)
   {
      CFE_EVS_SendEvent(MEM_FILE_CONSTRUCTOR_EID, CFE_EVS_EventType_ERROR,
                        "JSON init file error: MEM_FILE_LOAD_BLOCK_SIZE %d has been limited to app_cfg.h's MEM_FILE_IO_BLOCK_SIZE %d. See app_cfg.h for details.",
                        MemFile->LoadBlockSize, MEM_FILE_IO_BLOCK_SIZE);      
      MemFile->LoadBlockSize = MEM_FILE_IO_BLOCK_SIZE;
   }

   MemFile->DumpBlockSize = INITBL_GetIntConfig(IniTbl, CFG_MEM_FILE_DUMP_BLOCK_SIZE);
   if (MemFile->DumpBlockSize > MEM_FILE_IO_BLOCK_SIZE)
   {
      CFE_EVS_SendEvent(MEM_FILE_CONSTRUCTOR_EID, CFE_EVS_EventType_ERROR,
                        "JSON init file error: MEM_FILE_DUMP_BLOCK_SIZE %d has been limited to app_cfg.h's MEM_FILE_IO_BLOCK_SIZE %d. See app_cfg.h for details.",
                        MemFile->DumpBlockSize, MEM_FILE_IO_BLOCK_SIZE);      
      MemFile->DumpBlockSize = MEM_FILE_IO_BLOCK_SIZE;
   }

   MemFile->FillBlockSize = INITBL_GetIntConfig(IniTbl, CFG_MEM_FILE_FILL_BLOCK_SIZE);
   if (MemFile->FillBlockSize > MEM_FILE_IO_BLOCK_SIZE)
   {
      CFE_EVS_SendEvent(MEM_FILE_CONSTRUCTOR_EID, CFE_EVS_EventType_ERROR,
                        "JSON init file error: MEM_FILE_FILL_BLOCK_SIZE %d has been limited to app_cfg.h's MEM_FILE_IO_BLOCK_SIZE %d. See app_cfg.h for details.",
                        MemFile->FillBlockSize, MEM_FILE_IO_BLOCK_SIZE);      
      MemFile->FillBlockSize = MEM_FILE_IO_BLOCK_SIZE;
   }
   
} /* End MEM_FILE_Constructor */


/******************************************************************************
** Function: MEM_FILE_DumpCmd
**
** Notes:
**   1. Perform command message level processing, verify and open file, and
**      set telemetry response. File content processing is performed by helper
**      functions.    
**
*/
bool MEM_FILE_DumpCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{
   
   const MEM_MGR_DumpToFile_CmdPayload_t *DumpCmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_DumpToFile_t);
   
   bool       RetStatus = false;
   int32      OsStatus;   
   osal_id_t  FileHandle;
   MEM_MGR_SecFileHdr_t    SecFileHdr;
   MEMORY_VerifiedMemory_t VerifiedMemory;
   
   /* Errors reported by utility so no need for else clause */
   if (FileUtil_VerifyDirForWrite(DumpCmd->Filename))
   {
      if (MEMORY_VerifyAddr(DumpCmd->SymbolAddr, DumpCmd->MemType, DumpCmd->MemSize,
                            DumpCmd->ByteCnt, &VerifiedMemory))
      {
         OsStatus = OS_OpenCreate(&FileHandle, DumpCmd->Filename, OS_FILE_FLAG_NONE, OS_READ_WRITE);
         if (OsStatus == OS_SUCCESS)
         {         
            memset(&SecFileHdr, 0, sizeof(MEM_MGR_SecFileHdr_t));

            CFE_SB_MessageStringGet(SecFileHdr.SymbolAddr.Name, DumpCmd->SymbolAddr.Name, NULL, 
                                    sizeof(MEM_MGR_SymbolName_String_t), sizeof(MEM_MGR_SymbolName_String_t));
            SecFileHdr.SymbolAddr.Offset = DumpCmd->SymbolAddr.Offset;

            SecFileHdr.MemType = DumpCmd->MemType;
            SecFileHdr.MemSize = DumpCmd->MemSize;
            SecFileHdr.ByteCnt = DumpCmd->ByteCnt;
            SecFileHdr.CrcType = APP_C_FW_CrcUint8_CRC_16;

            CreateDumpFile(DumpCmd->Filename, FileHandle, &SecFileHdr, VerifiedMemory.CpuAddr);
            
            OsStatus = OS_close(FileHandle);
            if (OsStatus == OS_SUCCESS)
            {
               RetStatus = true;
            }
            else
            {
               CFE_EVS_SendEvent(MEM_FILE_DUMP_CMD_EID, CFE_EVS_EventType_ERROR,
                                 "Error closing memory dump to file %s after load completed, status = 0x%08X",
                                 DumpCmd->Filename, (unsigned int)OsStatus);
            }
         }
         else
         {
               CFE_EVS_SendEvent(MEM_FILE_DUMP_CMD_EID, CFE_EVS_EventType_ERROR,
                                 "Error opening memory dump file %s, status = 0x%08X",
                                 DumpCmd->Filename, (unsigned int)OsStatus);
         }
      }      
   } /* End if valid file */

   if (RetStatus == true)
   {
      MemFile->CmdStatus.Function  = MEM_MGR_MemFunction_DUMP_TO_FILE;
      MemFile->CmdStatus.Type      = SecFileHdr.MemType;
      MemFile->CmdStatus.Size      = SecFileHdr.MemSize;
      MemFile->CmdStatus.Addr      = VerifiedMemory.CpuAddr;
      MemFile->CmdStatus.Data      = 0;  // TODO: Capture last data byte?
      MemFile->CmdStatus.ByteCnt   = SecFileHdr.ByteCnt;
      
      MEMORY_SetCmdStatus(&MemFile->CmdStatus);
      strncpy(MemFile->Filename, DumpCmd->Filename, OS_MAX_PATH_LEN);
   }
    
   return RetStatus;

} /* End MEM_FILE_DumpCmd() */


/******************************************************************************
** Function: MEM_FILE_DumpSymTblCmd
**
** Notes:
**   1. Perform command message level processing, verify and open file, and
**      set telemetry response. File content processing is performed by helper
**      functions.    
**
*/
bool MEM_FILE_DumpSymTblCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{
   
   const MEM_MGR_DumpSymTblToFile_CmdPayload_t *DumpCmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_DumpSymTblToFile_t);
   
   bool   RetStatus = false;
   int32  OsStatus;
   char   Filename[OS_MAX_PATH_LEN];

   CFE_SB_MessageStringGet(Filename, DumpCmd->Filename, NULL, sizeof(Filename),sizeof(DumpCmd->Filename));
   
   if (MEM_MGR_strnlen(Filename, OS_MAX_PATH_LEN) > 0)
   {
      OsStatus = OS_SymbolTableDump(Filename, MEM_MGR_MAX_DUMP_FILE_DATA_SYMTBL);
      if (OsStatus == OS_SUCCESS)
      {
         strncpy(MemFile->Filename, DumpCmd->Filename, OS_MAX_PATH_LEN);
         //TODO: Is symbol table background? Shoudl this command be reported in memory filename?
         CFE_EVS_SendEvent(MEM_FILE_DUMP_SYM_TBL_CMD_EID, CFE_EVS_EventType_INFORMATION,
                           "Started Dump Symbol Table to File %s", Filename);
         RetStatus = true;
      }
      else
      {
         CFE_EVS_SendEvent(MEM_FILE_DUMP_SYM_TBL_CMD_EID, CFE_EVS_EventType_ERROR,
                           "Error dumping symbol table, OS_Status= 0x%X, File='%s'",
                           (unsigned int)OsStatus, Filename);
      }      
   }
   else
   {
      CFE_EVS_SendEvent(MEM_FILE_DUMP_SYM_TBL_CMD_EID, CFE_EVS_EventType_ERROR,
                        "Dump symbol table to file command rejected, filename string is empty");
                        
   } /* End string length */
   
   return RetStatus;

} /* End MEM_FILE_DumpSymTblCmd() */


/******************************************************************************
** Function: MEM_FILE_LoadCmd
**
** Notes:
**   1. Perform command message level processing, verify and open file, and
**      set telemetry response. File content processing is performed by helper
**      functions.    
**
*/
bool MEM_FILE_LoadCmd(void *DataObjPtr, const CFE_MSG_Message_t *MsgPtr)
{
   
   const MEM_MGR_LoadFromFile_CmdPayload_t *LoadCmd = CMDMGR_PAYLOAD_PTR(MsgPtr, MEM_MGR_LoadFromFile_t);
   
   bool       RetStatus = false;
   int32      OsStatus;   
   osal_id_t  FileHandle;
   MEM_MGR_SecFileHdr_t    SecFileHdr;
   MEM_MGR_CpuAddr_Atom_t  CpuAddr;
   
   /* Errors reported by utility so no need for else clause */
   if (FileUtil_VerifyFileForRead(LoadCmd->Filename))
   {
      OsStatus = OS_OpenCreate(&FileHandle, LoadCmd->Filename, OS_FILE_FLAG_NONE, OS_READ_ONLY);
      if (OsStatus == OS_SUCCESS)
      {

         ProcessLoadFile(LoadCmd->Filename, FileHandle, &SecFileHdr, &CpuAddr);
         
         OsStatus = OS_close(FileHandle);
         if (OsStatus == OS_SUCCESS)
         {
            RetStatus = true;
         }
         else
         {
            CFE_EVS_SendEvent(MEM_FILE_LOAD_CMD_EID, CFE_EVS_EventType_ERROR,
                              "Error closing memory load from file %s after load completed, status = 0x%08X",
                              LoadCmd->Filename, (unsigned int)OsStatus);
         }
      }
      else
      {
            CFE_EVS_SendEvent(MEM_FILE_LOAD_CMD_EID, CFE_EVS_EventType_ERROR,
                              "Error opening memory load file %s, status = 0x%08X",
                              LoadCmd->Filename, (unsigned int)OsStatus);
      }
      
   } /* End if valid file */

   if (RetStatus == true)
   {
      MemFile->CmdStatus.Function  = MEM_MGR_MemFunction_LOAD_FROM_FILE;
      MemFile->CmdStatus.Type      = SecFileHdr.MemType;
      MemFile->CmdStatus.Size      = SecFileHdr.MemSize;
      MemFile->CmdStatus.Addr      = CpuAddr;
      MemFile->CmdStatus.Data      = 0;  // TODO: Capture last data byte?
      MemFile->CmdStatus.ByteCnt   = SecFileHdr.ByteCnt;
      
      MEMORY_SetCmdStatus(&MemFile->CmdStatus);
      strncpy(MemFile->Filename, LoadCmd->Filename, OS_MAX_PATH_LEN);
   }
    
   return RetStatus;

} /* End MEM_FILE_LoadCmd() */


/******************************************************************************
** Function:  MEM_FILE_ResetStatus
**
*/
void MEM_FILE_ResetStatus(void)
{
   //TODO: Should anything be reset?
   
} /* End MEM_FILE_ResetStatus() */


/******************************************************************************
** Function: ComputeFileCrc
**
** Notes:
**   1. Assumes the file is positioned at the start of the load data. 
**   2. TaskBlockCount is the count of "task blocks" performed. A task block is 
**      is group of instructions that is CPU intensive and may need to be 
**      periodically suspended to prevent CPU hogging.
**
*/
static bool ComputeFileCrc(const char *Filename, osal_id_t FileHandle, APP_C_FW_CrcUint8_Enum_t CrcType, uint32 *Crc)
{
   
   bool    CrcComputed  = false;
   bool    ComputingCrc = true;
   uint32  CurrentCrc   = 0;
   int32   FileBytesRead;
   
   
   MemFile->TaskBlockCount = 0;
   if (CrcType == MEM_MGR_CRC)
   {

      *Crc = 0;   
      while (ComputingCrc)
      {
         
         FileBytesRead = OS_read(FileHandle, MemFile->IoBuf, MEM_FILE_IO_BLOCK_SIZE);

         if (FileBytesRead == 0) /* Successfully finished reading file */ 
         {  
            
            *Crc = CurrentCrc;
            ComputingCrc = false;
            CrcComputed  = true;
           
         }
         else if (FileBytesRead < 0) /* Error reading file */ 
         {  
            
            ComputingCrc = false;            
            CFE_EVS_SendEvent(MEM_FILE_COMPUTE_FILE_CRC_EID, CFE_EVS_EventType_ERROR,
                              "File read error %d while computing CRC for file %s",
                              FileBytesRead, Filename);
         }
         else
         {
                
            CurrentCrc = CFE_ES_CalculateCRC(MemFile->IoBuf, FileBytesRead,
                                             CurrentCrc, CrcType);
         
            CHILDMGR_PauseTask(&MemFile->TaskBlockCount, MemFile->TaskBlockLimit, MemFile->TaskBlockDelay, MemFile->TaskPerfId);
         
         } /* End if still reading file */

      } /* End while computing CRC */
   }
   else
   {
            CFE_EVS_SendEvent(MEM_FILE_COMPUTE_FILE_CRC_EID, CFE_EVS_EventType_ERROR,
                              "Invalid CRC type %d. See cFE ES for valid types.",
                              CrcType);      
   }
   
   return CrcComputed;
   
} /* End ComputeCrc() */


/******************************************************************************
** Function: CreateDumpFile
**
** Notes:
**   1. Perform all file verification before calling the function to perform
**      the memory dump
**
*/
static bool CreateDumpFile(const char *Filename, osal_id_t FileHandle,
                           const MEM_MGR_SecFileHdr_t *SecFileHdr, MEM_MGR_CpuAddr_Atom_t SrcCpuAddr)
{

   bool  RetStatus = false;
   int32 OsStatus; 
   CFE_FS_Header_t  CfeFileHeader;

   CFE_FS_InitHeader(&CfeFileHeader, INITBL_GetStrConfig(MemFile->IniTbl, CFG_MEM_FILE_CFE_HDR_DESCR),
                     INITBL_GetIntConfig(MemFile->IniTbl, CFG_MEM_FILE_CFE_HDR_SUBTYPE));
   
   OsStatus = CFE_FS_WriteHeader(FileHandle, &CfeFileHeader);
   if (OsStatus == FILE_PRI_HDR_BYTES)
   {
      OsStatus = OS_write(FileHandle, SecFileHdr, FILE_SEC_HDR_BYTES);
      if (OsStatus == FILE_SEC_HDR_BYTES)
      {
         RetStatus = DumpMemToFile(SrcCpuAddr, FileHandle, Filename, SecFileHdr->MemSize, SecFileHdr->ByteCnt);
      }           
      else
      {
         CFE_EVS_SendEvent(MEM_FILE_CREATE_DUMP_FILE_EID, CFE_EVS_EventType_ERROR,
                           "Error writing file %s MEM_MGR header. Status=0x%08X, Expected bytes=%u",
                           Filename, (unsigned int)OsStatus, (unsigned int)FILE_SEC_HDR_BYTES);

      } /* end OS_read if */

   } /* End valid cFE header read */
   else
   {
      CFE_EVS_SendEvent(MEM_FILE_CREATE_DUMP_FILE_EID, CFE_EVS_EventType_ERROR,
                        "Error writing file %s cFE header. Status=0x%08X, Expected bytes=%u",
                        Filename, (unsigned int)OsStatus, (unsigned int)FILE_PRI_HDR_BYTES);

   } /* End invalid cFE header read */
   
   return RetStatus;
   
}/* End CreateDumpFile() */


/******************************************************************************
** Function: DumpMemToFile
**
** Notes:
**   1. Assumes file position is at the start of the dump data.
**
*/
static bool DumpMemToFile(MEM_MGR_CpuAddr_Atom_t SrcCpuAddr, osal_id_t FileHandle,
                          const char *Filename, MEM_MGR_MemSize_Enum_t MemSize, uint32 ByteCnt)
{
   
   bool    RetStatus = false;
   int32   BytesRemaining = ByteCnt;
   size_t  FileWriteBlockSize = MemFile->DumpBlockSize;
   int32   FileWriteLength;
   int32   PspStatus;
   size_t  BytesProcessed = 0;

   MemFile->TaskBlockCount = 0;
   while (BytesRemaining != 0)
   {
      if (BytesRemaining < FileWriteBlockSize)
      {
         FileWriteBlockSize = BytesRemaining;
      }

      switch (MemSize)
      {
         case MEM_MGR_MemSize_8:
            RetStatus = MEM_SIZE8_ReadBlock((uint8*)SrcCpuAddr, (uint8*)MemFile->IoBuf, FileWriteBlockSize);
            break;
         case MEM_MGR_MemSize_16:
            RetStatus = MEM_SIZE16_ReadBlock((uint16*)SrcCpuAddr, (uint16*)MemFile->IoBuf, FileWriteBlockSize/2);
            break;
         case MEM_MGR_MemSize_32:
            RetStatus = MEM_SIZE32_ReadBlock((uint32*)SrcCpuAddr, (uint32*)MemFile->IoBuf, FileWriteBlockSize/4);
            break;
         case MEM_MGR_MemSize_VOID:
            PspStatus = CFE_PSP_MemCpy((void*)MemFile->IoBuf, (void*)SrcCpuAddr, FileWriteBlockSize);
            RetStatus = (PspStatus == CFE_PSP_SUCCESS);
            //TODO: Event
            break;
         default:
            //TODO: Event
            break;
      } /* End mem size switch */

      if (RetStatus == true)
      {
         if ((FileWriteLength = OS_write(FileHandle, MemFile->IoBuf, FileWriteBlockSize)) == FileWriteBlockSize)
         {
            BytesProcessed += FileWriteBlockSize;
            BytesRemaining -= FileWriteBlockSize;

            if (BytesRemaining != 0)
            {
               CHILDMGR_PauseTask(&MemFile->TaskBlockCount, MemFile->TaskBlockLimit, MemFile->TaskBlockDelay, MemFile->TaskPerfId);
            }
         } /* Valid memory write */            

      } /* End if read block */
      else
      {
         // Event sent by MEM_SIZEx_ReadBlock()
         RetStatus = false;
         BytesRemaining = 0;

      } /* End file read */
   } /* End while bytes */
   if (RetStatus == true)
   {
      RetStatus = (BytesProcessed == ByteCnt);
   }

   return RetStatus;
    
} /* End DumpMemToFile() */


/******************************************************************************
** Function: LoadMemFromFile
**
** Notes:
**   None
**
*/
static bool LoadMemFromFile(MEM_MGR_CpuAddr_Atom_t DestAddr, osal_id_t FileHandle, const char *Filename,
                            MEM_MGR_MemSize_Enum_t MemSize, uint32 ByteCnt)
{
   
   bool    RetStatus = false;
   int32   BytesRemaining = ByteCnt;
   size_t  FileReadBlockSize  = MemFile->LoadBlockSize;
   int32   FileReadLength;
   int32   OsStatus;
   int32   PspStatus;
   size_t  BytesProcessed = 0;

   MemFile->TaskBlockCount = 0;
   // Set file pointer to the start of the load data
   OsStatus = OS_lseek(FileHandle, FILE_HDR_BYTES, OS_SEEK_SET);
   if (OsStatus == FILE_HDR_BYTES)
   {
      while (BytesRemaining != 0)
      {
         if (BytesRemaining < FileReadBlockSize)
         {
            FileReadBlockSize = BytesRemaining;
         }

         if ((FileReadLength = OS_read(FileHandle, MemFile->IoBuf, FileReadBlockSize)) == FileReadBlockSize)
         {
            
            switch (MemSize)
            {
               case MEM_MGR_MemSize_8:
                  RetStatus = MEM_SIZE8_WriteBlock((uint8*)DestAddr, (uint8*)MemFile->IoBuf, FileReadBlockSize);
                  break;
               case MEM_MGR_MemSize_16:
                  RetStatus = MEM_SIZE16_WriteBlock((uint16*)DestAddr, (uint16*)MemFile->IoBuf, FileReadBlockSize/2);
                  break;
               case MEM_MGR_MemSize_32:
                  RetStatus = MEM_SIZE32_WriteBlock((uint32*)DestAddr, (uint32*)MemFile->IoBuf, FileReadBlockSize/4);
                  break;
               case MEM_MGR_MemSize_VOID:
                  PspStatus = CFE_PSP_MemCpy((void*)DestAddr, MemFile->IoBuf, FileReadBlockSize);
                  RetStatus = (PspStatus == CFE_PSP_SUCCESS);
                  //TODO: Event
                  break;
               default:
                  //TODO: Event
                  break;
            } /* End mem size switch */

            if (RetStatus == true)
            {
                BytesProcessed += FileReadBlockSize;
                BytesRemaining -= FileReadBlockSize;

                if (BytesRemaining != 0)
                {
                  CHILDMGR_PauseTask(&MemFile->TaskBlockCount, MemFile->TaskBlockLimit, MemFile->TaskBlockDelay, MemFile->TaskPerfId);
                }
            } /* Valid memory write */            
         } /* End file read */
         else
         {
            // Event sent by MEM_SIZEx_WriteBlock()            
            RetStatus = false;
            BytesRemaining = 0;

         } /* End file read */
      } /* End while bytes */
      if (RetStatus == true)
      {
         RetStatus = (BytesProcessed == ByteCnt);
      }
   } /* End valid lseek */
   else
   {
      CFE_EVS_SendEvent(MEM_FILE_LOAD_MEM_FROM_FILE_EID, CFE_EVS_EventType_ERROR,
                        "Error reading file %s MEM_MGR header. Status=0x%08X, Expected bytes=%u",
                        Filename, (unsigned int)OsStatus, (unsigned int)FILE_SEC_HDR_BYTES);               
   }

   return RetStatus;
    
} /* End LoadMemFromFile() */


/******************************************************************************
** Function: ProcessLoadFile
**
** Notes:
**   1. Perform all file verification before calling the function to perform
**      the memory load
**
*/
static bool ProcessLoadFile(const char *Filename, osal_id_t FileHandle,
                            MEM_MGR_SecFileHdr_t *SecFileHdr, MEM_MGR_CpuAddr_Atom_t *CpuAddr)
{

   bool  RetStatus = false;
   int32 OsStatus; 
   CFE_FS_Header_t         CfeFileHeader;
   MEMORY_VerifiedMemory_t VerifiedMemory;

         
   // Read and validate file headers
   OsStatus = CFE_FS_ReadHeader(&CfeFileHeader, FileHandle);
   if (OsStatus == FILE_PRI_HDR_BYTES)
   {
      OsStatus = OS_read(FileHandle, SecFileHdr, FILE_SEC_HDR_BYTES);
      if (OsStatus == FILE_SEC_HDR_BYTES)
      {
         if (ValidLoadFile(Filename, FileHandle, SecFileHdr))
         {
            if (MEMORY_VerifyAddr(SecFileHdr->SymbolAddr, SecFileHdr->MemType, SecFileHdr->MemSize,
                                  SecFileHdr->ByteCnt, &VerifiedMemory))
            {
               RetStatus = LoadMemFromFile(VerifiedMemory.CpuAddr, FileHandle, Filename,
                                           SecFileHdr->MemSize, SecFileHdr->ByteCnt);
            }
         }
      }           
      else
      {
         CFE_EVS_SendEvent(MEM_FILE_PROCESS_LOAD_FILE_EID, CFE_EVS_EventType_ERROR,
                           "Error reading file %s MEM_MGR header. Status=0x%08X, Expected bytes=%u",
                           Filename, (unsigned int)OsStatus, (unsigned int)FILE_SEC_HDR_BYTES);

      } /* end OS_read if */

   } /* End valid cFE header read */
   else
   {
      CFE_EVS_SendEvent(MEM_FILE_PROCESS_LOAD_FILE_EID, CFE_EVS_EventType_ERROR,
                        "Error reading file %s cFE header. Status=0x%08X, Expected bytes=%u",
                        Filename, (unsigned int)OsStatus, (unsigned int)FILE_PRI_HDR_BYTES);

   } /* End invalid cFE header read */
   
   return RetStatus;
   
}/* End ProcessLoadFile() */


/******************************************************************************
** Function: ValidLoadFile
**
** Notes:
**   1. Load file valid
**
*/
static bool ValidLoadFile(const char *Filename, osal_id_t FileHandle, const MEM_MGR_SecFileHdr_t *SecFileHdr)
{
   bool       RetStatus = false;
   int32      OsStatus;
   size_t     SizeFromHdr;
   int32      SizeFromOs;
   os_fstat_t FileStats;
   uint32     FileCrc;
   
   memset(&FileStats, 0, sizeof(FileStats));

   OsStatus = OS_stat(Filename, &FileStats);
   if (OsStatus == OS_SUCCESS)
   {
      SizeFromOs  = OS_FILESTAT_SIZE(FileStats);
      SizeFromHdr = SecFileHdr->ByteCnt + FILE_HDR_BYTES;
      if (SizeFromOs == SizeFromHdr)
      {
         if (ComputeFileCrc(Filename, FileHandle, SecFileHdr->CrcType, &FileCrc))
         {
            if (FileCrc == SecFileHdr->Crc)
            {
               RetStatus = true;
            }
            else
            {
               CFE_EVS_SendEvent(MEM_FILE_VALID_LOAD_FILE_EID, CFE_EVS_EventType_ERROR,
                                 "Load file CRC error: Computed=%d Expected=%u File: %s",
                                 (int)SizeFromOs,(unsigned int)SizeFromHdr, Filename);

            }
         }
      }
      else
      {
         CFE_EVS_SendEvent(MEM_FILE_VALID_LOAD_FILE_EID, CFE_EVS_EventType_ERROR,
                          "Load file size error: Reported by OS=%d Expected=%u File: %s",
                          (int)SizeFromOs,(unsigned int)SizeFromHdr, Filename);
      }
   }
   else
   {
      CFE_EVS_SendEvent(MEM_FILE_VALID_LOAD_FILE_EID, CFE_EVS_EventType_ERROR,
                        "Load file OS_stat error: Status=0x%08X File: %s",
                        (unsigned int)OsStatus, Filename);
   }

   return RetStatus;

} /* End ValidLoadFile() */
