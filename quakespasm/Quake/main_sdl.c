/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2005 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#if defined(USE_SDL2)
#include <SDL2/SDL.h>
#else
#include <SDL/SDL.h>
#endif
#else
#include "SDL.h"
#endif
#include <stdio.h>

#if defined(USE_SDL2)

/* need at least SDL_2.0.0 */
#define SDL_MIN_X	2
#define SDL_MIN_Y	0
#define SDL_MIN_Z	0
#define SDL_REQUIREDVERSION	(SDL_VERSIONNUM(SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z))
#define SDL_NEW_VERSION_REJECT	(SDL_VERSIONNUM(3,0,0))

#else

/* need at least SDL_1.2.10 */
#define SDL_MIN_X	1
#define SDL_MIN_Y	2
#define SDL_MIN_Z	10
#define SDL_REQUIREDVERSION	(SDL_VERSIONNUM(SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z))
/* reject 1.3.0 and newer at runtime. */
#define SDL_NEW_VERSION_REJECT	(SDL_VERSIONNUM(1,3,0))

#endif

extern uint64_t rumble_tick;

static void Sys_AtExit (void)
{
	SDL_Quit();
}

static void Sys_InitSDL (void)
{
#if defined(USE_SDL2)
	SDL_version v;
	SDL_version *sdl_version = &v;
	SDL_GetVersion(&v);
#else
	const SDL_version *sdl_version = SDL_Linked_Version();
#endif

	Sys_Printf("Found SDL version %i.%i.%i\n",sdl_version->major,sdl_version->minor,sdl_version->patch);
	if (SDL_VERSIONNUM(sdl_version->major,sdl_version->minor,sdl_version->patch) < SDL_REQUIREDVERSION)
	{	/*reject running under older SDL versions */
		Sys_Error("You need at least v%d.%d.%d of SDL to run this game.", SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z);
	}
#ifndef VITA
	if (SDL_VERSIONNUM(sdl_version->major,sdl_version->minor,sdl_version->patch) >= SDL_NEW_VERSION_REJECT)
	{	/*reject running under newer (1.3.x) SDL */
		Sys_Error("Your version of SDL library is incompatible with me.\n"
			  "You need a library version in the line of %d.%d.%d\n", SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z);
	}
#endif
	if (SDL_Init(0) < 0)
	{
		Sys_Error("Couldn't init SDL: %s", SDL_GetError());
	}
	atexit(Sys_AtExit);
}

#ifdef VITA
#include <vitasdk.h>
#define DEFAULT_MEMORY (128 * 1024 * 1024) // ericw -- was 72MB (64-bit) / 64MB (32-bit)
#else
#define DEFAULT_MEMORY (256 * 1024 * 1024) // ericw -- was 72MB (64-bit) / 64MB (32-bit)
#endif

static quakeparms_t	parms;

// On OS X we call SDL_main from the launcher, but SDL2 doesn't redefine main
// as SDL_main on OS X anymore, so we do it ourselves.
#if defined(USE_SDL2) && defined(__APPLE__)
#define main SDL_main
#endif

#ifdef VITA
uint16_t title[SCE_IME_DIALOG_MAX_TITLE_LENGTH];
uint16_t initial_text[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
uint16_t input_text[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
char title_keyboard[256];

void ascii2utf(uint16_t* dst, char* src){
	if(!src || !dst)return;
	while(*src)*(dst++)=(*src++);
	*dst=0x00;
}

void utf2ascii(char* dst, uint16_t* src){
	if(!src || !dst)return;
	while(*src)*(dst++)=(*(src++))&0xFF;
	*dst=0x00;
}

int quake_main (unsigned int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
#ifdef VITA
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	sceSysmoduleLoadModule(SCE_SYSMODULE_PSPNET_ADHOC);
	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);
	sceIoMkdir("ux0:data/Quakespasm/tmp", 0777);
	sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
	SceCommonDialogConfigParam cmnDlgCfgParam;
	sceCommonDialogConfigParamInit(&cmnDlgCfgParam);
	sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, (int *)&cmnDlgCfgParam.language);
	sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_ENTER_BUTTON, (int *)&cmnDlgCfgParam.enterButtonAssign);
	sceCommonDialogSetConfigParam(&cmnDlgCfgParam);
