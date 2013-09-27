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

/**
 * CoreAudio output driver for MultiVoc
 *
 * Inspired by the example set by the Audiere sound library available at
 *   https://audiere.svn.sourceforge.net/svnroot/audiere/trunk/audiere/
 *
 */

/*
 * The driver was modified to support modern OSX API
 * Added CD emulation in order to play music stored in OGG-files
 */


#include <AudioUnit/AudioUnit.h>
#include <pthread.h>
#include "driver_coreaudio.h"
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

enum {
    CAErr_Warning = -2,
    CAErr_Error   = -1,
    CAErr_Ok      = 0,
	CAErr_Uninitialised,
	CAErr_FindNextComponent,
	CAErr_OpenAComponent,
	CAErr_AudioUnitInitialize,
	CAErr_AudioUnitSetProperty,
	CAErr_Mutex
};

static int ErrorCode = CAErr_Ok;
static int Initialised = 0;
static int Playing = 0;

static AudioComponent comp;
static AudioComponentInstance output_audio_unit;
static AudioComponentInstance music_audio_unit;
static pthread_mutex_t mutex;

static char *MixBuffer = 0;
static int MixBufferSize = 0;
static int MixBufferCount = 0;
static int MixBufferCurrent = 0;
static int MixBufferUsed = 0;
static void (*MixCallBack)( void ) = 0;

static OSStatus fillInput(
                          void                          *inRefCon,
                          AudioUnitRenderActionFlags    *ioActionFlags,
                          const AudioTimeStamp          *inTimeStamp,
                          UInt32						inBusNumber,
                          UInt32						inNumberFrames,
                          AudioBufferList               *ioDataList)

{
	UInt32 remaining, len;
	char *ptr, *sptr;
    AudioBuffer *ioData;
	
    ioData = &ioDataList->mBuffers[0];
	remaining = ioData->mDataByteSize;
	ptr = ioData->mData;
	
	while (remaining > 0) {
		if (MixBufferUsed == MixBufferSize) {
			CoreAudioDrv_PCM_Lock();
			MixCallBack();
			CoreAudioDrv_PCM_Unlock();
			
			MixBufferUsed = 0;
			MixBufferCurrent++;
			if (MixBufferCurrent >= MixBufferCount) {
				MixBufferCurrent -= MixBufferCount;
			}
		}
		
		while (remaining > 0 && MixBufferUsed < MixBufferSize) {
			sptr = MixBuffer + (MixBufferCurrent * MixBufferSize) + MixBufferUsed;
			
			len = MixBufferSize - MixBufferUsed;
			if (remaining < len) {
				len = remaining;
			}
			
            memcpy(ptr, sptr, len);
			
			ptr += len;
			MixBufferUsed += len;
			remaining -= len;
		}
	}
    
	return noErr;
}

int CoreAudioDrv_GetError(void)
{
	return ErrorCode;
}

const char *CoreAudioDrv_ErrorString( int ErrorNumber )
{
	const char *ErrorString;
	
    switch( ErrorNumber )
	{
        case CAErr_Warning :
        case CAErr_Error :
            ErrorString = CoreAudioDrv_ErrorString( ErrorCode );
            break;
			
        case CAErr_Ok :
            ErrorString = "CoreAudio ok.";
            break;
			
		case CAErr_Uninitialised:
			ErrorString = "CoreAudio uninitialised.";
			break;
			
		case CAErr_FindNextComponent:
			ErrorString = "CoreAudio error: FindNextComponent returned NULL.";
			break;
			
		case CAErr_OpenAComponent:
			ErrorString = "CoreAudio error: OpenAComponent failed.";
			break;
			
		case CAErr_AudioUnitInitialize:
			ErrorString = "CoreAudio error: AudioUnitInitialize failed.";
			break;
			
		case CAErr_AudioUnitSetProperty:
			ErrorString = "CoreAudio error: AudioUnitSetProperty failed.";
			break;
            
		case CAErr_Mutex:
			ErrorString = "CoreAudio error: could not initialise mutex.";
			break;
			
		default:
			ErrorString = "Unknown CoreAudio error code.";
			break;
	}
	
	return ErrorString;
}

