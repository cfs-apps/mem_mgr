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
**    Implement Memory Manager's Dwell Table management functions
**
**  Notes:
**    1. The static "TblData" serves as a table load buffer. Table dump data is
**       read directly from table owner's table storage.
**    2. There are two design options for using the JSON parser that are illustrated
**       by KIT_TO's message table and APP_C_DEMO's histogram table. I chose APP_c_DEMO's
**       that uses the static JsonTblObjs[] array. This array needs to be updated if
**       the number of dwell tables or entries/dwell table sizes change.
**
*/

/*
** Include Files:
*/

#include <string.h>
#include "cfe_endian.h"
#include "mem_dwell_tbl.h"


/***********************/
/** Macro Definitions **/
/***********************/


/**********************/
/** Type Definitions **/
/**********************/


/************************************/
/** Local File Function Prototypes **/
/************************************/

static bool LoadJsonData(size_t JsonFileLen);
static bool ValidTblData(void);
static void WriteDwellTable(osal_id_t FileHandle, uint16 id);

/**********************/
/** Global File Data **/
/**********************/

static MEM_DWELL_TBL_Class_t  *MemDwellTbl = NULL;
static MEM_DWELL_TBL_Dwell_t  Dwell[MEM_MGR_DWELL_ID_CNT];   /* Working buffer for loads */


#define ID_SIZE        sizeof(uint16)
#define NAME_SIZE      sizeof(MEM_MGR_DwellName_String_t)
#define TOPIC_ID_SIZE  sizeof(uint16)
#define ENABLED_SIZE   JSON_MAX_KW_LEN
#define LENGTH_SIZE    sizeof(uint16)
#define DELAY_SIZE     sizeof(uint16)
#define SYM_NAME_SIZE  MEM_MGR_MAX_SYM_LEN
#define OFFSET_SIZE    sizeof(cpuaddr)

