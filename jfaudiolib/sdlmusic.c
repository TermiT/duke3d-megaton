//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
//-------------------------------------------------------------------------

/*
 * A reimplementation of Jim Dose's FX_MAN routines, using SDL_mixer.
 *   Whee. FX_MAN is also known as the "Apogee Sound System", or "ASS" for
 *   short. How strangely appropriate that seems.
 */

#include <stdio.h>
#include <errno.h>

#ifndef UNREFERENCED_PARAMETER
# define UNREFERENCED_PARAMETER(x) x=x
#endif

#if defined __APPLE__ && defined __BIG_ENDIAN__
// is* hacks for ppc...
# include "compat.h"
#endif

#include "duke3d.h"
#include "cache1d.h"

#define _NEED_SDLMIXER	1
#include <SDL.h>
#include <SDL_mixer.h>
#include "music.h"

#if !defined _WIN32 && !defined(GEKKO)
# define FORK_EXEC_MIDI 1
#endif

#if defined FORK_EXEC_MIDI  // fork/exec based external midi player
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
static char **external_midi_argv;
static pid_t external_midi_pid=-1;
static int8_t external_midi_restart=0;
#endif
static char *external_midi_tempfn = "/tmp/eduke32-music.mid";
static int32_t external_midi = 0;

int32_t MUSIC_ErrorCode = MUSIC_Ok;

static char warningMessage[80];
static char errorMessage[80];

static int32_t music_initialized = 0;
static int32_t music_context = 0;
static int32_t music_loopflag = MUSIC_PlayOnce;
static Mix_Music *music_musicchunk = NULL;

int32_t g_musicSize;

static void setErrorMessage(const char *msg)
{
    strncpy(errorMessage, msg, sizeof(errorMessage));
}

// The music functions...

const char *MUSIC_ErrorString(int32_t ErrorNumber)
{
    switch (ErrorNumber)
    {
    case MUSIC_Warning:
        return(warningMessage);

    case MUSIC_Error:
        return(errorMessage);

    case MUSIC_Ok:
        return("OK; no error.");

    case MUSIC_ASSVersion:
        return("Incorrect sound library version.");

    case MUSIC_SoundCardError:
        return("General sound card error.");

    case MUSIC_InvalidCard:
        return("Invalid sound card.");

    case MUSIC_MidiError:
        return("MIDI error.");

    case MUSIC_MPU401Error:
        return("MPU401 error.");

    case MUSIC_TaskManError:
        return("Task Manager error.");

        //case MUSIC_FMNotDetected:
        //    return("FM not detected error.");

    case MUSIC_DPMI_Error:
        return("DPMI error.");

    default:
        return("Unknown error.");
    } // switch

    return(NULL);
} // MUSIC_ErrorString

#if 0 // defined ( _WIN32 )
int32_t __stdcall SetEnvironmentVariableA(const char *name, const char *value);
uint32_t __stdcall GetEnvironmentVariableA(const char *name, char *buffer, uint32_t buflen);

void setenv(const char *name, const char *value, int overwrite) {
	SetEnvironmentVariableA(name, value);
}

const char *zgetenv(const char *name) {
	static char buffer[256];
	uint32_t len;
	len = GetEnvironmentVariableA(name, buffer, sizeof(buffer)/sizeof(buffer[0])-1);
	buffer[len] = 0;
	return &buffer[0];
}

#endif

int32_t MUSIC_Init(int32_t SoundCard, int32_t Address)
{
    if (music_initialized)
    {
        setErrorMessage("Music system is already initialized.");
        return(MUSIC_Error);
    } // if
    
    Mix_Init(MIX_INIT_OGG|MIX_INIT_MP3);

    //setenv("TIMIDITY_CFG", "data/timidity.cfg", 1);
	putenv("TIMIDITY_CFG=data/timidity.cfg");
    {
        const char *s[] = { "data/timidity.cfg" };
        FILE *fp;
        int32_t i;

        for (i = (sizeof(s)/sizeof(s[0]))-1; i>=0; i--)
        {
            fp = Bfopen(s[i], "r");
            if (fp == NULL)
            {
                if (i == 0)
                {
                    initprintf("Error: couldn't open any of the following files:\n");
                    for (i = (sizeof(s)/sizeof(s[0]))-1; i>=0; i--)
                        initprintf("%s\n",s[i]);
                    return(MUSIC_Error);
                }
                continue;
            }
            else break;
        }
        Bfclose(fp);
    }

    music_initialized = 1;
    return(MUSIC_Ok);
} // MUSIC_Init