static void setMixerCallback(void) {
    AURenderCallbackStruct callback;
	callback.inputProc = fillInput;
	callback.inputProcRefCon = 0;
	AudioUnitSetProperty(output_audio_unit,
                         kAudioUnitProperty_SetRenderCallback,
                         kAudioUnitScope_Input,
                         0,
                         &callback,
                         sizeof(callback));
}

int CoreAudioDrv_PCM_Init(int * mixrate, int * numchannels, int * samplebits, void * initdata)
{
	OSStatus result = noErr;
	AudioComponentDescription desc;
	AudioStreamBasicDescription requestedDesc;
	
	if (Initialised) {
		CoreAudioDrv_PCM_Shutdown();
	}
	
	if (pthread_mutex_init(&mutex, 0) < 0) {
		ErrorCode = CAErr_Mutex;
		return CAErr_Error;
	}
    
    // Setup a AudioStreamBasicDescription with the requested format
	requestedDesc.mFormatID = kAudioFormatLinearPCM;
	requestedDesc.mFormatFlags = kLinearPCMFormatFlagIsPacked;
	requestedDesc.mChannelsPerFrame = *numchannels;
	requestedDesc.mSampleRate = *mixrate;
	
	requestedDesc.mBitsPerChannel = *samplebits;
	if (*samplebits == 16) {
		requestedDesc.mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
	}
	
	requestedDesc.mFramesPerPacket = 1;
	requestedDesc.mBytesPerFrame = requestedDesc.mBitsPerChannel * requestedDesc.mChannelsPerFrame / 8;
	requestedDesc.mBytesPerPacket = requestedDesc.mBytesPerFrame * requestedDesc.mFramesPerPacket;
	
    // Locate the default output audio unit
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	
    comp = AudioComponentFindNext(NULL, &desc);
	if (comp == NULL) {
        ErrorCode = CAErr_FindNextComponent;
		pthread_mutex_destroy(&mutex);
        return CAErr_Error;
	}
	
    // Open & initialize the default output audio unit
    result = AudioComponentInstanceNew(comp, &output_audio_unit);
	if (result != noErr) {
        ErrorCode = CAErr_OpenAComponent;
        //CloseComponent(output_audio_unit);
        pthread_mutex_destroy(&mutex);
        return CAErr_Error;
	}
	
	result = AudioUnitInitialize(output_audio_unit);
	if (result != noErr) {
        ErrorCode = CAErr_AudioUnitInitialize;
        //CloseComponent(output_audio_unit);
		pthread_mutex_destroy(&mutex);
        return CAErr_Error;
	}
	
    // Set the input format of the audio unit.
	result = AudioUnitSetProperty(output_audio_unit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  0,
                                  &requestedDesc,
                                  sizeof(requestedDesc));
	if (result != noErr) {
        ErrorCode = CAErr_AudioUnitSetProperty;
        //CloseComponent(output_audio_unit);
		pthread_mutex_destroy(&mutex);
        return CAErr_Error;
	}
	
    // Set the audio callback
    setMixerCallback();
	
	Initialised = 1;
	
	return CAErr_Ok;
}

void CoreAudioDrv_PCM_Shutdown(void)
{
	OSStatus result;
	struct AURenderCallbackStruct callback;
    
	if (!Initialised) {
		return;
	}
	
    // stop processing the audio unit
	CoreAudioDrv_PCM_StopPlayback();
	
    // Remove the input callback
	callback.inputProc = 0;
	callback.inputProcRefCon = 0;
	result = AudioUnitSetProperty(output_audio_unit,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Input,
                                  0,
                                  &callback,
                                  sizeof(callback));
	//result = CloseComponent(output_audio_unit);
    
	pthread_mutex_destroy(&mutex);
	
	Initialised = 0;
}

