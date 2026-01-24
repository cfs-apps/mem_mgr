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
**    Define configurations for the Memory Manager application
**
**  Notes:
**    1. Compile-time configurations that are applied to all platform
**       deployments of this app for a mission.
**
*/
#ifndef _mem_mgr_mission_cfg_
#define _mem_mgr_mission_cfg_

#include "cfe_mission_eds_designparameters.h"

/*
** Define CRC memory operations. The same CRC is used for all operations. See
** config.xml for cFE CRC options.
*/

#define MEM_MGR_CRC  CFE_MISSION_ES_CRC_16

#endif /* _mem_mgr_mission_cfg_ */
