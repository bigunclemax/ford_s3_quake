/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * This is the clients main loop as well as some miscelangelous utility
 * and support functions
 *
 * =======================================================================
 */

#include "client/client.h"
#include "backends/input.h"

void CL_ForwardToServer_f();
void CL_Changing_f();
void CL_Reconnect_f();
void CL_Connect_f();
void CL_Rcon_f();
void CL_CheckForResend();

cvar_t *adr0;
cvar_t *adr1;
cvar_t *adr2;
cvar_t *adr3;
cvar_t *adr4;
cvar_t *adr5;
cvar_t *adr6;
cvar_t *adr7;
cvar_t *adr8;

cvar_t *rcon_client_password;
cvar_t *rcon_address;

cvar_t *cl_noskins;
cvar_t *cl_autoskins;
cvar_t *cl_footsteps;
cvar_t *cl_timeout;
cvar_t *cl_predict;
cvar_t *cl_maxfps;
cvar_t *cl_drawfps;
cvar_t *cl_gun;
cvar_t *cl_add_particles;
cvar_t *cl_add_lights;
cvar_t *cl_add_entities;
cvar_t *cl_add_blend;

cvar_t *cl_shownet;
cvar_t *cl_showmiss;
cvar_t *cl_showclamp;

cvar_t *cl_paused;
cvar_t *cl_timedemo;

cvar_t *cl_lightlevel;

/* userinfo */
cvar_t *info_password;
cvar_t *info_spectator;
cvar_t *name;
cvar_t *skin;
cvar_t *rate;
cvar_t *fov;
cvar_t *horplus;
cvar_t *msg;
cvar_t *hand;
cvar_t *gender;
cvar_t *gender_auto;

cvar_t *cl_vwep;

client_static_t cls;
client_state_t cl;

centity_t cl_entities[MAX_EDICTS];

entity_state_t cl_parse_entities[MAX_PARSE_ENTITIES];

extern cvar_t *allow_download;
extern cvar_t *allow_download_players;
extern cvar_t *allow_download_models;
extern cvar_t *allow_download_sounds;
extern cvar_t *allow_download_maps;

/*
 * Dumps the current net message, prefixed by the length
 */
void CL_WriteDemoMessage()
{
	int len, swlen;

	/* the first eight bytes are just packet sequencing stuff */
	len = net_message.cursize - 8;
	swlen = LittleLong(len);
	fwrite(&swlen, 4, 1, cls.demofile);
	fwrite(net_message.data + 8, len, 1, cls.demofile);
}

/*
 * stop recording a demo
 */
