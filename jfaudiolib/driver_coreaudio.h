/*
 Copyright (C) 2009 Jonathon Fowler <jf@jonof.id.au>
 
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

int CoreAudioDrv_GetError(void);
const char *CoreAudioDrv_ErrorString( int ErrorNumber );

int  CoreAudioDrv_PCM_Init(int * mixrate, int * numchannels, int * samplebits, void * initdata);
void CoreAudioDrv_PCM_Shutdown(void);
int  CoreAudioDrv_PCM_BeginPlayback(char *BufferStart, int BufferSize,
             int NumDivisions, void ( *CallBackFunc )( void ) );
void CoreAudioDrv_PCM_StopPlayback(void);
void CoreAudioDrv_PCM_Lock(void);
void CoreAudioDrv_PCM_Unlock(void);
void CoreAudioDrv_PCM_ResumePlayback(void);

int  CoreAudioDrv_CD_Init(void);
void CoreAudioDrv_CD_Shutdown(void);
int  CoreAudioDrv_CD_Play(int track, int loop);
void CoreAudioDrv_CD_Stop(void);
void CoreAudioDrv_CD_Pause(int pauseon);
int  CoreAudioDrv_CD_IsPlaying(void);
void CoreAudioDrv_CD_SetVolume(int volume);