int CoreAudioDrv_PCM_BeginPlayback(char *BufferStart, int BufferSize,
                                   int NumDivisions, void ( *CallBackFunc )( void ) )
{
	if (!Initialised) {
		ErrorCode = CAErr_Uninitialised;
		return CAErr_Error;
	}
	
	if (Playing) {
		CoreAudioDrv_PCM_StopPlayback();
	}
	
	MixBuffer = BufferStart;
	MixBufferSize = BufferSize;
	MixBufferCount = NumDivisions;
	MixBufferCurrent = 0;
	MixBufferUsed = 0;
	MixCallBack = CallBackFunc;
	
	// prime the buffer
	MixCallBack();
	
	AudioOutputUnitStart(output_audio_unit);
	
	Playing = 1;
	
	return CAErr_Ok;
}

void CoreAudioDrv_PCM_StopPlayback(void)
{
	if (!Initialised || !Playing) {
		return;
	}
	
	CoreAudioDrv_PCM_Lock();
	AudioOutputUnitStop(output_audio_unit);
	CoreAudioDrv_PCM_Unlock();
	
	Playing = 0;
}

void CoreAudioDrv_PCM_Lock(void)
{
	pthread_mutex_lock(&mutex);
}

void CoreAudioDrv_PCM_Unlock(void)
{
	pthread_mutex_unlock(&mutex);
}

void CoreAudioDrv_PCM_ResumePlayback(void) {
    CoreAudioDrv_PCM_Lock();
    AudioOutputUnitStart(output_audio_unit);
    CoreAudioDrv_PCM_Unlock();
}

void CoreAudioDrv_PCM_SetVolume(float volume) {
    volume = 0;
    OSStatus result = AudioUnitSetParameter (output_audio_unit, kHALOutputParam_Volume, kAudioUnitScope_Output, 0, volume, 0);
}

/*
 *  CD emulation
 */

static OggVorbis_File vf;
static char current_track[200] = {0};
static int current_loop = -1;
static int music_volume = 250.0f;
static int file_opened = 0;
static ogg_int64_t loop_start, loop_end;

static const char *loop_start_tags[] = { "LOOP_START", "LOOPSTART" };
static const char *loop_end_tags[] = { "LOOP_END", "LOOPEND" };

#define ARRAYLEN(a) (sizeof(a)/sizeof(a[0]))



static OSStatus musicInput(
                           void                          *inRefCon,
                           AudioUnitRenderActionFlags    *ioActionFlags,
                           const AudioTimeStamp          *inTimeStamp,
                           UInt32						inBusNumber,
                           UInt32						inNumberFrames,
                           AudioBufferList               *ioDataList)

{
	UInt32 remaining;
	char *ptr, *sptr, *end;
    AudioBuffer *ioData;
	int sec = 0;
    int ret;
    
    ioData = &ioDataList->mBuffers[0];
	remaining = ioData->mDataByteSize;
	ptr = ioData->mData;
    sptr = ptr;
    end = ptr+remaining;
    
    do {
        ret = ov_read(&vf, sptr, end-sptr, 0, 2, 1, &sec);
        if (ret > 0) {
            sptr += ret;
            if (ov_pcm_tell(&vf) > loop_end) {
                ret = 0;
            }
        }
    } while (ret > 0 && sptr < end);
    
    if (ret == 0) {
        ov_pcm_seek(&vf, loop_start);
        do {
            ret = ov_read(&vf, sptr, end-sptr, 0, 2, 1, &sec);
            if (ret > 0) {
                sptr += ret;
            }
        } while (ret > 0 && sptr < end);
    }
    return noErr;
}

int  CoreAudioDrv_CD_Init(void) {
    music_audio_unit = NULL;
    return 0;
}

void CoreAudioDrv_CD_Shutdown(void) {
    CoreAudioDrv_CD_Stop();
}