void CL_Stop_f()
{
	int len;

	if (!cls.demorecording)
	{
		Com_Printf("Not recording a demo.\n");
		return;
	}

	len = -1;

	fwrite(&len, 4, 1, cls.demofile);
	fclose(cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Com_Printf("Stopped demo.\n");
}

/*
 * record <demoname>
 * Begins recording a demo from the current position
 */
void CL_Record_f()
{
	char name[MAX_OSPATH];
	byte buf_data[MAX_MSGLEN];
	sizebuf_t buf;
	int i;
	int len;
	entity_state_t *ent;
	entity_state_t nullstate;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("record <demoname>\n");
		return;
	}

	if (cls.demorecording)
	{
		Com_Printf("Already recording.\n");
		return;
	}

	if (cls.state != ca_active)
	{
		Com_Printf("You must be in a level to record.\n");
		return;
	}

	Com_sprintf(name, sizeof(name), "%s/demos/%s.dm2", FS_WritableGamedir(), Cmd_Argv(1));

	Com_Printf("recording to %s.\n", name);
	FS_CreatePath(name);
	cls.demofile = fopen(name, "wb");

	if (!cls.demofile)
	{
		Com_Printf("ERROR: couldn't open.\n");
		return;
	}

	cls.demorecording = true;

	/* don't start saving messages until a non-delta compressed message is received */
	cls.demowaiting = true;

	/* write out messages to hold the startup information */
	SZ_Init(&buf, buf_data, sizeof(buf_data));

	/* send the serverdata */
	MSG_WriteByte(&buf, svc_serverdata);
	MSG_WriteLong(&buf, PROTOCOL_VERSION);
	MSG_WriteLong(&buf, 0x10000 + cl.servercount);
	MSG_WriteByte(&buf, 1); /* demos are always attract loops */
	MSG_WriteString(&buf, cl.gamedir);
	MSG_WriteShort(&buf, cl.playernum);

	MSG_WriteString(&buf, cl.configstrings[CS_NAME]);

	/* configstrings */
	for (i = 0; i < MAX_CONFIGSTRINGS; i++)
	{
		if (cl.configstrings[i][0])
		{
			if (buf.cursize + Q_strlen(cl.configstrings[i]) + 32 > buf.maxsize)
			{
				len = LittleLong(buf.cursize);
				fwrite(&len, 4, 1, cls.demofile);
				fwrite(buf.data, buf.cursize, 1, cls.demofile);
				buf.cursize = 0;
			}

			MSG_WriteByte(&buf, svc_configstring);

			MSG_WriteShort(&buf, i);
			MSG_WriteString(&buf, cl.configstrings[i]);
		}
	}

	/* baselines */
	memset(&nullstate, 0, sizeof(nullstate));

	for (i = 0; i < MAX_EDICTS; i++)
	{
		ent = &cl_entities[i].baseline;

		if (!ent->modelindex)
		{
			continue;
		}

		if (buf.cursize + 64 > buf.maxsize)
		{
			len = LittleLong(buf.cursize);
			fwrite(&len, 4, 1, cls.demofile);
			fwrite(buf.data, buf.cursize, 1, cls.demofile);
			buf.cursize = 0;
		}

		MSG_WriteByte(&buf, svc_spawnbaseline);

		MSG_WriteDeltaEntity(&nullstate, &cl_entities[i].baseline,
			&buf, true, true);
	}

	MSG_WriteByte(&buf, svc_stufftext);

	MSG_WriteString(&buf, "precache\n");

	/* write it to the demo file */
	len = LittleLong(buf.cursize);
	fwrite(&len, 4, 1, cls.demofile);
	fwrite(buf.data, buf.cursize, 1, cls.demofile);
}

static void CL_Setenv_f()
{
	#if 1
	Com_Printf("setenv not implemented on this platform\n");
	#else
	int argc = Cmd_Argc();

	if (argc > 2)
	{
		char buffer[1000];
		int i;

		strcpy(buffer, Cmd_Argv(1));
		strcat(buffer, "=");

		for (i = 2; i < argc; i++)
		{
			strcat(buffer, Cmd_Argv(i));
			strcat(buffer, " ");
		}

		putenv(buffer);
	}
	else
	if (argc == 2)
	{
		char *env = getenv(Cmd_Argv(1));

		if (env)
		{
			Com_Printf("%s=%s\n", Cmd_Argv(1), env);
		}
		else
		{
			Com_Printf("%s undefined\n", Cmd_Argv(1));
		}
	}
	#endif
}

void CL_Pause(bool pauseFlag)
{
    /* never pause in multiplayer */
	if ((Cvar_VariableValue("maxclients") > 1) || !Com_ServerState())
	{
        pauseFlag = false;
	}

	Cvar_SetValue("paused", pauseFlag);
}

static void CL_PauseOn()
{
    CL_Pause(true);
}

static void CL_PauseOff()
{
    CL_Pause(false);
}

static void CL_TogglePause_f()
{
    bool pauseFlag = !cl_paused->value;
    CL_Pause(pauseFlag);
}

void CL_Quit_f()
{
	CL_Disconnect();
	Com_Quit();
}

void CL_ClearState()
{
	S_StopAllSounds();
	CL_ClearEffects();
	CL_ClearTEnts();

	/* wipe the entire cl structure */
	memset(&cl, 0, sizeof(cl));
	memset(&cl_entities, 0, sizeof(cl_entities));

	SZ_Clear(&cls.netchan.message);
}

