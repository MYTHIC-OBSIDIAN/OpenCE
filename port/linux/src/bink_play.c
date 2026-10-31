/*
BINK_PLAY.C

The Bink video SDK entry points bink_playback.c uses, on this port's own
Bink decoder (bink_decoder.c, adapted from FFmpeg's: LGPL 2.1 or later), so
the Xbox's movies play as they did there: the opening titles
(bink\intro.bik) before the main menu, the credits, the attract movies.
Each frame is decoded as the game asks (BinkDoFrame), at the movie's frame
rate (BinkWait), and copied into the game's texture as x8r8g8b8
(BinkCopyToBuffer); the first audio track plays through an SDL audio stream
of its own as the frames are decoded. A movie the decoder cannot read is
skipped, as the game skips a missing one.

The prototypes match the declarations in bink_playback.c; the RAD SDK's
RADEXPLINK is __stdcall. The handle starts with the fields bink_playback.c
reads (Width, Height, Frames, FrameNum, LastFrameNum).
*/

#include "platform.h"
#include "port_config.h"
#include "bink_decoder.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void *(__stdcall *rad_memory_allocate_proc)(unsigned long size);
typedef void (__stdcall *rad_memory_free_proc)(void *memory);
typedef void *(__stdcall *bink_sound_system_open_proc)(unsigned long param);

typedef struct BINK
{
	/* (bink_playback.c's: FrameNum is the next frame's number, from 1) */
	unsigned long Width;
	unsigned long Height;
	unsigned long Frames;
	unsigned long FrameNum;
	unsigned long LastFrameNum;

	FILE *file;
	struct bink_decoder *decoder;
	SDL_AudioStream *audio;
	/* when the first frame was decoded (SDL_GetTicksNS), and how many have
	been since */
	Uint64 start;
	unsigned long decoded;
	int failed;
} *HBINK;

/* the frame rate's milliseconds, in nanoseconds, of `frames` frames */
static Uint64 bink_frames_time(HBINK bink, unsigned long frames)
{
	return (Uint64)frames * bink_decoder_fps_denominator(bink->decoder) * 1000000000ULL /
		bink_decoder_fps_numerator(bink->decoder);
}

static void bink_audio_put(void *context, float const *samples, int sample_count)
{
	HBINK bink = context;

	if (bink->audio && sample_count > 0)
		SDL_PutAudioStreamData(bink->audio, samples, sample_count * (int)sizeof(float));
}

void __stdcall RADSetMemory(rad_memory_allocate_proc allocate, rad_memory_free_proc release)
{
	(void)allocate;
	(void)release;
}

void *__stdcall BinkOpenDirectSound(unsigned long param)
{
	(void)param;
	return (void *)1;
}

/* (the movie's sound plays through SDL, BinkOpen's) */
long __stdcall BinkSetSoundSystem(bink_sound_system_open_proc open, unsigned long param)
{
	(void)open;
	(void)param;
	return 1;
}

void __stdcall BinkSetIOSize(unsigned long io_size)
{
	(void)io_size;
}