static CJSON_Obj_t JsonTblObjs[] = {

   /* Table Data Address                 Table Data Size, Updated, Data Type,  Float,  core-json query string, length of query string(exclude '\0') */

   { &Dwell[0].Id,                         ID_SIZE,        false,   JSONNumber, false,  { "dwell[0].id",                     (sizeof("dwell[0].id")-1)}},
   { &Dwell[0].Name,                       NAME_SIZE,      false,   JSONNumber, false,  { "dwell[0].name",                   (sizeof("dwell[0].name")-1)}},
   { &Dwell[0].TopicId,                    TOPIC_ID_SIZE,  false,   JSONNumber, false,  { "dwell[0].topic-id",               (sizeof("dwell[0].topic-id")-1)}},
   { &Dwell[0].EnaStr,                     ENABLED_SIZE,   false,   JSONString, false,  { "dwell[0].enabled",                (sizeof("dwell[0].enabled")-1)}},
   { &Dwell[0].Entry[0].Length,            LENGTH_SIZE,    false,   JSONNumber, false,  { "dwell[0].entry[0].length",        (sizeof("dwell[0].entry[0].length")-1)}},
   { &Dwell[0].Entry[0].Delay,             DELAY_SIZE,     false,   JSONNumber, false,  { "dwell[0].entry[0].delay",         (sizeof("dwell[0].entry[0].delay")-1)}},
   { &Dwell[0].Entry[0].SymbolAddr.Name,   SYM_NAME_SIZE,  false,   JSONString, false,  { "dwell[0].entry[0].address.symbol",(sizeof("dwell[0].entry[0].address.symbol")-1)}},
   { &Dwell[0].Entry[0].SymbolAddr.Offset, OFFSET_SIZE,    false,   JSONNumber, false,  { "dwell[0].entry[0].address.offset",(sizeof("dwell[0].entry[0].address.offset")-1)}},
   { &Dwell[0].Entry[1].Length,            LENGTH_SIZE,    false,   JSONNumber, false,  { "dwell[0].entry[1].length",        (sizeof("dwell[0].entry[1].length")-1)}},
   { &Dwell[0].Entry[1].Delay,             DELAY_SIZE,     false,   JSONNumber, false,  { "dwell[0].entry[1].delay",         (sizeof("dwell[0].entry[1].delay")-1)}},
   { &Dwell[0].Entry[1].SymbolAddr.Name,   SYM_NAME_SIZE,  false,   JSONString, false,  { "dwell[0].entry[1].address.symbol",(sizeof("dwell[0].entry[1].address.symbol")-1)}},
   { &Dwell[0].Entry[1].SymbolAddr.Offset, OFFSET_SIZE,    false,   JSONNumber, false,  { "dwell[0].entry[1].address.offset",(sizeof("dwell[0].entry[1].address.offset")-1)}},
   { &Dwell[0].Entry[2].Length,            LENGTH_SIZE,    false,   JSONNumber, false,  { "dwell[0].entry[2].length",        (sizeof("dwell[0].entry[2].length")-1)}},
   { &Dwell[0].Entry[2].Delay,             DELAY_SIZE,     false,   JSONNumber, false,  { "dwell[0].entry[2].delay",         (sizeof("dwell[0].entry[2].delay")-1)}},
   { &Dwell[0].Entry[2].SymbolAddr.Name,   SYM_NAME_SIZE,  false,   JSONString, false,  { "dwell[0].entry[2].address.symbol",(sizeof("dwell[0].entry[2].address.symbol")-1)}},
   { &Dwell[0].Entry[2].SymbolAddr.Offset, OFFSET_SIZE,    false,   JSONNumber, false,  { "dwell[0].entry[2].address.offset",(sizeof("dwell[0].entry[2].address.offset")-1)}},
   { &Dwell[0].Entry[3].Length,            LENGTH_SIZE,    false,   JSONNumber, false,  { "dwell[0].entry[3].length",        (sizeof("dwell[0].entry[3].length")-1)}},
   { &Dwell[0].Entry[3].Delay,             DELAY_SIZE,     false,   JSONNumber, false,  { "dwell[0].entry[3].delay",         (sizeof("dwell[0].entry[3].delay")-1)}},
   { &Dwell[0].Entry[3].SymbolAddr.Name,   SYM_NAME_SIZE,  false,   JSONString, false,  { "dwell[0].entry[3].address.symbol",(sizeof("dwell[0].entry[3].address.symbol")-1)}},
   { &Dwell[0].Entry[3].SymbolAddr.Offset, OFFSET_SIZE,    false,   JSONNumber, false,  { "dwell[0].entry[3].address.offset",(sizeof("dwell[0].entry[3].address.offset")-1)}},

   { &Dwell[1].Id,                         ID_SIZE,        false,   JSONNumber, false,  { "dwell[1].id",                     (sizeof("dwell[1].id")-1)}},
   { &Dwell[1].TopicId,                    TOPIC_ID_SIZE,  false,   JSONNumber, false,  { "dwell[1].topic-id",               (sizeof("dwell[1].topic-id")-1)}},
   { &Dwell[1].EnaStr,                     ENABLED_SIZE,   false,   JSONString, false,  { "dwell[1].enabled",                (sizeof("dwell[1].enabled")-1)}},
   { &Dwell[1].Entry[0].Length,            LENGTH_SIZE,    false,   JSONNumber, false,  { "dwell[1].entry[0].length",        (sizeof("dwell[1].entry[0].length")-1)}},
   { &Dwell[1].Entry[0].Delay,             DELAY_SIZE,     false,   JSONNumber, false,  { "dwell[1].entry[0].delay",         (sizeof("dwell[1].entry[0].delay")-1)}},
   { &Dwell[1].Entry[0].SymbolAddr.Name,   SYM_NAME_SIZE,  false,   JSONString, false,  { "dwell[1].entry[0].address.symbol",(sizeof("dwell[1].entry[0].address.symbol")-1)}},
   { &Dwell[1].Entry[0].SymbolAddr.Offset, OFFSET_SIZE,    false,   JSONNumber, false,  { "dwell[1].entry[0].address.offset",(sizeof("dwell[1].entry[0].address.offset")-1)}},
   { &Dwell[1].Entry[1].Length,            LENGTH_SIZE,    false,   JSONNumber, false,  { "dwell[1].entry[1].length",        (sizeof("dwell[1].entry[1].length")-1)}},
   { &Dwell[1].Entry[1].Delay,             DELAY_SIZE,     false,   JSONNumber, false,  { "dwell[1].entry[1].delay",         (sizeof("dwell[1].entry[1].delay")-1)}},
   { &Dwell[1].Entry[1].SymbolAddr.Name,   SYM_NAME_SIZE,  false,   JSONString, false,  { "dwell[1].entry[1].address.symbol",(sizeof("dwell[1].entry[1].address.symbol")-1)}},
   { &Dwell[1].Entry[1].SymbolAddr.Offset, OFFSET_SIZE,    false,   JSONNumber, false,  { "dwell[1].entry[1].address.offset",(sizeof("dwell[1].entry[1].address.offset")-1)}},
   { &Dwell[1].Entry[2].Length,            LENGTH_SIZE,    false,   JSONNumber, false,  { "dwell[1].entry[2].length",        (sizeof("dwell[1].entry[2].length")-1)}},
   { &Dwell[1].Entry[2].Delay,             DELAY_SIZE,     false,   JSONNumber, false,  { "dwell[1].entry[2].delay",         (sizeof("dwell[1].entry[2].delay")-1)}},
   { &Dwell[1].Entry[2].SymbolAddr.Name,   SYM_NAME_SIZE,  false,   JSONString, false,  { "dwell[1].entry[2].address.symbol",(sizeof("dwell[1].entry[2].address.symbol")-1)}},
   { &Dwell[1].Entry[2].SymbolAddr.Offset, OFFSET_SIZE,    false,   JSONNumber, false,  { "dwell[1].entry[2].address.offset",(sizeof("dwell[1].entry[2].address.offset")-1)}},
   { &Dwell[1].Entry[3].Length,            LENGTH_SIZE,    false,   JSONNumber, false,  { "dwell[1].entry[3].length",        (sizeof("dwell[1].entry[3].length")-1)}},
   { &Dwell[1].Entry[3].Delay,             DELAY_SIZE,     false,   JSONNumber, false,  { "dwell[1].entry[3].delay",         (sizeof("dwell[1].entry[3].delay")-1)}},
   { &Dwell[1].Entry[3].SymbolAddr.Name,   SYM_NAME_SIZE,  false,   JSONString, false,  { "dwell[1].entry[3].address.symbol",(sizeof("dwell[1].entry[3].address.symbol")-1)}},
   { &Dwell[1].Entry[3].SymbolAddr.Offset, OFFSET_SIZE,    false,   JSONNumber, false,  { "dwell[1].entry[3].address.offset",(sizeof("dwell[1].entry[3].address.offset")-1)}},

};