/*
 * Handle a reply from a ping
 */
void CL_ParseStatusMessage()
{
	char *s = MSG_ReadString(&net_message);
	Com_Printf("%s\n", s);
	M_AddToServerList(net_from, s);
}

/*
 * Load or download any custom player skins and models
 */
static void CL_Skins_f()
{
	int i;
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS + i][0])
		{
			continue;
		}

		Com_Printf("client %i: %s\n", i, cl.configstrings[CS_PLAYERSKINS + i]);

		SCR_UpdateScreen();

		Sys_SendKeyEvents(); /* pump message loop */

		CL_ParseClientinfo(i);
	}
}

/* This fixes some problems with wrong tagged models and skins */
void CL_FixUpGender()
{
	char *p;
	char sk[80];

	if (gender_auto->value)
	{
		if (gender->modified)
		{
			/* was set directly, don't override the user */
			gender->modified = false;
			return;
		}

		Q_strlcpy(sk, skin->string, sizeof(sk));

		if ((p = strchr(sk, '/')) != NULL)
		{
			*p = 0;
		}

		if ((Q_stricmp(sk, "male") == 0) || (Q_stricmp(sk, "cyborg") == 0))
		{
			Cvar_Set("gender", "male");
		}
		else
		if ((Q_stricmp(sk, "female") == 0) || (Q_stricmp(sk, "crackhor") == 0))
		{
			Cvar_Set("gender", "female");
		}
		else
		{
			Cvar_Set("gender", "none");
		}

		gender->modified = false;
	}
}

static void CL_Userinfo_f()
{
	Com_Printf("User info settings:\n");
	Info_Print(Cvar_Userinfo());
}

/*
 * Restart the sound subsystem so it can pick up
 * new parameters and flush all sounds
 */
void CL_Snd_Restart_f()
{
	S_Shutdown();
	S_Init();
	CL_RegisterSounds();
}

int precache_check;
int precache_spawncount;
int precache_tex;
int precache_model_skin;

byte *precache_model;

/*
 * The server will send this command right
 * before allowing the client into the server
 */
static void CL_Precache_f()
{
	/* Yet another hack to let old demos work */
	if (Cmd_Argc() < 2)
	{
		unsigned map_checksum; /* for detecting cheater maps */

		CM_LoadMap(cl.configstrings[CS_MODELS + 1], true, &map_checksum);
		CL_RegisterSounds();
		CL_PrepRefresh();
		return;
	}

	precache_check = CS_MODELS;

	precache_spawncount = (int)strtol(Cmd_Argv(1), (char **)NULL, 10);
	precache_model = 0;
	precache_model_skin = 0;

	CL_RequestNextDownload();
}