HBINK __stdcall BinkOpen(const char *name, unsigned long flags)
{
	char path[1024];
	char error[256];
	HBINK bink;

	(void)flags;
	if (!name || !name[0])
		return NULL;
	platform_translate_path(name, path, sizeof(path));
	bink = calloc(1, sizeof(*bink));
	if (!bink)
		return NULL;
	bink->file = fopen(path, "rb");
	if (!bink->file)
	{
		platform_log("bink: %s could not be opened", path);
		free(bink);
		return NULL;
	}
	bink->decoder = bink_decoder_open(bink->file, error, sizeof(error));
	if (!bink->decoder)
	{
		platform_log("bink: %s cannot be played: %s", path, error);
		fclose(bink->file);
		free(bink);
		return NULL;
	}
	bink->Width = (unsigned long)bink_decoder_width(bink->decoder);
	bink->Height = (unsigned long)bink_decoder_height(bink->decoder);
	bink->Frames = bink_decoder_frame_count(bink->decoder);
	bink->FrameNum = 1;
	if (bink_decoder_audio_rate(bink->decoder) && config_boolean("audio.enabled") &&
		SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		SDL_AudioSpec spec;

		spec.format = SDL_AUDIO_F32;
		spec.channels = bink_decoder_audio_channels(bink->decoder);
		spec.freq = bink_decoder_audio_rate(bink->decoder);
		bink->audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
		if (bink->audio)
		{
			SDL_SetAudioStreamGain(bink->audio, (float)config_real("audio.volume"));
			SDL_ResumeAudioStreamDevice(bink->audio);
		}
		else
		{
			platform_log("bink: no sound (%s)", SDL_GetError());
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
		}
	}
	platform_log("bink: playing %s (%lux%lu, %lu frames at %.2f per second%s)", path, bink->Width, bink->Height,
		bink->Frames, (double)bink_decoder_fps_numerator(bink->decoder) / bink_decoder_fps_denominator(bink->decoder),
		bink->audio ? ", with sound" : "");
	return bink;
}

void __stdcall BinkClose(HBINK bink)
{
	if (!bink)
		return;
	if (bink->audio)
	{
		SDL_DestroyAudioStream(bink->audio);
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}
	bink_decoder_close(bink->decoder);
	fclose(bink->file);
	free(bink);
}

/* decodes frame FrameNum (its sound queued), the start of the movie's
clock at the first */
long __stdcall BinkDoFrame(HBINK bink)
{
	if (!bink || bink->failed)
		return 0;
	if (!bink->decoded)
		bink->start = SDL_GetTicksNS();
	if (bink_decoder_decode(bink->decoder, (uint32_t)(bink->FrameNum - 1), bink_audio_put, bink) < 0)
	{
		platform_log("bink: frame %lu could not be decoded; the movie stops", bink->FrameNum);
		bink->failed = 1;
		/* (bink_playback.c stops a movie at its last frame) */
		bink->FrameNum = bink->Frames - 1;
		return 0;
	}
	bink->LastFrameNum = bink->FrameNum;
	bink->decoded++;
	return 0;
}

void __stdcall BinkNextFrame(HBINK bink)
{
	if (!bink || bink->failed)
		return;
	/* (after the last, the first again: a movie looped) */
	bink->FrameNum = bink->FrameNum >= bink->Frames ? 1 : bink->FrameNum + 1;
	if (bink->FrameNum == 1)
	{
		bink->decoded = 0;
		if (bink->audio)
			SDL_ClearAudioStream(bink->audio);
	}
}

/* nonzero while it is too early for the next frame */
long __stdcall BinkWait(HBINK bink)
{
	if (!bink || bink->failed || !bink->decoded)
		return 0;
	return SDL_GetTicksNS() - bink->start < bink_frames_time(bink, bink->decoded) ? 1 : 0;
}

long __stdcall BinkCopyToBuffer(HBINK bink, void *destination, long destination_pitch,
	unsigned long destination_height, unsigned long destination_x, unsigned long destination_y,
	unsigned long flags)
{
	(void)destination_x;
	(void)destination_y;
	(void)flags;
	if (!bink || !destination || !bink->decoded)
		return 0;
	bink_decoder_to_xrgb(bink->decoder, destination, destination_pitch, (int)destination_height);
	return 0;
}

void __stdcall BinkGetSummary(HBINK bink, void *summary)
{
	(void)bink;
	/* (BINKSUMMARY: 0x7C bytes, debug_bink's) */
	if (summary)
		memset(summary, 0, 0x7C);
}

void __stdcall BinkGetRealtime(HBINK bink, void *realtime, unsigned long frame_count)
{
	(void)bink;
	(void)frame_count;
	/* (BINKREALTIME: 0x38 bytes) */
	if (realtime)
		memset(realtime, 0, 0x38);
}
