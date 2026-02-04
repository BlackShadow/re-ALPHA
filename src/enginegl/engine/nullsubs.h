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

#ifndef NULLSUBS_H
#define NULLSUBS_H

/*
 * Declarations for stub functions that exist as empty/null functions in the
 * original enginegl.exe (IDA nullsub_*).
 *
 * IMPORTANT: Do not add inline stub implementations here. Several real
 * functions exist elsewhere, and an inline stub would silently shadow them.
 */

void VID_HandlePause(void);
void VID_HandlePause2(void);
void VID_ForceLockState(void);
void VID_LockBuffer(void);
void VID_UnlockBuffer(void);
void VID_ForceLockState2(void);
void VID_Shutdown_Stub(void);

short NullSub_ReturnShort(short value);

void Sbar_IntermissionOverlay(void);
void Sbar_FinaleOverlay(void);

void D_BeginDirectRect(void);
void D_EndDirectRect(void);

void GL_ErrorString_Null(void);

void S_AmbientOff_Null(void);
void S_AmbientOn_Null(void);

void Cmd_StuffCmds_Null(void);

void MSG_BadRead_Null(void);
void MSG_ReadByte_Null(void);
void MSG_ReadShort_Null(void);

void COM_NullSub(void);
void COM_StripExtension_Null(void);

void Host_ClientCommands_Null(void);

void CL_KeyDown_Null(void);
void CL_KeyUp_Null(void);
void CL_Stub(void);

void IN_ClearStates(void);

#endif /* NULLSUBS_H */