/******************************************************************************
** Function: MEM_DWELL_TBL_Constructor
**
** Notes:
**    1. This must be called prior to any other functions
**
*/
void MEM_DWELL_TBL_Constructor(MEM_DWELL_TBL_Class_t *ObjPtr)
{
   
   MemDwellTbl = ObjPtr;

   CFE_PSP_MemSet(MemDwellTbl, 0, sizeof(MEM_DWELL_TBL_Class_t));
   
   MemDwellTbl->JsonObjCnt = (sizeof(JsonTblObjs)/sizeof(CJSON_Obj_t));
   
} /* End MEM_DWELL_TBL_Constructor() */


/******************************************************************************
** Function: MEM_DWELL_TBL_DumpCmd
**
** Notes:
**  1. Function signature must match TBLMGR_DumpTblFuncPtr.
**  2. TBLMGR opens the JSON dump file and writes standard JSON header objects.
**     This only writes table-specific JSON objects excluding the final closing
**     bracket } for the table's main JSON object. 
**  3. File is formatted so it can be used as a load file. It does NOT contain
**     the topic-id map. 
*/

bool MEM_DWELL_TBL_DumpCmd(osal_id_t FileHandle)
{

   int32   i;
   char    DumpRecord[256];


   sprintf(DumpRecord,"   \"dwell\": [\n");
   OS_write(FileHandle, DumpRecord, strlen(DumpRecord));
   
   for (i=1; i <= MEM_MGR_DWELL_ID_CNT; i++)
   {
      if (i > 1)
      {
         sprintf(DumpRecord,",\n");
         OS_write(FileHandle, DumpRecord, strlen(DumpRecord));      
      }
      WriteDwellTable(FileHandle, i);
      sprintf(DumpRecord,"         ]\n      }");
      OS_write(FileHandle, DumpRecord, strlen(DumpRecord));
   }

   sprintf(DumpRecord,"\n   ]");
   OS_write(FileHandle, DumpRecord, strlen(DumpRecord));
   
   return true;
   
} /* End MEM_DWELL_TBL_DumpCmd() */


