#include "oggplayer.h"
#include "crc32.h"

OggPlayer::OggPlayer()
{
    bFileOpened     = false;
    bInitialized    = false;
    bReleaseDS      = false;
    pDS             = NULL;
    pDSB            = NULL;
    bLoop           = false;
    bDone           = false;
    bAlmostDone     = false;
	fVolume         = 1.0f;
	nLoopStart      = 0;
	nLoopEnd        = 0;
}

OggPlayer::~OggPlayer()
{
    if (bFileOpened)
        Close();

    if (bReleaseDS && pDS)
        pDS->Release();
}

bool OggPlayer::InitDirectSound( HWND hWnd )
{
	HRESULT hr;
	
	if (FAILED(hr = DirectSoundCreate8(NULL, &pDS, NULL)))
        return bInitialized = false;

	pDS->Initialize(NULL);
	pDS->SetCooperativeLevel( hWnd, DSSCL_PRIORITY );

    bReleaseDS = true;

    return bInitialized = true;
}

static const char *loop_start_tags[] = { "LOOP_START", "LOOPSTART" };
static const char *loop_end_tags[] = { "LOOP_END", "LOOPEND" };

#define ARRAYLEN(a) (sizeof(a)/sizeof(a[0]))

void calcHash( FILE *f ) {
	unsigned long var;
	crc32init( &var );
	while ( !feof( f ) ) {
		unsigned char c;
		fread( (void*)&c, 1, 1, f );
		crc32block( &var, &c, 1 );
	}
	fseek( f, 0, 0 );
	char s[30];
	sprintf( s, "CRC32: %x", crc32finish( &var ) );
	OutputDebugStringA( s );
}

bool OggPlayer::OpenOgg( const char *filename )
{
    if (!bInitialized)
        return false;

    if (bFileOpened)
        Close();

    FILE    *f;

    f = fopen(filename, "rb");
    if (!f) return false;

	ov_open(f, &vf, NULL, 0);

    // ok now the tricky part

    // the vorbis_info struct keeps the most of the interesting format info
    vorbis_info *vi = ov_info(&vf,-1);
	vorbis_comment *vc = ov_comment(&vf, 0);
	nLoopStart = 0;
	nLoopEnd = 0;

	if (vc != NULL) {
		for (int i = 0; i < ARRAYLEN(loop_start_tags); i++) {
			char *value = vorbis_comment_query(vc, loop_start_tags[i], 0);
			if (value != NULL) {
				nLoopStart = atoi(value);
			}
		}
		for (int i = 0; i < ARRAYLEN(loop_end_tags); i++) {
			char *value = vorbis_comment_query(vc, loop_end_tags[i], 0);
			if (value != NULL) {
				nLoopEnd = atoi(value);
			}
		}
	}

	if (nLoopEnd <= nLoopStart) {
		nLoopEnd = ov_pcm_total(&vf, -1);
	}

    // set the wave format
	WAVEFORMATEX	    wfm;

    memset(&wfm, 0, sizeof(wfm));

    wfm.cbSize          = sizeof(wfm);
    wfm.nChannels       = vi->channels;
    wfm.wBitsPerSample  = 16;                    // ogg vorbis is always 16 bit
    wfm.nSamplesPerSec  = vi->rate;
    wfm.nAvgBytesPerSec = wfm.nSamplesPerSec*wfm.nChannels*2;
    wfm.nBlockAlign     = 2*wfm.nChannels;
    wfm.wFormatTag      = 1;


    // set up the buffer
	DSBUFFERDESC desc;

	desc.dwSize         = sizeof(desc);
	desc.dwFlags        = DSBCAPS_CTRLVOLUME;
	desc.lpwfxFormat    = &wfm;
	desc.dwReserved     = 0;

    desc.dwBufferBytes  = BUFSIZE*2;
    pDS->CreateSoundBuffer(&desc, &pDSB, NULL );

    // fill the buffer

    DWORD   pos = 0;
    int     sec = 0;
    int     ret = 1;
    DWORD   size = BUFSIZE*2;

    char    *buf;

    pDSB->Lock(0, size, (LPVOID*)&buf, &size, NULL, NULL, DSBLOCK_ENTIREBUFFER);
    
    // now read in the bits
    while(ret && pos<size)
    {
        ret = ov_read(&vf, buf+pos, size-pos, 0, 2, 1, &sec);
        pos += ret;
    }

	pDSB->Unlock( buf, size, NULL, NULL );

    nCurSection         =
    nLastSection        = 0;

    return bFileOpened = true;
}

void OggPlayer::Close()
{
    bFileOpened = false;

	if (pDSB) {
        pDSB->Release();
		pDSB = NULL;
	}
}


void OggPlayer::Play(bool loop)
{
    if (!bInitialized)
        return;

    if (!bFileOpened)
        return;

	if (pDSB == NULL)
		return;

    // play looping because we will fill the buffer
    pDSB->Play(0,0,DSBPLAY_LOOPING);    
	SetVolume(fVolume);

    bLoop = loop;
    bDone = false;
    bAlmostDone = false;
}

void OggPlayer::Stop()
{
    if (!bInitialized)
        return;

    if (!bFileOpened)
        return;

    pDSB->Stop();
}

void OggPlayer::Update()
{
    DWORD pos;

	if (!bInitialized)
		return;

	if (pDSB == NULL)
		return;

    pDSB->GetCurrentPosition(&pos, NULL);

    nCurSection = pos<BUFSIZE?0:1;

    // section changed?
    if (nCurSection != nLastSection)
    {
        if (bDone && !bLoop)
            Stop();

        // gotta use this trick 'cause otherwise there wont be played all bits
        if (bAlmostDone && !bLoop)
            bDone = true;

        DWORD   size = BUFSIZE;
        char    *buf;

        // fill the section we just left
        pDSB->Lock( nLastSection*BUFSIZE, size, (LPVOID*)&buf, &size, NULL, NULL, 0 );

        DWORD   pos = 0;
        int     sec = 0;
        int     ret = 1;
		int     pcm_pos;
                
        while(ret && pos<size)
        {
            ret = ov_read(&vf, buf+pos, size-pos, 0, 2, 1, &sec);
            pos += ret;
			pcm_pos = ov_pcm_tell  (&vf);
			if (pcm_pos >= nLoopEnd) {
				ret = 0;
			}
        }

        // reached the and?
        if (!ret && bLoop)
        {
            // we are looping so restart from the beginning
            // NOTE: sound with sizes smaller than BUFSIZE may be cut off

            ret = ov_pcm_seek(&vf, nLoopStart);
            ret = 1;
            while(ret && pos<size)
            {
                ret = ov_read(&vf, buf+pos, size-pos, 0, 2, 1, &sec);
                pos += ret;
            }
        }
        else if (!ret && !(bLoop))
        {
            // not looping so fill the rest with 0
            while(pos<size)
                *(buf+pos)=0; pos ++;

            // and say that after the current section no other sectin follows
            bAlmostDone = true;
        }
                
        pDSB->Unlock( buf, size, NULL, NULL );
    
        nLastSection = nCurSection;
    }
}

void OggPlayer::SetVolume(float volume) {
	fVolume = volume;
	if (pDSB != NULL) {
		volume = 0.5f + volume*0.5f;
		pDSB->SetVolume((LONG)(DSBVOLUME_MIN+(DSBVOLUME_MAX-DSBVOLUME_MIN)*volume));
	}
}