int32_t MUSIC_Shutdown(void)
{
    // TODO - make sure this is being called from the menu -- SA
#if !defined FORK_EXEC_MIDI
    if (external_midi)
        Mix_SetMusicCMD(NULL);
#endif

    MUSIC_StopSong();
    music_context = 0;
    music_initialized = 0;
    music_loopflag = MUSIC_PlayOnce;

    return(MUSIC_Ok);
} // MUSIC_Shutdown


void MUSIC_SetMaxFMMidiChannel(int32_t channel)
{
    UNREFERENCED_PARAMETER(channel);
} // MUSIC_SetMaxFMMidiChannel


void MUSIC_SetVolume(int32_t volume)
{
    volume = max(0, volume);
    volume = min(volume, 255);

    Mix_VolumeMusic(volume >> 1);  // convert 0-255 to 0-128.
} // MUSIC_SetVolume


void MUSIC_SetMidiChannelVolume(int32_t channel, int32_t volume)
{
    UNREFERENCED_PARAMETER(channel);
    UNREFERENCED_PARAMETER(volume);
} // MUSIC_SetMidiChannelVolume


void MUSIC_ResetMidiChannelVolumes(void)
{
} // MUSIC_ResetMidiChannelVolumes


int32_t MUSIC_GetVolume(void)
{
    return(Mix_VolumeMusic(-1) << 1);  // convert 0-128 to 0-255.
} // MUSIC_GetVolume


void MUSIC_SetLoopFlag(int32_t loopflag)
{
    music_loopflag = loopflag;
} // MUSIC_SetLoopFlag


int32_t MUSIC_SongPlaying(void)
{
    return((Mix_PlayingMusic()) ? TRUE : FALSE);
} // MUSIC_SongPlaying


void MUSIC_Continue(void)
{
    if (Mix_PausedMusic())
        Mix_ResumeMusic();
} // MUSIC_Continue


void MUSIC_Pause(void)
{
    Mix_PauseMusic();
} // MUSIC_Pause

int32_t MUSIC_StopSong(void)
{
#if defined FORK_EXEC_MIDI
    if (external_midi)
    {
        if (external_midi_pid > 0)
        {
            int32_t ret;
            struct timespec ts;

            external_midi_restart = 0;  // make SIGCHLD handler a no-op

            ts.tv_sec = 0;
            ts.tv_nsec = 5000000;  // sleep 5ms at most

            kill(external_midi_pid, SIGTERM);
            nanosleep(&ts, NULL);
            ret = waitpid(external_midi_pid, NULL, WNOHANG|WUNTRACED);
//            printf("(%d)", ret);

            if (ret != external_midi_pid)
            {
                if (ret==-1)
                    perror("waitpid");
                else
                {
                    // we tried to be nice, but no...
                    kill(external_midi_pid, SIGKILL);
                    initprintf("%s: wait for SIGTERM timed out.\n", __func__);
                    if (waitpid(external_midi_pid, NULL, WUNTRACED)==-1)
                        perror("waitpid (2)");
                }
            }

            external_midi_pid = -1;
        }

        return(MUSIC_Ok);
    }
#endif

    //if (!fx_initialized)
    if (!Mix_QuerySpec(NULL, NULL, NULL))
    {
        setErrorMessage("Need FX system initialized, too. Sorry.");
        return(MUSIC_Error);
    } // if

//    if ((Mix_PlayingMusic()) || (Mix_PausedMusic()))
//        Mix_HaltMusic();

    if (music_musicchunk)
        Mix_FreeMusic(music_musicchunk);

    music_musicchunk = NULL;

    return(MUSIC_Ok);
} // MUSIC_StopSong

#if defined FORK_EXEC_MIDI
static void playmusic_local()
{
    pid_t pid = vfork();

    if (pid==-1)  // error
    {
        initprintf("%s: vfork: %s\n", __func__, strerror(errno));
    }
    else if (pid==0)  // child
    {
        // exec with PATH lookup
        if (execvp(external_midi_argv[0], external_midi_argv) < 0)
        {
            perror("execv");
            _exit(1);
        }
    }
    else  // parent
    {
        external_midi_pid = pid;
    }
}

static void sigchld_handler(int signo)
{
    if (signo==SIGCHLD && external_midi_restart)
    {
        int status;

        if (external_midi_pid > 0)
        {
            if (waitpid(external_midi_pid, &status, WUNTRACED)==-1)
                perror("waitpid (3)");

            if (WIFEXITED(status) && WEXITSTATUS(status)==0)
            {
                // loop ...
                playmusic_local();
            }
        }
    }
}
#endif