/******************************************************************************
** Function: MEM_DWELL_TBL_LoadCmd
**
** Notes:
**  1. Function signature must match TBLMGR_LoadTblFuncPtr.
**  2. Can assume valid table file name because this is a callback from 
**     the app framework table manager that has verified the file.
*/
bool MEM_DWELL_TBL_LoadCmd(APP_C_FW_TblLoadOptions_Enum_t LoadType, const char *Filename)
{

   bool  RetStatus = false;

   if (CJSON_ProcessFile(Filename, MemDwellTbl->JsonBuf, MEM_DWELL_TBL_JSON_FILE_MAX_CHAR, LoadJsonData))
   {
      MemDwellTbl->Loaded = true;
      RetStatus = true;   
   }

   return RetStatus;
   
} /* End MEM_DWELL_TBL_LoadCmd() */


/******************************************************************************
** Function: MEM_DWELL_TBL_ResetStatus
**
*/
void MEM_DWELL_TBL_ResetStatus(void)
{
   
   MemDwellTbl->LastLoadCnt = 0;
    
} /* End MEM_DWELL_TBL_ResetStatus() */


/******************************************************************************
** Function: LoadJsonData
**
** Notes:
**  1. See file prologue for full/partial table load scenarios
*/
static bool LoadJsonData(size_t JsonFileLen)
{

   bool    RetStatus = false;
   size_t  ObjLoadCnt;
   uint16  i;  


   MemDwellTbl->JsonFileLen = JsonFileLen;

   /* 
   ** 1. Copy table owner data into local table buffer
   ** 2. Process JSON file which updates local table buffer with JSON supplied values
   ** 3. If valid, copy local buffer over owner's data 
   */
   for (i= MEM_MGR_DwellId_Enum_t_MIN; i < MEM_MGR_DwellId_Enum_t_MAX; i++)
   {
      memcpy((void*)MEM_DWELL_TBL_PTR(Dwell,i), (void*)MEM_DWELL_TBL_PTR(MemDwellTbl->Dwell,i),
             sizeof(MEM_DWELL_TBL_Dwell_t));
   }
   
   ObjLoadCnt = CJSON_LoadObjArray(JsonTblObjs, MemDwellTbl->JsonObjCnt, MemDwellTbl->JsonBuf, MemDwellTbl->JsonFileLen);

   if (!MemDwellTbl->Loaded && (ObjLoadCnt != MemDwellTbl->JsonObjCnt))
   {

      CFE_EVS_SendEvent(MEM_DWELL_TBL_LOAD_EID, CFE_EVS_EventType_ERROR, 
                        "Table has never been loaded and new table only contains %d of %d data objects",
                        (unsigned int)ObjLoadCnt, (unsigned int)MemDwellTbl->JsonObjCnt);
   
   }
   else
   {

      if (ValidTblData())
      {
         for (i= MEM_MGR_DwellId_Enum_t_MIN; i < MEM_MGR_DwellId_Enum_t_MAX; i++)
         {
            memcpy((void*)MEM_DWELL_TBL_PTR(MemDwellTbl->Dwell,i),
                   (void*)MEM_DWELL_TBL_PTR(Dwell,i), sizeof(MEM_DWELL_TBL_Dwell_t));
         }
         MemDwellTbl->LastLoadCnt = ObjLoadCnt;
         CFE_EVS_SendEvent(MEM_DWELL_TBL_LOAD_EID, CFE_EVS_EventType_DEBUG, 
                           "Successfully loaded %d JSON objects",
                           (unsigned int)ObjLoadCnt);
         RetStatus = true;
      }
      
   } /* End if valid JSON obj count */
   
   return RetStatus;
   
} /* End LoadJsonData() */