#endif
	int		t;
	double		time, oldtime, newtime;

	host_parms = &parms;
	parms.basedir = "ux0:data/Quakespasm";

#ifdef VITA	
	sceClibPrintf("Initializing vitaGL...\n");
	vglUseTripleBuffering(GL_FALSE);
	vglSetParamBufferSize(6 * 1024 * 1024);
	vglSetVertexPoolSize(48 * 1024 * 1024);
	vglInitWithCustomThreshold(20 * 1024 * 1024, 960, 544, 2 * 1024 * 1024, 0, 0, 0, SCE_GXM_MULTISAMPLE_4X);

	// Checking for libshacccg.suprx existence
	SceIoStat st1, st2;
	if (!(sceIoGetstat("ur0:/data/libshacccg.suprx", &st1) >= 0 || sceIoGetstat("ur0:/data/external/libshacccg.suprx", &st2) >= 0)) {
		SceMsgDialogUserMessageParam msg_param;
		sceClibMemset(&msg_param, 0, sizeof(SceMsgDialogUserMessageParam));
		msg_param.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
		msg_param.msg = (const SceChar8*)"Error: Runtime shader compiler (libshacccg.suprx) is not installed.";
		SceMsgDialogParam param;
		sceMsgDialogParamInit(&param);
		param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
		param.userMsgParam = &msg_param;
		sceMsgDialogInit(&param);
		while (sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) {
			vglSwapBuffers(GL_TRUE);
		}
		sceKernelExitProcess(0);
	}
	
	// Official mission packs support
	SceAppUtilAppEventParam eventParam;
	memset(&eventParam, 0, sizeof(SceAppUtilAppEventParam));
	sceAppUtilReceiveAppEvent(&eventParam);
	char *int_argv[16];
	int int_argc = 2;
	char buffer[2048];
	if (eventParam.type == 0x05) {
		int_argv[0] = int_argv[2] = "";
		memset(buffer, 0, 2048);
		sceAppUtilAppEventParseLiveArea(&eventParam, buffer);
		if (strstr(buffer, "custom") != NULL) {
			memset(input_text, 0, (SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1) << 1);
			memset(initial_text, 0, (SCE_IME_DIALOG_MAX_TEXT_LENGTH) << 1);
			sprintf(title_keyboard, "Insert args list");
			ascii2utf(title, title_keyboard);
			SceImeDialogParam param;
			sceImeDialogParamInit(&param);
			param.supportedLanguages = 0x0001FFFF;
			param.languagesForced = SCE_TRUE;
			param.type = SCE_IME_TYPE_BASIC_LATIN;
			param.title = title;
			param.maxTextLength = SCE_IME_DIALOG_MAX_TEXT_LENGTH;
			param.initialText = initial_text;
			param.inputTextBuffer = input_text;
			sceImeDialogInit(&param);
			while (sceImeDialogGetStatus() != 2) {
				vglSwapBuffers(GL_TRUE);
			}
			SceCommonDialogStatus status = sceImeDialogGetStatus();
			SceImeDialogResult result;
			memset(&result, 0, sizeof(SceImeDialogResult));
			sceImeDialogGetResult(&result);
			if (result.button == SCE_IME_DIALOG_BUTTON_ENTER)
			{
				utf2ascii(title_keyboard, input_text);
				if (strlen(title_keyboard) > 1) {
					
					int_argv[1] = title_keyboard;
					char *ptr = title_keyboard;
					for (;;) {
						char *space = strstr(ptr, " ");
						if (space == NULL) break;
						*space = 0;
						int_argv[int_argc++] = ptr = space + 1;
					}
					int_argv[int_argc++] = "";
					parms.argc = int_argc;
					parms.argv = int_argv;
				} else {
					parms.argc = argc;
					parms.argv = argv;
				}
			} else {
				parms.argc = argc;
				parms.argv = argv;
			}
			sceImeDialogTerm();
		} else {
			int_argv[1] = buffer;
			parms.argc = 3;
			parms.argv = int_argv;
		}
	} else {
		parms.argc = argc;
		parms.argv = argv;
	}
