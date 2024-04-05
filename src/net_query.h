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
// DESCRIPTION:
//     Querying servers to find their current status.
//

#ifndef NET_QUERY_H
#define NET_QUERY_H

#include "net_defs.h"

typedef void (*net_query_callback_t)(net_addr_t *addr,
                                     net_querydata_t *querydata,
                                     unsigned int ping_time,
                                     void *user_data);

DOOM_C_API int NET_StartLANQuery(void);
DOOM_C_API int NET_StartMasterQuery(void);

DOOM_C_API void NET_LANQuery(void);
DOOM_C_API void NET_MasterQuery(void);
DOOM_C_API void NET_QueryAddress(const char *addr);
DOOM_C_API net_addr_t *NET_FindLANServer(void);

DOOM_C_API int NET_Query_Poll(net_query_callback_t callback, void *user_data);

DOOM_C_API net_addr_t *NET_Query_ResolveMaster(net_context_t *context);
DOOM_C_API void NET_Query_AddToMaster(net_addr_t *master_addr);
DOOM_C_API doombool NET_Query_CheckAddedToMaster(doombool *result);
DOOM_C_API void NET_Query_AddResponse(net_packet_t *packet);
DOOM_C_API void NET_RequestHolePunch(net_context_t *context, net_addr_t *addr);

#endif /* #ifndef NET_QUERY_H */