static void CL_InitLocal()
{
	cls.state = ca_disconnected;
	cls.realtime = Sys_Milliseconds();

	CL_InitInput();

	adr0 = Cvar_Get("adr0", "", CVAR_ARCHIVE);
	adr1 = Cvar_Get("adr1", "", CVAR_ARCHIVE);
	adr2 = Cvar_Get("adr2", "", CVAR_ARCHIVE);
	adr3 = Cvar_Get("adr3", "", CVAR_ARCHIVE);
	adr4 = Cvar_Get("adr4", "", CVAR_ARCHIVE);
	adr5 = Cvar_Get("adr5", "", CVAR_ARCHIVE);
	adr6 = Cvar_Get("adr6", "", CVAR_ARCHIVE);
	adr7 = Cvar_Get("adr7", "", CVAR_ARCHIVE);
	adr8 = Cvar_Get("adr8", "", CVAR_ARCHIVE);

	cin_force43 = Cvar_Get("cin_force43", "1", 0);

	/* register our variables */
	cl_add_blend = Cvar_Get("cl_blend", "1", 0);
	cl_add_lights = Cvar_Get("cl_lights", "1", 0);
	cl_add_particles = Cvar_Get("cl_particles", "1", 0);
	cl_add_entities = Cvar_Get("cl_entities", "1", 0);
	cl_gun = Cvar_Get("cl_gun", "2", CVAR_ARCHIVE);
	cl_footsteps = Cvar_Get("cl_footsteps", "1", 0);
	cl_noskins = Cvar_Get("cl_noskins", "0", 0);
	cl_autoskins = Cvar_Get("cl_autoskins", "0", 0);
	cl_predict = Cvar_Get("cl_predict", "1", 0);
	cl_maxfps = Cvar_Get("cl_maxfps", "95", CVAR_ARCHIVE);
	cl_drawfps = Cvar_Get("cl_drawfps", "0", CVAR_ARCHIVE);

	cl_speed_up = Cvar_Get("cl_speed_up", "200", 0);
	cl_speed_forward = Cvar_Get("cl_speed_forward", "200", 0);
	cl_speed_side = Cvar_Get("cl_speed_side", "200", 0);
	cl_speed_yaw = Cvar_Get("cl_speed_yaw", "140", 0);
	cl_speed_pitch = Cvar_Get("cl_speed_pitch", "150", 0);
	cl_anglespeedkey = Cvar_Get("cl_anglespeedkey", "1.5", 0);
	cl_run = Cvar_Get("cl_run", "0", CVAR_ARCHIVE);

	cl_shownet = Cvar_Get("cl_shownet", "0", 0);
	cl_showmiss = Cvar_Get("cl_showmiss", "0", 0);
	cl_showclamp = Cvar_Get("showclamp", "0", 0);
	cl_timeout = Cvar_Get("cl_timeout", "120", 0);
	cl_paused = Cvar_Get("paused", "0", 0);
	cl_timedemo = Cvar_Get("timedemo", "0", 0);

	rcon_client_password = Cvar_Get("rcon_password", "", 0);
	rcon_address = Cvar_Get("rcon_address", "", 0);

	cl_lightlevel = Cvar_Get("gl_lightlevel", "0", 0);

	/* userinfo */
	info_password = Cvar_Get("password", "", CVAR_USERINFO);
	info_spectator = Cvar_Get("spectator", "0", CVAR_USERINFO);
	name = Cvar_Get("name", "unnamed", CVAR_USERINFO | CVAR_ARCHIVE);
	skin = Cvar_Get("skin", "male/grunt", CVAR_USERINFO | CVAR_ARCHIVE);
	rate = Cvar_Get("rate", "32768", CVAR_USERINFO | CVAR_ARCHIVE);
	msg = Cvar_Get("msg", "1", CVAR_USERINFO | CVAR_ARCHIVE);
	hand = Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	fov = Cvar_Get("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);
	horplus = Cvar_Get("horplus", "1", CVAR_ARCHIVE);
	gender = Cvar_Get("gender", "male", CVAR_USERINFO | CVAR_ARCHIVE);
	gender_auto = Cvar_Get("gender_auto", "1", CVAR_ARCHIVE);
	gender->modified = false;

	cl_vwep = Cvar_Get("cl_vwep", "1", CVAR_ARCHIVE);

	/* register our commands */
	Cmd_AddCommand("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand("togglepause", CL_TogglePause_f);
	Cmd_AddCommand("+pause", CL_PauseOn);
	Cmd_AddCommand("-pause", CL_PauseOff);
	Cmd_AddCommand("pingservers", CL_PingServers_f);
	Cmd_AddCommand("skins", CL_Skins_f);

	Cmd_AddCommand("userinfo", CL_Userinfo_f);
	Cmd_AddCommand("snd_restart", CL_Snd_Restart_f);

	Cmd_AddCommand("changing", CL_Changing_f);
	Cmd_AddCommand("disconnect", CL_Disconnect_f);
	Cmd_AddCommand("record", CL_Record_f);
	Cmd_AddCommand("stop", CL_Stop_f);

	Cmd_AddCommand("quit", CL_Quit_f);

	Cmd_AddCommand("connect", CL_Connect_f);
	Cmd_AddCommand("reconnect", CL_Reconnect_f);

	Cmd_AddCommand("rcon", CL_Rcon_f);

	Cmd_AddCommand("setenv", CL_Setenv_f);

	Cmd_AddCommand("precache", CL_Precache_f);

	Cmd_AddCommand("download", CL_Download_f);

	/* forward to server commands
	 * the only thing this does is allow command completion
	 * to work -- all unknown commands are automatically
	 * forwarded to the server */
	Cmd_AddCommand("wave", NULL);
	Cmd_AddCommand("inven", NULL);
	Cmd_AddCommand("kill", NULL);
	Cmd_AddCommand("use", NULL);
	Cmd_AddCommand("drop", NULL);
	Cmd_AddCommand("say", NULL);
	Cmd_AddCommand("say_team", NULL);
	Cmd_AddCommand("info", NULL);
	Cmd_AddCommand("prog", NULL);
	Cmd_AddCommand("give", NULL);
	Cmd_AddCommand("god", NULL);
	Cmd_AddCommand("notarget", NULL);
	Cmd_AddCommand("noclip", NULL);
	Cmd_AddCommand("invuse", NULL);
	Cmd_AddCommand("invprev", NULL);
	Cmd_AddCommand("invnext", NULL);
	Cmd_AddCommand("invdrop", NULL);
	Cmd_AddCommand("weapnext", NULL);
	Cmd_AddCommand("weapprev", NULL);
}

/*
 * Writes key bindings and archived cvars to user.cfg
 */
void CL_WriteConfiguration()
{
	FILE *f;
	char path[MAX_OSPATH];

	if (cls.state == ca_uninitialized)
	{
		return;
	}

	Com_sprintf(path, sizeof(path), "%s/user.cfg", FS_WritableGamedir());

	f = fopen(path, "w");
	if (!f)
	{
		Com_Printf("Couldn't write user.cfg.\n");
		return;
	}

	fprintf(f, "// generated by quake, do not modify\n");

	Key_WriteBindings(f);
	fclose(f);

	Cvar_WriteVariables(path);
}

typedef struct
{
	char *name;
	char *value;
	cvar_t *var;
} cheatvar_t;

cheatvar_t cheatvars[] =
{
	{ "timescale", "1", NULL },
	{ "timedemo", "0", NULL },
	{ "gl_drawworld", "1", NULL },
	{ "cl_testlights", "0", NULL },
	{ "r_lightmap_disabled", "0", NULL },
	{ "paused", "0", NULL },
	{ "fixedtime", "0", NULL },
	{ "r_lightmap_only", "0", NULL },
	{ "r_lightmap_saturate", "0", NULL },
	{ NULL, NULL, NULL }
};

int numcheatvars;

void CL_FixCvarCheats()
{
	int i;
	cheatvar_t *var;

	if (!strcmp(cl.configstrings[CS_MAXCLIENTS], "1") || !cl.configstrings[CS_MAXCLIENTS][0])
		return; /* single player can cheat  */

	/* find all the cvars if we haven't done it yet */
	if (!numcheatvars)
	{
		while (cheatvars[numcheatvars].name)
		{
			cheatvars[numcheatvars].var = Cvar_Get(cheatvars[numcheatvars].name, cheatvars[numcheatvars].value, 0);
			numcheatvars++;
		}
	}

	/* make sure they are all set to the proper values */
	for (i = 0, var = cheatvars; i < numcheatvars; i++, var++)
	{
		if (strcmp(var->var->string, var->value))
		{
			Cvar_Set(var->name, var->value);
		}
	}
}

static void CL_UpdateWindowedMouse()
{
	if (cls.disable_screen)
		return;

	if (cls.key_dest == key_menu || cls.key_dest == key_console ||
	    (cls.key_dest == key_game && (cls.state != ca_active || !cl.refresh_prepped)))
	{
		if (mouse_windowed->value)
			Cvar_SetValue("mouse_windowed", 0);
	}
	else
	{
		if (!mouse_windowed->value)
			Cvar_SetValue("mouse_windowed", 1);
	}
}

void CL_SendCommand()
{
	/* update mouse_windowed cvar */
	CL_UpdateWindowedMouse();

	/* get new key events */
	Sys_SendKeyEvents();

	/* process console commands */
	Cbuf_Execute();

	/* fix any cheating cvars */
	CL_FixCvarCheats();

	/* send intentions now */
	CL_SendCmd();

	/* resend a connection request if necessary */
	CL_CheckForResend();
}

void CL_Frame(int msec)
{
	static int extratime;
	static int lasttimecalled;

	if (dedicated->value)
		return;

	extratime += msec;

	if (!cl_timedemo->value)
	{
		if ((cls.state == ca_connected) && (extratime < 100))
			return; /* don't flood packets out while connecting */

		if (extratime < 1000 / cl_maxfps->value)
			return; /* framerate is too high */
	}

	/* decide the simulation time */
	cls.frametime = extratime / 1000.0;

	cl.time += extratime;

	cls.realtime = curtime;

	extratime = 0;

	if (cls.frametime > (1.0f / 5))
	{
		cls.frametime = (1.0f / 5);
	}

	/* if in the debugger last frame, don't timeout */
	if (msec > 5000)
	{
		cls.netchan.last_received = Sys_Milliseconds();
	}

	/* fetch results from server */
	CL_ReadPackets();

	/* send a new command message to the server */
	CL_SendCommand();

	/* predict all unacknowledged movements */
	CL_PredictMovement();

	/* allow renderer DLL change */
	R_checkChanges();

	if (!cl.refresh_prepped && (cls.state == ca_active))
	{
		CL_PrepRefresh();
	}

	/* update the screen */
	if (host_speeds->value)
	{
		time_before_ref = Sys_Milliseconds();
	}

	SCR_UpdateScreen();

	if (host_speeds->value)
	{
		time_after_ref = Sys_Milliseconds();
	}

	/* update audio */
	S_Update(cl.refdef.vieworg, cl.v_forward, cl.v_right, cl.v_up);
	#ifdef CDA
	CDAudio_Update();
	#endif

	/* advance local effects for next frame */
	CL_RunDLights();

	CL_RunLightStyles();

	SCR_RunCinematic();

	SCR_RunConsole();

	cls.framecount++;

	if (log_stats->value)
	{
		if (cls.state == ca_active)
		{
			if (!lasttimecalled)
			{
				lasttimecalled = Sys_Milliseconds();

				if (log_stats_file)
				{
					fprintf(log_stats_file, "0\n");
				}
			}
			else
			{
				int now = Sys_Milliseconds();

				if (log_stats_file)
				{
					fprintf(log_stats_file, "%d\n", now - lasttimecalled);
				}

				lasttimecalled = now;
			}
		}
	}
}

void CL_Init()
{
	if (dedicated->value)
		return; /* nothing running on the client */

	/* all archived variables will now be loaded */
	Con_Init();
	S_Init();
	SCR_Init();
	R_initialize();
	IN_Init();
	V_Init();

	net_message.data = net_message_buffer;
	net_message.maxsize = sizeof(net_message_buffer);

	M_Init();

	cls.disable_screen = true; /* don't draw yet */

	CL_InitLocal();

	Cbuf_Execute();

	Key_ReadConsoleHistory();
}

void CL_Shutdown()
{
	static qboolean isdown = false;

	if (isdown)
	{
		printf("recursive shutdown\n");
		return;
	}

	isdown = true;

	CL_WriteConfiguration();

	Key_WriteConsoleHistory();

	#ifdef OGG
	OGG_Stop();
	#endif
	S_Shutdown();
	IN_Shutdown();
	R_finalize();
}
