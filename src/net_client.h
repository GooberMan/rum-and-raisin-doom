//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// Network client code
//

#ifndef NET_CLIENT_H
#define NET_CLIENT_H

#include "doomtype.h"
#include "d_ticcmd.h"
#include "sha1.h"
#include "net_defs.h"

DOOM_C_API doombool NET_CL_Connect(net_addr_t *addr, net_connect_data_t *data);
DOOM_C_API void NET_CL_Disconnect(void);
DOOM_C_API void NET_CL_Run(void);
DOOM_C_API void NET_CL_Init(void);
DOOM_C_API void NET_CL_LaunchGame(void);
DOOM_C_API void NET_CL_StartGame(net_gamesettings_t *settings);
DOOM_C_API void NET_CL_SendTiccmd(ticcmd_t *ticcmd, uint64_t maketic);
DOOM_C_API doombool NET_CL_GetSettings(net_gamesettings_t *_settings);
DOOM_C_API void NET_Init(void);

DOOM_C_API void NET_BindVariables(void);

DOOM_C_API extern doombool net_client_connected;
DOOM_C_API extern doombool net_client_received_wait_data;
DOOM_C_API extern net_waitdata_t net_client_wait_data;
DOOM_C_API extern char *net_client_reject_reason;
DOOM_C_API extern doombool net_waiting_for_launch;
DOOM_C_API extern char *net_player_name;

DOOM_C_API extern sha1_digest_t net_server_wad_sha1sum;
DOOM_C_API extern sha1_digest_t net_server_deh_sha1sum;
DOOM_C_API extern unsigned int net_server_is_freedoom;
DOOM_C_API extern sha1_digest_t net_local_wad_sha1sum;
DOOM_C_API extern sha1_digest_t net_local_deh_sha1sum;
DOOM_C_API extern unsigned int net_local_is_freedoom;

DOOM_C_API extern doombool drone;

#endif /* #ifndef NET_CLIENT_H */