/******************************************************************************
** Function: ValidTblData
**
** Notes:
**  1. Validates new table data before it is accepted.
**  2. Sends an event for first detected error.
*/
static bool ValidTblData(void)
{
   bool    RetStatus = true;
   uint16  id, i;
   MEM_DWELL_TBL_Dwell_t *DwellTbl;

 
   for (id=1; id <= MEM_MGR_DWELL_ID_CNT; id++)
   {
      DwellTbl = MEM_DWELL_TBL_PTR(Dwell,id);
      if (DwellTbl->Id != id)
      {
         CFE_EVS_SendEvent(MEM_DWELL_TBL_VALID_EID, CFE_EVS_EventType_ERROR, 
                           "Invalid table ID %d, it must match physical table ID %d",
                           DwellTbl->Id, id);
         RetStatus = false;
      }
      if (RetStatus)
      {
         DwellTbl->Enabled = (strcmp(DwellTbl->EnaStr, "true") == 0);
         if (!DwellTbl->Enabled)
         {
            if (strcmp(DwellTbl->EnaStr, "false") != 0)
            {
               CFE_EVS_SendEvent(MEM_DWELL_TBL_VALID_EID, CFE_EVS_EventType_ERROR, 
                                 "Invalid enable string %s. It must be either 'true' or 'false'",
                                 DwellTbl->EnaStr);
               RetStatus = false;
            }
         }
      } /* End if valid ID */
      if (RetStatus)
      {
         for (i=0; i < MEM_MGR_DWELL_ENTRIES; i++)
         {
            //TODO: Add entry validation
         }
      }      
   } /* End table ID loop */

   return RetStatus;
   
} /* End ValidTblData() */


/******************************************************************************
** Function: WriteDwellTable
**
*/
static void WriteDwellTable(osal_id_t FileHandle, uint16 id)
{
   uint16 i;
   char   DumpRecord[256];
   MEM_DWELL_TBL_Dwell_t *DwellTbl;

   DwellTbl = MEM_DWELL_TBL_PTR(MemDwellTbl->Dwell,id);
   
   sprintf(DumpRecord,"      {\n         \"id\": %d,\n         \"name\": \"%s\",\n         \"topic-id\": %d,\n         \"enabled\": \"%s\",\n         \"entry\": [\n",
           id, DwellTbl->Name, DwellTbl->TopicId, DwellTbl->EnaStr);
   OS_write(FileHandle, DumpRecord, strlen(DumpRecord));
         
   for (i=0; i < MEM_MGR_DWELL_ENTRIES; i++)
   {
      if (i > 0)
      {
         sprintf(DumpRecord,",\n");
         OS_write(FileHandle, DumpRecord, strlen(DumpRecord));      
      }
      sprintf(DumpRecord,"            { \"length\": %d, \"delay\": %d, \"address\": {\"symbol\": \"%s\", \"offset\": %ld}}",
              DwellTbl->Entry[i].Length, DwellTbl->Entry[i].Delay, DwellTbl->Entry[i].SymbolAddr.Name, DwellTbl->Entry[i].SymbolAddr.Offset);
      OS_write(FileHandle, DumpRecord, strlen(DumpRecord));
   }
   
} /* End WriteDwellTable() */