static int CD_PlayFile(const char *filename, int loop) {
    FILE *f;
    OSStatus result;
	AudioStreamBasicDescription requestedDesc;
    
    if (!strcasecmp(filename, current_track) && loop == current_loop) {
        return 0;
    }
    
    strcpy(current_track, filename);
    current_loop = loop;
    
    if (music_audio_unit != NULL) {
        CoreAudioDrv_CD_Stop();
    }
        
    if (file_opened) {
        ov_clear(&vf);
    }
    
    f = fopen(filename, "rb");
    if (f == NULL) {
        return -1;
    }
    
    
    ov_open(f, &vf, NULL, 0);
    file_opened = 1;
    vorbis_info *vi = ov_info(&vf, -1);
    
    vorbis_comment *vc = ov_comment(&vf, 0);
	loop_start = 0;
	loop_end = 0;
    
	if (vc != NULL) {
		for (int i = 0; i < ARRAYLEN(loop_start_tags); i++) {
			char *value = vorbis_comment_query(vc, loop_start_tags[i], 0);
			if (value != NULL) {
				loop_start = atoi(value);
			}
		}
		for (int i = 0; i < ARRAYLEN(loop_end_tags); i++) {
			char *value = vorbis_comment_query(vc, loop_end_tags[i], 0);
			if (value != NULL) {
				loop_end = atoi(value);
			}
		}
	}
    
	if (loop_end <= loop_start) {
		loop_end = ov_pcm_total(&vf, -1);
	}
    
	requestedDesc.mFormatID = kAudioFormatLinearPCM;
	requestedDesc.mFormatFlags = kLinearPCMFormatFlagIsPacked | kLinearPCMFormatFlagIsSignedInteger;
	requestedDesc.mChannelsPerFrame = vi->channels;
	requestedDesc.mSampleRate = vi->rate;
	requestedDesc.mBitsPerChannel = 16;
	requestedDesc.mFramesPerPacket = 1;
	requestedDesc.mBytesPerFrame = requestedDesc.mBitsPerChannel * requestedDesc.mChannelsPerFrame / 8;
	requestedDesc.mBytesPerPacket = requestedDesc.mBytesPerFrame * requestedDesc.mFramesPerPacket;
    
    result = AudioComponentInstanceNew(comp, &music_audio_unit);
	if (result != noErr) {
        return CAErr_Error;
	}
	
	result = AudioUnitInitialize(music_audio_unit);
	if (result != noErr) {
        return CAErr_Error;
	}
	
	result = AudioUnitSetProperty(music_audio_unit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  0,
                                  &requestedDesc,
                                  sizeof(requestedDesc));
	if (result != noErr) {
        return CAErr_Error;
	}
    
    AURenderCallbackStruct callback;
    callback.inputProc = musicInput;
    callback.inputProcRefCon = 0;
    AudioUnitSetProperty(music_audio_unit,
                         kAudioUnitProperty_SetRenderCallback,
                         kAudioUnitScope_Input,
                         0,
                         &callback,
                         sizeof(callback));
	AudioOutputUnitStart(music_audio_unit);
    CoreAudioDrv_CD_SetVolume(music_volume);
    return 0;
}

int CD_PlayByName(const char *songname, const char *folder, int loop) {
    char filename[200];
    sprintf(filename, "%s/%s", folder, songname); /* MEGATON specific */
    return CD_PlayFile(filename, loop);
}

int  CoreAudioDrv_CD_Play(int track, int loop) {
    char filename[200];
    sprintf(filename, "classic/MUSIC/Track%02d.ogg", track); /* SW REDUX specific */
    return CD_PlayFile(filename, loop);
}

void CoreAudioDrv_CD_Stop(void) {
    if (music_audio_unit != NULL) {
        AudioOutputUnitStop(music_audio_unit);
        AudioUnitUninitialize(music_audio_unit);
        music_audio_unit = NULL;
        ov_clear(&vf);
        file_opened = 0;
        strcpy(current_track, "");
        current_loop = -1;
    }
}

void CoreAudioDrv_CD_Pause(int pauseon) {
    
}

int  CoreAudioDrv_CD_IsPlaying(void) {
    return music_audio_unit != NULL;
}

void CoreAudioDrv_CD_SetVolume(int volume) {
    music_volume = volume;
    if (music_audio_unit != nil) {
        AudioUnitSetParameter(music_audio_unit, kHALOutputParam_Volume, kAudioUnitScope_Output, 0, music_volume/250.0f, 0);
    }
}
