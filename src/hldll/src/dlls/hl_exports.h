#pragma once

#include "hl_types.h"
#include "../public/edict.h"
#include "../public/eiface.h"

extern "C" {
// The engine<->DLL game callbacks are __stdcall in the original hl.dll (the engine
// invokes them with the stdcall convention). Calling them as __cdecl corrupts the
// stack (RTC #0 / "ESP not saved across a function call"). ABI fidelity overrides
// the no-keyword style rule here. Everything below (Dispatch*/entity functions) is __cdecl.
DLLEXPORT void __stdcall GiveFnptrsToDll(enginefuncs_t *pEngineFuncs);
DLLEXPORT int __stdcall SetChangeParms(client_t *pClient);
DLLEXPORT int __stdcall SetNewParms(client_t *pClient);
DLLEXPORT int __stdcall ClientKill(client_t *pClient);
DLLEXPORT int __stdcall PutClientInServer(client_t *pClient);
DLLEXPORT int __stdcall PlayerPreThink(client_t *pClient);
DLLEXPORT int __stdcall PlayerPostThink(client_t *pClient);
DLLEXPORT void __stdcall ClientConnect(client_t *pClient);
DLLEXPORT int __stdcall ClientDisconnect(client_t *pClient);
DLLEXPORT int __stdcall StartFrame(globalvars_t *pGlobals);
// The alpha engine passes entvars_t* (NOT edict_t*) to every Dispatch* entry
// point, exactly like the classname factories below. The edict/object are
// recovered inside via pev->pContainingEntity + engine GET_PRIVATE.
DLLEXPORT int DispatchSpawn(entvars_t *pev);
DLLEXPORT const char* DispatchKeyValue(entvars_t *pev, KeyValueData *pkvd);
DLLEXPORT int DispatchRestore(entvars_t *pev, SAVERESTOREDATA *pSaveData);
DLLEXPORT int DispatchSave(entvars_t *pev, SAVERESTOREDATA *pSaveData);
DLLEXPORT void DispatchThink(entvars_t *pev, entvars_t *pevOther);
DLLEXPORT void DispatchTouch(entvars_t *pev, entvars_t *pevOther);
DLLEXPORT void DispatchUse(entvars_t *pev, entvars_t *pevOther);
DLLEXPORT void DispatchBlocked(entvars_t *pev, entvars_t *pevOther);
DLLEXPORT void ambient_generic(entvars_t *pev);
DLLEXPORT void cine_blood(entvars_t *pev);
DLLEXPORT void cycler_alien_grunt(entvars_t *pev);
DLLEXPORT void cycler_alien_slave(entvars_t *pev);
DLLEXPORT void cycler_bluedoc(entvars_t *pev);
DLLEXPORT void cycler_bullchicken(entvars_t *pev);
DLLEXPORT void cycler_desert(entvars_t *pev);
DLLEXPORT void cycler_doctor(entvars_t *pev);
DLLEXPORT void cycler_greendoc(entvars_t *pev);
DLLEXPORT void cycler_headcrab(entvars_t *pev);
DLLEXPORT void cycler_houndeye(entvars_t *pev);
DLLEXPORT void cycler_human_assault(entvars_t *pev);
DLLEXPORT void cycler_human_grunt(entvars_t *pev);
DLLEXPORT void cycler_olive(entvars_t *pev);
DLLEXPORT void cycler_panther(entvars_t *pev);
DLLEXPORT void cycler_prdroid(entvars_t *pev);
DLLEXPORT void cycler_reddoc(entvars_t *pev);
DLLEXPORT void cycler_scientist(entvars_t *pev);
DLLEXPORT void cycler_secure(entvars_t *pev);
DLLEXPORT void cycler_turret(entvars_t *pev);
DLLEXPORT void env_sound(entvars_t *pev);
DLLEXPORT void explode(entvars_t *pev);
DLLEXPORT void func_breakable(entvars_t *pev);
DLLEXPORT void func_button(entvars_t *pev);
DLLEXPORT void func_door(entvars_t *pev);
DLLEXPORT void func_door_rotating(entvars_t *pev);
DLLEXPORT void func_friction(entvars_t *pev);
DLLEXPORT void func_glass(entvars_t *pev);
DLLEXPORT void func_illusionary(entvars_t *pev);
DLLEXPORT void func_ladder(entvars_t *pev);
DLLEXPORT void func_pendulum(entvars_t *pev);
DLLEXPORT void func_plat(entvars_t *pev);
DLLEXPORT void func_pushable(entvars_t *pev);
DLLEXPORT void func_rot_button(entvars_t *pev);
DLLEXPORT void func_rotating(entvars_t *pev);
DLLEXPORT void func_train(entvars_t *pev);
DLLEXPORT void func_wall(entvars_t *pev);
DLLEXPORT void func_water(entvars_t *pev);
DLLEXPORT void hint_stairsdown(entvars_t *pev);
DLLEXPORT void hint_stairsup(entvars_t *pev);
DLLEXPORT void info_landmark(entvars_t *pev);
DLLEXPORT void info_null(entvars_t *pev);
DLLEXPORT void info_player_deathmatch(entvars_t *pev);
DLLEXPORT void info_player_start(entvars_t *pev);
DLLEXPORT void item(entvars_t *pev);
DLLEXPORT void john_train(entvars_t *pev);
DLLEXPORT void light(entvars_t *pev);
DLLEXPORT void light_spot(entvars_t *pev);
DLLEXPORT void momentary_door(entvars_t *pev);
DLLEXPORT void momentary_rot_button(entvars_t *pev);
DLLEXPORT void monster_alien_grunt(entvars_t *pev);
DLLEXPORT void monster_alien_slave(entvars_t *pev);
DLLEXPORT void monster_barney(entvars_t *pev);
DLLEXPORT void monster_boid(entvars_t *pev);
DLLEXPORT void monster_boid_flock(entvars_t *pev);
DLLEXPORT void monster_bullchicken(entvars_t *pev);
DLLEXPORT void monster_cine2_hvyweapons(entvars_t *pev);
DLLEXPORT void monster_cine2_scientist(entvars_t *pev);
DLLEXPORT void monster_cine2_slave(entvars_t *pev);
DLLEXPORT void monster_cine3_barney(entvars_t *pev);
DLLEXPORT void monster_cine3_scientist(entvars_t *pev);
DLLEXPORT void monster_cine_barney(entvars_t *pev);
DLLEXPORT void monster_cine_panther(entvars_t *pev);
DLLEXPORT void monster_cine_scientist(entvars_t *pev);
DLLEXPORT void monster_headcrab(entvars_t *pev);
DLLEXPORT void monster_houndeye(entvars_t *pev);
DLLEXPORT void monster_human_grunt(entvars_t *pev);
DLLEXPORT void monster_human_sarge(entvars_t *pev);
DLLEXPORT void monster_m44(entvars_t *pev);
DLLEXPORT void monster_panther(entvars_t *pev);
DLLEXPORT void monster_polyrobo(entvars_t *pev);
DLLEXPORT void monster_scientist(entvars_t *pev);
DLLEXPORT void monster_tentacle(entvars_t *pev);
DLLEXPORT void monster_tentacle_beak(entvars_t *pev);
DLLEXPORT void monster_turret(entvars_t *pev);
DLLEXPORT void multi_manager(entvars_t *pev);
DLLEXPORT void multisource(entvars_t *pev);
DLLEXPORT void path_corner(entvars_t *pev);
DLLEXPORT void train_info(entvars_t *pev);
DLLEXPORT void trigger(entvars_t *pev);
DLLEXPORT void trigger_cdaudio(entvars_t *pev);
DLLEXPORT void trigger_changelevel(entvars_t *pev);
DLLEXPORT void trigger_counter(entvars_t *pev);
DLLEXPORT void trigger_hurt(entvars_t *pev);
DLLEXPORT void trigger_monsterjump(entvars_t *pev);
DLLEXPORT void trigger_multiple(entvars_t *pev);
DLLEXPORT void trigger_once(entvars_t *pev);
DLLEXPORT void trigger_push(entvars_t *pev);
DLLEXPORT void worldspawn(entvars_t *pev);
DLLEXPORT BOOL __stdcall DllEntryPoint(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
}