#endif	

	parms.errstate = 0;

	COM_InitArgv(parms.argc, parms.argv);
#ifndef VITA
	isDedicated = (COM_CheckParm("-dedicated") != 0);
#endif
	Sys_InitSDL ();

	Sys_Init();

	parms.memsize = DEFAULT_MEMORY;
	if (COM_CheckParm("-heapsize"))
	{	//in kb
		t = COM_CheckParm("-heapsize") + 1;
		if (t < com_argc)
			parms.memsize = Q_atoi(com_argv[t]) * 1024;
	}
	else if (COM_CheckParm("-mem"))
	{	//in mb, matching vanilla's arg on dos or linux.
		t = COM_CheckParm("-mem") + 1;
		if (t < com_argc)
			parms.memsize = Q_atoi(com_argv[t]) * 1024*1024;
	}

	parms.membase = malloc (parms.memsize);

	if (!parms.membase)
		Sys_Error ("Not enough memory free; check disk space\n");

	Sys_Printf("Quake %1.2f (c) id Software\n", VERSION);
	Sys_Printf("GLQuake %1.2f (c) id Software\n", GLQUAKE_VERSION);
	Sys_Printf("FitzQuake %1.2f (c) John Fitzgibbons\n", FITZQUAKE_VERSION);
	Sys_Printf("FitzQuake SDL port (c) SleepwalkR, Baker\n");
	Sys_Printf("QuakeSpasm " QUAKESPASM_VER_STRING " (c) Ozkan Sezer, Eric Wasylishen & others\n");
	Sys_Printf("QuakeSpasm-Spiked (c) Spike\n");

	Sys_Printf("Host_Init\n");
	Host_Init();

	oldtime = Sys_DoubleTime();
	if (isDedicated)
	{
		while (1)
		{
			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;

			while (time < sys_ticrate.value )
			{
				SDL_Delay(1);
				newtime = Sys_DoubleTime ();
				time = newtime - oldtime;
			}
			
			// Rumble effect managing (PSTV only)
			if (rumble_tick != 0) {
				if (sceKernelGetProcessTimeWide() - rumble_tick > 500000) IN_StopRumble(); // 0.5 sec
			}

			Host_Frame (time);
			oldtime = newtime;
		}
	}
	else
	while (1)
	{
		/* If we have no input focus at all, sleep a bit */
#ifndef VITA
		if (!VID_HasMouseOrInputFocus() || cl.paused)
		{
			SDL_Delay(16);
		}
		/* If we're minimised, sleep a bit more */
		if (VID_IsMinimized())
		{
			scr_skipupdate = 1;
			SDL_Delay(32);
		}
		else
#endif
		{
			scr_skipupdate = 0;
		}
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;

		Host_Frame (time);
#ifndef VITA
		if (time < sys_throttle.value && !cls.timedemo)
			SDL_Delay(1);
#endif
		oldtime = newtime;
	}

	return 0;
}

#ifdef VITA
int main(int argc, char **argv)
{
	// We need a bigger stack to run Quake, so we create a new thread with a proper stack size
	SceUID main_thread = sceKernelCreateThread("Quake", quake_main, 0x40, 0x800000, 0, 0, NULL);
	if (main_thread >= 0) {
		sceKernelStartThread(main_thread, 0, NULL);
	}
	return sceKernelExitDeleteThread(0);
}
#endif