// Duke3D-specific.  --ryan.
// void MUSIC_PlayMusic(char *_filename)
int32_t MUSIC_PlaySong(char *song, int32_t loopflag)
{
    MUSIC_StopSong();
    MUSIC_SetVolume(MusicVolume);

    if (external_midi)
    {
        FILE *fp;

#if defined FORK_EXEC_MIDI
        static int32_t sigchld_handler_set = 0;

        if (!sigchld_handler_set)
        {
            struct sigaction sa;
            sa.sa_handler=sigchld_handler;
            sa.sa_flags=0;
            sigemptyset(&sa.sa_mask);

            if (sigaction(SIGCHLD, &sa, NULL)==-1)
                initprintf("%s: sigaction: %s\n", __func__, strerror(errno));

            sigchld_handler_set = 1;
        }
#endif

        fp = Bfopen(external_midi_tempfn, "wb");
        if (fp)
        {
            fwrite(song, 1, g_musicSize, fp);
            Bfclose(fp);

#if defined FORK_EXEC_MIDI
            external_midi_restart = loopflag;
            playmusic_local();
#else
            music_musicchunk = Mix_LoadMUS(external_midi_tempfn);
            if (!music_musicchunk)
                initprintf("Mix_LoadMUS: %s\n", Mix_GetError());
#endif
        }
		else initprintf("%s: fopen: %s\n", __FUNCTION__, strerror(errno));
    }
    else {
        SDL_RWops *rw = SDL_RWFromMem((char *) song, g_musicSize);
        music_musicchunk = Mix_LoadMUS_RW(rw
#if (SDL_MAJOR_VERSION > 1)
            , SDL_FALSE
#endif
            );
    }

    if (music_musicchunk != NULL) {
        if (Mix_PlayMusic(music_musicchunk, (loopflag == MUSIC_LoopSong)?-1:0) == -1)
            initprintf("Mix_PlayMusic: %s\n", Mix_GetError());
    } else {
        initprintf("What? An error? Really? Here it is: %s\n", Mix_GetError());
    }

    return MUSIC_Ok;
}


void MUSIC_SetContext(int32_t context)
{
    music_context = context;
} // MUSIC_SetContext


int32_t MUSIC_GetContext(void)
{
    return(music_context);
} // MUSIC_GetContext


void MUSIC_SetSongTick(uint32_t PositionInTicks)
{
    UNREFERENCED_PARAMETER(PositionInTicks);
} // MUSIC_SetSongTick


void MUSIC_SetSongTime(uint32_t milliseconds)
{
    UNREFERENCED_PARAMETER(milliseconds);
}// MUSIC_SetSongTime


void MUSIC_SetSongPosition(int32_t measure, int32_t beat, int32_t tick)
{
    UNREFERENCED_PARAMETER(measure);
    UNREFERENCED_PARAMETER(beat);
    UNREFERENCED_PARAMETER(tick);
} // MUSIC_SetSongPosition


void MUSIC_GetSongPosition(songposition *pos)
{
    UNREFERENCED_PARAMETER(pos);
} // MUSIC_GetSongPosition


void MUSIC_GetSongLength(songposition *pos)
{
    UNREFERENCED_PARAMETER(pos);
} // MUSIC_GetSongLength


int32_t MUSIC_FadeVolume(int32_t tovolume, int32_t milliseconds)
{
    UNREFERENCED_PARAMETER(tovolume);
    Mix_FadeOutMusic(milliseconds);
    return(MUSIC_Ok);
} // MUSIC_FadeVolume


int32_t MUSIC_FadeActive(void)
{
    return((Mix_FadingMusic() == MIX_FADING_OUT) ? TRUE : FALSE);
} // MUSIC_FadeActive


void MUSIC_StopFade(void)
{
} // MUSIC_StopFade


void MUSIC_RerouteMidiChannel(int32_t channel, int32_t (*function)(int32_t, int32_t, int32_t))
{
    UNREFERENCED_PARAMETER(channel);
    UNREFERENCED_PARAMETER(function);
} // MUSIC_RerouteMidiChannel


void MUSIC_RegisterTimbreBank(char *timbres)
{
    UNREFERENCED_PARAMETER(timbres);
} // MUSIC_RegisterTimbreBank


void MUSIC_Update(void)
{}
