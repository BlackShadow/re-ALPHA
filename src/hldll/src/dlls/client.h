/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
#ifndef CLIENT_H
#define CLIENT_H

//
// Client callbacks. The engine passes its globals, gpGlobals->self is the client.
//
extern "C"
{
void WINAPI SetChangeParms(globalvars_t *pGlobals);
void WINAPI SetNewParms(globalvars_t *pGlobals);
void WINAPI ClientKill(globalvars_t *pGlobals);
void WINAPI PutClientInServer(globalvars_t *pGlobals);
void WINAPI PlayerPreThink(globalvars_t *pGlobals);
void WINAPI PlayerPostThink(globalvars_t *pGlobals);
void WINAPI ClientConnect(globalvars_t *pGlobals);
void WINAPI ClientDisconnect(globalvars_t *pGlobals);
void WINAPI StartFrame(globalvars_t *pGlobals);
}

void respawn(entvars_t *pev);

#endif // CLIENT_H
