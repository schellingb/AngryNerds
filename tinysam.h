/* TinySAM - v0.01prerelease - Software Automatic Mouth
                no warranty implied; use at your own risk
   Do this:
      #define TINYSAM_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.
   // i.e. it should look like this:
   #include ...
   #include ...
   #define TINYSAM_IMPLEMENTATION
   #include "tinysam.h"

   Thread safety:
   Your audio output which calls the tinysam_render* functions will most likely
   run on a different thread than where the playback tinysam_speak* functions
   are called. In which case some sort of concurrency control like a
   mutex needs to be used so they are not called at the same time.
*/

#ifndef TINYSAM_INCLUDE_TINYSAM_INL
#define TINYSAM_INCLUDE_TINYSAM_INL

#ifdef __cplusplus
extern "C" {
#  define CPP_DEFAULT(x) = x
#else
#  define CPP_DEFAULT(x)
#endif

//define this if you want the API functions to be static
#ifdef TINYSAM_STATIC
#define TINYSAMDEF static
#else
#define TINYSAMDEF extern
#endif

// The main structure holding all the data which gets passed to all functions
typedef struct tinysam tinysam;

// Create a new tinysam instance
TINYSAMDEF tinysam* tinysam_create();

// Destroy and free resources
TINYSAMDEF void tinysam_destroy(tinysam* ts);

// Supported output modes by the render methods
enum TinySamOutputMode
{
	// Two channels with single left/right samples one after another
	TINYSAM_STEREO_INTERLEAVED,
	// Two channels with all samples for the left channel first then right
	TINYSAM_STEREO_UNWEAVED,
	// A single channel (stereo instruments are mixed into center)
	TINYSAM_MONO,
};

// Setup the parameters for the voice render methods
//   outputmode: if mono or stereo and how stereo channel data is ordered
//   samplerate: the number of samples per second (output frequency)
//   global_volume: global volume scale applied to the output (must be <= 1.0)
TINYSAMDEF void tinysam_set_output(tinysam* ts, enum TinySamOutputMode mode, int samplerate, float global_volume CPP_DEFAULT(1.0f));

// Set voice parameters (initially 128, 128)
TINYSAMDEF void tinysam_set_voice(tinysam* ts, unsigned char mouth, unsigned char throat);

// Set speed parameter (initially 72)
TINYSAMDEF void tinysam_set_speed(tinysam* ts, unsigned char speed);

// Set pitch parameter (initially 64)
TINYSAMDEF void tinysam_set_pitch(tinysam* ts, unsigned char pitch);

// Set singmode on or off (initially off (0))
TINYSAMDEF void tinysam_set_singmode(tinysam* ts, unsigned char singmode);

// Speak either english text or phonetics
TINYSAMDEF int tinysam_speak_english(tinysam* ts, const char* str);
TINYSAMDEF int tinysam_speak_phonetic(tinysam* ts, const char* str);

// Stop speaking after the current phoneme
TINYSAMDEF void tinysam_stop(tinysam* ts);

// Stop speaking immediatly and reset
TINYSAMDEF void tinysam_reset(tinysam* ts);

// Render output samples into a buffer
// You can either render as unsigned 8-bit values (tinysam_render_byte) or
// as signed 16-bit values (tinysam_render_short) or
// as 32-bit float values (tinysam_render_float)
//   buffer: target buffer of size samples * output_channels * sizeof(type)
//   samples: number of samples to render (per channel)
//   flag_mixing: if 0 overwrite the buffer, otherwise mix into existing data
TINYSAMDEF int tinysam_render_byte(tinysam* ts, unsigned char* buffer, int samples, int flag_mixing CPP_DEFAULT(0));
TINYSAMDEF int tinysam_render_short(tinysam* ts, short* buffer, int samples, int flag_mixing CPP_DEFAULT(0));
TINYSAMDEF int tinysam_render_float(tinysam* ts, float* buffer, int samples, int flag_mixing CPP_DEFAULT(0));

#ifdef __cplusplus
#  undef CPP_DEFAULT
}
#endif

// end header
// ---------------------------------------------------------------------------------------------------------
#endif //TINYSAM_INCLUDE_TINYSAM_INL

#ifdef TINYSAM_IMPLEMENTATION
#ifdef __cplusplus
extern "C" {
#endif

#if !defined(TINYSAM_MALLOC) || !defined(TINYSAM_FREE) || !defined(TINYSAM_REALLOC)
#  include <stdlib.h>
#  define TINYSAM_MALLOC  malloc
#  define TINYSAM_FREE    free
#  define TINYSAM_REALLOC realloc
#endif

#if !defined(TINYSAM_MEMCPY) || !defined(TINYSAM_MEMSET)
#  include <string.h>
#  define TINYSAM_MEMCPY  memcpy
#  define TINYSAM_MEMSET  memset
#  define TINYSAM_MEMMOVE  memmove
#endif

#ifdef NDEBUG
#define TINYSAM_ASSERT(test) (void)0
#define TINYSAM_DA(arr,n,l) arr[n]
#define TINYSAM_SA(arr,n)   arr[n]
#else
#define TINYSAM_ASSERT(test) (void)((test) ? 0 : (*(volatile int*)0 = 0xbad))
#define TINYSAM_DA(arr,n,l) ((arr)[TINYSAM_ASSERT((int)(n) >= 0 && (int)(n) < (int)(l)),(n)])
#define TINYSAM_SA(arr,n)   ((arr)[TINYSAM_ASSERT((int)(n) >= 0 && (int)(n) < (int)(sizeof(arr)/sizeof(*arr))),(n)])
#endif
#define TINYSAM_DEBUGPRINT(arglist) (void)0
//#define TINYSAM_DEBUGPRINT(arglist) printf arglist

struct _tinysam__phoneme { unsigned char index, length, stress; };
struct _tinysam__frame { unsigned char pitch, frequency1, frequency2, frequency3, amplitude1, amplitude2, amplitude3, consonant; };
struct _tinysam__renderstate { unsigned char glottalPulse, glottalOff, glottalQuick, phase1, phase2, phase3, consonant; };

struct tinysam
{
	float globalVolume;
	unsigned char outputmode, speed, pitch, singmode;
	unsigned char mouthFreqData[160];
	int timetable[5][5];

	int phonemesReserve, phonemesCount, phonemesCursor, phonemesTransitioned;
	struct _tinysam__phoneme* phonemes;

	int framesReserve, framesCount, framesTransitioned, framesRendered;
	struct _tinysam__frame* frames;

	struct _tinysam__renderstate lastFrameRenderState;
	int lastFrameRenderedUntil, lastFramebufferPos, renderSamples;
	unsigned short lastFrameFirstHistory;
};

struct _tinysam__output
{
	int bufferPos, flow;
	void* buffer;
	void* (*writefunc)(void* buffer, int count, unsigned char val, struct tinysam* ts);
};

TINYSAMDEF tinysam* tinysam_create()
{
	static const unsigned char defaultMouthFreqData[160] = {
		0x00, 0x13, 0x13, 0x13, 0x13, 0xA, 0xE, 0x12, 0x18, 0x1A, 0x16, 0x14, 0x10, 0x14, 0xE, 0x12, 0xE, 0x12, 0x12, 0x10, 0xC,
		0xE, 0xA, 0x12, 0xE, 0xA, 8, 6, 6, 6, 6, 0x11, 6, 6, 6, 6, 0xE, 0x10, 9, 0xA, 8, 0xA, 6, 6, 6, 5, 6, 0, 0x12, 0x1A,
		0x14, 0x1A, 0x12, 0xC, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 0xA, 0xA, 6, 6, 6, 0x2C, 0x13,
		0x00, 0x43, 0x43, 0x43, 0x43, 0x54, 0x48, 0x42, 0x3E, 0x28, 0x2C, 0x1E, 0x24, 0x2C, 0x48, 0x30, 0x24, 0x1E, 0x32, 0x24,
		0x1C, 0x44, 0x18, 0x32, 0x1E, 0x18, 0x52, 0x2E, 0x36, 0x56, 0x36, 0x43, 0x49, 0x4F, 0x1A, 0x42, 0x49, 0x25, 0x33, 0x42,
		0x28, 0x2F, 0x4F, 0x4F, 0x42, 0x4F, 0x6E, 0x00, 0x48, 0x26, 0x1E, 0x2A, 0x1E, 0x22, 0x1A, 0x1A, 0x1A, 0x42, 0x42, 0x42,
		0x6E, 0x6E, 0x6E, 0x54, 0x54, 0x54, 0x1A, 0x1A, 0x1A, 0x42, 0x42, 0x42, 0x6D, 0x56, 0x6D, 0x54, 0x54, 0x54, 0x7F, 0x7F
	};
	tinysam* ts = (tinysam*)TINYSAM_MALLOC(sizeof(tinysam));
	TINYSAM_MEMSET(ts, 0, sizeof(tinysam));
	ts->speed = 72;
	ts->pitch = 64;
	TINYSAM_MEMCPY(ts->mouthFreqData, defaultMouthFreqData, sizeof(defaultMouthFreqData));
	tinysam_set_output(ts, TINYSAM_STEREO_INTERLEAVED, 44100, 1);
	return ts;
}

TINYSAMDEF void tinysam_destroy(tinysam* ts)
{
	TINYSAM_FREE(ts->phonemes);
	TINYSAM_FREE(ts->frames);
	TINYSAM_FREE(ts);
}

TINYSAMDEF void tinysam_set_output(tinysam* ts, enum TinySamOutputMode mode, int samplerate, float global_volume)
{
	ts->outputmode = (unsigned char)mode;
	ts->globalVolume = global_volume;

	//timetable for more accurate c64 simulation, scaled to requested samplerate
	float ttscale = (samplerate / 22050.f);
	static const int default_timetable[5][5] = { {162, 167, 167, 127, 128}, {226, 60, 60, 0, 0}, {225, 60, 59, 0, 0}, {200, 0, 0, 54, 55}, {199, 0, 0, 54, 54} };
	const int* ttsrc = default_timetable[0];
	int *ttdst = ts->timetable[0];
	while (ttsrc <= &default_timetable[4][4]) *(ttdst++) = (int)(*(ttsrc++) * ttscale);
}

TINYSAMDEF void tinysam_set_voice(tinysam* ts, unsigned char mouth, unsigned char throat)
{
	static const unsigned char mouthFormants5_29[25] = { 10, 14, 19, 24, 27, 23, 21, 16, 20, 14, 18, 14, 18, 18, 16, 13, 15, 11, 18, 14, 11, 9, 6, 6, 6 };
	static const unsigned char throatFormants5_29[25] = { 84, 73, 67, 63, 40, 44, 31, 37, 45, 73, 49, 36, 30, 51, 37, 29, 69, 24, 50, 30, 24, 83, 46, 54, 86 };
	static const unsigned char mouthFormants48_53[6] = { 19, 27, 21, 27, 18, 13 };
	static const unsigned char throatFormants48_53[6] = { 72, 39, 31, 43, 30, 34 };

	unsigned char newFrequency = 0;
	unsigned char pos;

	unsigned char *mouthFreqData1 = ts->mouthFreqData, *mouthFreqData2 = ts->mouthFreqData + 80;

	#define _TS_TRANSFORM_FREQ(a, b) ((((unsigned int)(a) * (b)) >> 8) << 1)

	// recalculate formant frequencies 5..29 for the mouth (F1) and throat (F2)
	for (pos = 0; pos < 25; pos++)
	{
		// recalculate mouth frequency
		unsigned char initialFrequency = TINYSAM_SA(mouthFormants5_29, pos);
		if (initialFrequency != 0) newFrequency = _TS_TRANSFORM_FREQ(mouth, initialFrequency);
		mouthFreqData1[pos + 5] = newFrequency;

		// recalculate throat frequency
		initialFrequency = TINYSAM_SA(throatFormants5_29, pos);
		if (initialFrequency != 0) newFrequency = _TS_TRANSFORM_FREQ(throat, initialFrequency);
		mouthFreqData2[pos + 5] = newFrequency;
	}

	// recalculate formant frequencies 48..53
	for (pos = 0; pos < 6; pos++)
	{
		// recalculate F1 (mouth formant)
		unsigned char initialFrequency = TINYSAM_SA(mouthFormants48_53, pos);
		mouthFreqData1[pos + 48] = _TS_TRANSFORM_FREQ(mouth, initialFrequency);

		// recalculate F2 (throat formant)
		initialFrequency = TINYSAM_SA(throatFormants48_53, pos);
		mouthFreqData2[pos + 48] = _TS_TRANSFORM_FREQ(throat, initialFrequency);
	}

	#undef _TS_TRANSFORM_FREQ
}

TINYSAMDEF void tinysam_set_speed(tinysam* ts, unsigned char speed)
{
	ts->speed = speed;
}

TINYSAMDEF void tinysam_set_pitch(tinysam* ts, unsigned char pitch)
{
	ts->pitch = pitch;
}

TINYSAMDEF void tinysam_set_singmode(tinysam* ts, unsigned char singmode)
{
	ts->singmode = singmode;
}

TINYSAMDEF void tinysam_stop(tinysam* ts)
{
	ts->phonemesCursor       = ts->phonemesCount;
	ts->phonemesTransitioned = ts->phonemesCount;
	int framesLeft = (ts->framesCount - ts->framesRendered);
	if (!framesLeft) return;
	if (framesLeft > 5) framesLeft = 5;
	ts->framesTransitioned = ts->framesCount = ts->framesRendered + framesLeft;
	for (int i = 0; i != framesLeft; i++)
	{
		struct _tinysam__frame* pFrame = &ts->frames[ts->framesRendered + i];
		pFrame->amplitude1 = pFrame->amplitude1 * (framesLeft - i) / (framesLeft + 1);
		pFrame->amplitude2 = pFrame->amplitude2 * (framesLeft - i) / (framesLeft + 1);
		pFrame->amplitude3 = pFrame->amplitude3 * (framesLeft - i) / (framesLeft + 1);
	}
}

TINYSAMDEF void tinysam_reset(tinysam* ts)
{
	ts->phonemesCursor       = ts->phonemesCount;
	ts->phonemesTransitioned = ts->phonemesCount;
	ts->framesRendered       = ts->framesCount;
	ts->framesTransitioned   = ts->framesCount;
}

static void _tinysam__insert(tinysam* ts, int i, unsigned char index, unsigned char length, unsigned char stress)
{
	if (ts->phonemesCount + 1 > ts->phonemesReserve)
	{
		ts->phonemes = (struct _tinysam__phoneme*)TINYSAM_REALLOC(ts->phonemes, (ts->phonemesReserve += 16) * sizeof(*ts->phonemes));
	}
	if (i < ts->phonemesCount) TINYSAM_MEMMOVE(ts->phonemes+i+1, ts->phonemes+i, (ts->phonemesCount - i) * sizeof(*ts->phonemes));
	ts->phonemes[i].index = index;
	ts->phonemes[i].length = length;
	ts->phonemes[i].stress = stress;
	ts->phonemesCount++;
}

TINYSAMDEF int tinysam_speak_english(tinysam* ts, const char* str)
{
	enum {
		FLAG_NUMERIC       = 0x01, // numeric
		FLAG_RULESET2      = 0x02, // use rule set 2
		FLAG_VOICED        = 0x04, // D J L N R S T Z
		FLAG_0X08          = 0x08, // B D G J L M N R V W Z
		FLAG_DIPTHONG      = 0x10, // C G J S X Z
		FLAG_CONSONANT     = 0x20, // B C D F G H J K L M N P Q R S T V W X Y Z `
		FLAG_VOWEL_OR_Y    = 0x40, // is vowel or Y
		FLAG_ALPHA_OR_QUOT = 0x80, // alpha or '
	};

	static const unsigned char charflags[108] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0-31
		0, 2, 2, 2, 2, 2, 2, 0x82, 0, 0, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, // ' ', !
		2, 0xC0, 0xA8, 0xB0, 0xAC, 0xC0, 0xA0, 0xB8, 0xA0, 0xC0, 0xBC, 0xA0, 0xAC, 0xA8, 0xAC, 0xC0, 0xA0, 0xA0, 0xAC, 0xB4, 0xA4, 0xC0, 0xA8, 0xA8, // @, A
		0xB0, 0xC0, 0xBC, 0, 0, 0, 2, 0, 0x20, 0x20, 0x9B, 0x20, 0xC0, 0xB9, 0x20, 0xCD, 0xA3, 0x4C, 0x8A, 0x8E // X, Y, Z, [, \, ], ^, _, `
	};

	static const unsigned char rules1[] = { (unsigned char)(0x80),
		' ','(','A','.',')',                    '=','E','H','4','Y','.',(unsigned char)(' '|0x80),
		'(','A',')',' ',                        '=','A',(unsigned char)('H'|0x80),
		' ','(','A','R','E',')',' ',            '=','A','A',(unsigned char)('R'|0x80),
		' ','(','A','R',')','O',                '=','A','X',(unsigned char)('R'|0x80),
		'(','A','R',')','#',                    '=','E','H','4',(unsigned char)('R'|0x80),
		' ','^','(','A','S',')','#',            '=','E','Y','4',(unsigned char)('S'|0x80),
		'(','A',')','W','A',                    '=','A',(unsigned char)('X'|0x80),
		'(','A','W',')',                        '=','A','O',(unsigned char)('5'|0x80),
		' ',':','(','A','N','Y',')',            '=','E','H','4','N','I',(unsigned char)('Y'|0x80),
		'(','A',')','^','+','#',                '=','E','Y',(unsigned char)('5'|0x80),
		'#',':','(','A','L','L','Y',')',        '=','U','L','I',(unsigned char)('Y'|0x80),
		' ','(','A','L',')','#',                '=','U',(unsigned char)('L'|0x80),
		'(','A','G','A','I','N',')',            '=','A','X','G','E','H','4',(unsigned char)('N'|0x80),
		'#',':','(','A','G',')','E',            '=','I','H',(unsigned char)('J'|0x80),
		'(','A',')','^','%',                    '=','E',(unsigned char)('Y'|0x80),
		'(','A',')','^','+',':','#',            '=','A',(unsigned char)('E'|0x80),
		' ',':','(','A',')','^','+',' ',        '=','E','Y',(unsigned char)('4'|0x80),
		' ','(','A','R','R',')',                '=','A','X',(unsigned char)('R'|0x80),
		'(','A','R','R',')',                    '=','A','E','4',(unsigned char)('R'|0x80),
		' ','^','(','A','R',')',' ',            '=','A','A','5',(unsigned char)('R'|0x80),
		'(','A','R',')',                        '=','A','A','5',(unsigned char)('R'|0x80),
		'(','A','I','R',')',                    '=','E','H','4',(unsigned char)('R'|0x80),
		'(','A','I',')',                        '=','E','Y',(unsigned char)('4'|0x80),
		'(','A','Y',')',                        '=','E','Y',(unsigned char)('5'|0x80),
		'(','A','U',')',                        '=','A','O',(unsigned char)('4'|0x80),
		'#',':','(','A','L',')',' ',            '=','U',(unsigned char)('L'|0x80),
		'#',':','(','A','L','S',')',' ',        '=','U','L',(unsigned char)('Z'|0x80),
		'(','A','L','K',')',                    '=','A','O','4',(unsigned char)('K'|0x80),
		'(','A','L',')','^',                    '=','A','O',(unsigned char)('L'|0x80),
		' ',':','(','A','B','L','E',')',        '=','E','Y','4','B','U',(unsigned char)('L'|0x80),
		'(','A','B','L','E',')',                '=','A','X','B','U',(unsigned char)('L'|0x80),
		'(','A',')','V','O',                    '=','E','Y',(unsigned char)('4'|0x80),
		'(','A','N','G',')','+',                '=','E','Y','4','N',(unsigned char)('J'|0x80),
		'(','A','T','A','R','I',')',            '=','A','H','T','A','A','4','R','I',(unsigned char)('Y'|0x80),
		'(','A',')','T','O','M',                '=','A',(unsigned char)('E'|0x80),
		'(','A',')','T','T','I',                '=','A',(unsigned char)('E'|0x80),
		' ','(','A','T',')',' ',                '=','A','E',(unsigned char)('T'|0x80),
		' ','(','A',')','T',                    '=','A',(unsigned char)('H'|0x80),
		'(','A',')',                            '=','A',(unsigned char)('E'|0x80),

		' ','(','B',')',' ',                    '=','B','I','Y',(unsigned char)('4'|0x80),
		' ','(','B','E',')','^','#',            '=','B','I',(unsigned char)('H'|0x80),
		'(','B','E','I','N','G',')',            '=','B','I','Y','4','I','H','N',(unsigned char)('X'|0x80),
		' ','(','B','O','T','H',')',' ',        '=','B','O','W','4','T',(unsigned char)('H'|0x80),
		' ','(','B','U','S',')','#',            '=','B','I','H','4',(unsigned char)('Z'|0x80),
		'(','B','R','E','A','K',')',            '=','B','R','E','Y','5',(unsigned char)('K'|0x80),
		'(','B','U','I','L',')',                '=','B','I','H','4',(unsigned char)('L'|0x80),
		'(','B',')',                            '=',(unsigned char)('B'|0x80),

		' ','(','C',')',' ',                    '=','S','I','Y',(unsigned char)('4'|0x80),
		' ','(','C','H',')','^',                '=',(unsigned char)('K'|0x80),
		'^','E','(','C','H',')',                '=',(unsigned char)('K'|0x80),
		'(','C','H','A',')','R','#',            '=','K','E','H',(unsigned char)('5'|0x80),
		'(','C','H',')',                        '=','C',(unsigned char)('H'|0x80),
		' ','S','(','C','I',')','#',            '=','S','A','Y',(unsigned char)('4'|0x80),
		'(','C','I',')','A',                    '=','S',(unsigned char)('H'|0x80),
		'(','C','I',')','O',                    '=','S',(unsigned char)('H'|0x80),
		'(','C','I',')','E','N',                '=','S',(unsigned char)('H'|0x80),
		'(','C','I','T','Y',')',                '=','S','I','H','T','I',(unsigned char)('Y'|0x80),
		'(','C',')','+',                        '=',(unsigned char)('S'|0x80),
		'(','C','K',')',                        '=',(unsigned char)('K'|0x80),
		'(','C','O','M','M','O','D','O','R','E',')','=','K','A','A','4','M','A','H','D','O','H',(unsigned char)('R'|0x80),
		'(','C','O','M',')',                    '=','K','A','H',(unsigned char)('M'|0x80),
		'(','C','U','I','T',')',                '=','K','I','H',(unsigned char)('T'|0x80),
		'(','C','R','E','A',')',                '=','K','R','I','Y','E',(unsigned char)('Y'|0x80),
		'(','C',')',                            '=',(unsigned char)('K'|0x80),

		' ','(','D',')',' ',                    '=','D','I','Y',(unsigned char)('4'|0x80),
		' ','(','D','R','.',')',' ',            '=','D','A','A','4','K','T','E',(unsigned char)('R'|0x80),
		'#',':','(','D','E','D',')',' ',        '=','D','I','H',(unsigned char)('D'|0x80),
		'.','E','(','D',')',' ',                '=',(unsigned char)('D'|0x80),
		'#',':','^','E','(','D',')',' ',        '=',(unsigned char)('T'|0x80),
		' ','(','D','E',')','^','#',            '=','D','I',(unsigned char)('H'|0x80),
		' ','(','D','O',')',' ',                '=','D','U',(unsigned char)('W'|0x80),
		' ','(','D','O','E','S',')',            '=','D','A','H',(unsigned char)('Z'|0x80),
		'(','D','O','N','E',')',' ',            '=','D','A','H','5',(unsigned char)('N'|0x80),
		'(','D','O','I','N','G',')',            '=','D','U','W','4','I','H','N',(unsigned char)('X'|0x80),
		' ','(','D','O','W',')',                '=','D','A',(unsigned char)('W'|0x80),
		'#','(','D','U',')','A',                '=','J','U',(unsigned char)('W'|0x80),
		'#','(','D','U',')','^','#',            '=','J','A',(unsigned char)('X'|0x80),
		'(','D',')',                            '=',(unsigned char)('D'|0x80),

		' ','(','E',')',' ',                    '=','I','Y','I','Y',(unsigned char)('4'|0x80),
		'#',':','(','E',')',' ',(unsigned char)('='|0x80),
		'\'',':','^','(','E',')',' ',(unsigned char)('='|0x80),
		' ',':','(','E',')',' ',                '=','I',(unsigned char)('Y'|0x80),
		'#','(','E','D',')',' ',                '=',(unsigned char)('D'|0x80),
		'#',':','(','E',')','D',' ',(unsigned char)('='|0x80),
		'(','E','V',')','E','R',                '=','E','H','4',(unsigned char)('V'|0x80),
		'(','E',')','^','%',                    '=','I','Y',(unsigned char)('4'|0x80),
		'(','E','R','I',')','#',                '=','I','Y','4','R','I',(unsigned char)('Y'|0x80),
		'(','E','R','I',')',                    '=','E','H','4','R','I',(unsigned char)('H'|0x80),
		'#',':','(','E','R',')','#',            '=','E',(unsigned char)('R'|0x80),
		'(','E','R','R','O','R',')',            '=','E','H','4','R','O','H',(unsigned char)('R'|0x80),
		'(','E','R','A','S','E',')',            '=','I','H','R','E','Y','5',(unsigned char)('S'|0x80),
		'(','E','R',')','#',                    '=','E','H',(unsigned char)('R'|0x80),
		'(','E','R',')',                        '=','E',(unsigned char)('R'|0x80),
		' ','(','E','V','E','N',')',            '=','I','Y','V','E','H',(unsigned char)('N'|0x80),
		'#',':','(','E',')','W',(unsigned char)('='|0x80),
		'@','(','E','W',')',                    '=','U',(unsigned char)('W'|0x80),
		'(','E','W',')',                        '=','Y','U',(unsigned char)('W'|0x80),
		'(','E',')','O',                        '=','I',(unsigned char)('Y'|0x80),
		'#',':','&','(','E','S',')',' ',        '=','I','H',(unsigned char)('Z'|0x80),
		'#',':','(','E',')','S',' ',(unsigned char)('='|0x80),
		'#',':','(','E','L','Y',')',' ',        '=','L','I',(unsigned char)('Y'|0x80),
		'#',':','(','E','M','E','N','T',')',    '=','M','E','H','N',(unsigned char)('T'|0x80),
		'(','E','F','U','L',')',                '=','F','U','H',(unsigned char)('L'|0x80),
		'(','E','E',')',                        '=','I','Y',(unsigned char)('4'|0x80),
		'(','E','A','R','N',')',                '=','E','R','5',(unsigned char)('N'|0x80),
		' ','(','E','A','R',')','^',            '=','E','R',(unsigned char)('5'|0x80),
		'(','E','A','D',')',                    '=','E','H',(unsigned char)('D'|0x80),
		'#',':','(','E','A',')',' ',            '=','I','Y','A',(unsigned char)('X'|0x80),
		'(','E','A',')','S','U',                '=','E','H',(unsigned char)('5'|0x80),
		'(','E','A',')',                        '=','I','Y',(unsigned char)('5'|0x80),
		'(','E','I','G','H',')',                '=','E','Y',(unsigned char)('4'|0x80),
		'(','E','I',')',                        '=','I','Y',(unsigned char)('4'|0x80),
		' ','(','E','Y','E',')',                '=','A','Y',(unsigned char)('4'|0x80),
		'(','E','Y',')',                        '=','I',(unsigned char)('Y'|0x80),
		'(','E','U',')',                        '=','Y','U','W',(unsigned char)('5'|0x80),
		'(','E','Q','U','A','L',')',            '=','I','Y','4','K','W','U',(unsigned char)('L'|0x80),
		'(','E',')',                            '=','E',(unsigned char)('H'|0x80),

		' ','(','F',')',' ',                    '=','E','H','4',(unsigned char)('F'|0x80),
		'(','F','U','L',')',                    '=','F','U','H',(unsigned char)('L'|0x80),
		'(','F','R','I','E','N','D',')',        '=','F','R','E','H','5','N',(unsigned char)('D'|0x80),
		'(','F','A','T','H','E','R',')',        '=','F','A','A','4','D','H','E',(unsigned char)('R'|0x80),
		'(','F',')','F',(unsigned char)('='|0x80),
		'(','F',')',                            '=',(unsigned char)('F'|0x80),

		' ','(','G',')',' ',                    '=','J','I','Y',(unsigned char)('4'|0x80),
		'(','G','I','V',')',                    '=','G','I','H','5',(unsigned char)('V'|0x80),
		' ','(','G',')','I','^',                '=',(unsigned char)('G'|0x80),
		'(','G','E',')','T',                    '=','G','E','H',(unsigned char)('5'|0x80),
		'S','U','(','G','G','E','S',')',        '=','G','J','E','H','4',(unsigned char)('S'|0x80),
		'(','G','G',')',                        '=',(unsigned char)('G'|0x80),
		' ','B','#','(','G',')',                '=',(unsigned char)('G'|0x80),
		'(','G',')','+',                        '=',(unsigned char)('J'|0x80),
		'(','G','R','E','A','T',')',            '=','G','R','E','Y','4',(unsigned char)('T'|0x80),
		'(','G','O','N',')','E',                '=','G','A','O','5',(unsigned char)('N'|0x80),
		'#','(','G','H',')',(unsigned char)('='|0x80),
		' ','(','G','N',')',                    '=',(unsigned char)('N'|0x80),
		'(','G',')',                            '=',(unsigned char)('G'|0x80),

		' ','(','H',')',' ',                    '=','E','Y','4','C',(unsigned char)('H'|0x80),
		' ','(','H','A','V',')',                '=','/','H','A','E','6',(unsigned char)('V'|0x80),
		' ','(','H','E','R','E',')',            '=','/','H','I','Y',(unsigned char)('R'|0x80),
		' ','(','H','O','U','R',')',            '=','A','W','5','E',(unsigned char)('R'|0x80),
		'(','H','O','W',')',                    '=','/','H','A',(unsigned char)('W'|0x80),
		'(','H',')','#',                        '=','/',(unsigned char)('H'|0x80),
		'(','H',')',(unsigned char)('='|0x80),

		' ','(','I','N',')',                    '=','I','H',(unsigned char)('N'|0x80),
		' ','(','I',')',' ',                    '=','A','Y',(unsigned char)('4'|0x80),
		'(','I',')',' ',                        '=','A',(unsigned char)('Y'|0x80),
		'(','I','N',')','D',                    '=','A','Y','5',(unsigned char)('N'|0x80),
		'S','E','M','(','I',')',                '=','I',(unsigned char)('Y'|0x80),
		' ','A','N','T','(','I',')',            '=','A',(unsigned char)('Y'|0x80),
		'(','I','E','R',')',                    '=','I','Y','E',(unsigned char)('R'|0x80),
		'#',':','R','(','I','E','D',')',' ',    '=','I','Y',(unsigned char)('D'|0x80),
		'(','I','E','D',')',' ',                '=','A','Y','5',(unsigned char)('D'|0x80),
		'(','I','E','N',')',                    '=','I','Y','E','H',(unsigned char)('N'|0x80),
		'(','I','E',')','T',                    '=','A','Y','4','E',(unsigned char)('H'|0x80),
		'(','I','\'',')',                        '=','A','Y',(unsigned char)('5'|0x80),
		' ',':','(','I',')','^','%',            '=','A','Y',(unsigned char)('5'|0x80),
		' ',':','(','I','E',')',' ',            '=','A','Y',(unsigned char)('4'|0x80),
		'(','I',')','%',                        '=','I',(unsigned char)('Y'|0x80),
		'(','I','E',')',                        '=','I','Y',(unsigned char)('4'|0x80),
		' ','(','I','D','E','A',')',            '=','A','Y','D','I','Y','5','A',(unsigned char)('H'|0x80),
		'(','I',')','^','+',':','#',            '=','I',(unsigned char)('H'|0x80),
		'(','I','R',')','#',                    '=','A','Y',(unsigned char)('R'|0x80),
		'(','I','Z',')','%',                    '=','A','Y',(unsigned char)('Z'|0x80),
		'(','I','S',')','%',                    '=','A','Y',(unsigned char)('Z'|0x80),
		'I','^','(','I',')','^','#',            '=','I',(unsigned char)('H'|0x80),
		'+','^','(','I',')','^','+',            '=','A',(unsigned char)('Y'|0x80),
		'#',':','^','(','I',')','^','+',        '=','I',(unsigned char)('H'|0x80),
		'(','I',')','^','+',                    '=','A',(unsigned char)('Y'|0x80),
		'(','I','R',')',                        '=','E',(unsigned char)('R'|0x80),
		'(','I','G','H',')',                    '=','A','Y',(unsigned char)('4'|0x80),
		'(','I','L','D',')',                    '=','A','Y','5','L',(unsigned char)('D'|0x80),
		' ','(','I','G','N',')',                '=','I','H','G',(unsigned char)('N'|0x80),
		'(','I','G','N',')',' ',                '=','A','Y','4',(unsigned char)('N'|0x80),
		'(','I','G','N',')','^',                '=','A','Y','4',(unsigned char)('N'|0x80),
		'(','I','G','N',')','%',                '=','A','Y','4',(unsigned char)('N'|0x80),
		'(','I','C','R','O',')',                '=','A','Y','4','K','R','O',(unsigned char)('H'|0x80),
		'(','I','Q','U','E',')',                '=','I','Y','4',(unsigned char)('K'|0x80),
		'(','I',')',                            '=','I',(unsigned char)('H'|0x80),

		' ','(','J',')',' ',                    '=','J','E','Y',(unsigned char)('4'|0x80),
		'(','J',')',                            '=',(unsigned char)('J'|0x80),

		' ','(','K',')',' ',                    '=','K','E','Y',(unsigned char)('4'|0x80),
		' ','(','K',')','N',(unsigned char)('='|0x80),
		'(','K',')',                            '=',(unsigned char)('K'|0x80),

		' ','(','L',')',' ',                    '=','E','H','4',(unsigned char)('L'|0x80),
		'(','L','O',')','C','#',                '=','L','O',(unsigned char)('W'|0x80),
		'L','(','L',')',(unsigned char)('='|0x80),
		'#',':','^','(','L',')','%',            '=','U',(unsigned char)('L'|0x80),
		'(','L','E','A','D',')',                '=','L','I','Y',(unsigned char)('D'|0x80),
		' ','(','L','A','U','G','H',')',        '=','L','A','E','4',(unsigned char)('F'|0x80),
		'(','L',')',                            '=',(unsigned char)('L'|0x80),

		' ','(','M',')',' ',                    '=','E','H','4',(unsigned char)('M'|0x80),
		' ','(','M','R','.',')',' ',            '=','M','I','H','4','S','T','E',(unsigned char)('R'|0x80),
		' ','(','M','S','.',')',                '=','M','I','H','5',(unsigned char)('Z'|0x80),
		' ','(','M','R','S','.',')',' ',        '=','M','I','H','4','S','I','X',(unsigned char)('Z'|0x80),
		'(','M','O','V',')',                    '=','M','U','W','4',(unsigned char)('V'|0x80),
		'(','M','A','C','H','I','N',')',        '=','M','A','H','S','H','I','Y','5',(unsigned char)('N'|0x80),
		'M','(','M',')',(unsigned char)('='|0x80),
		'(','M',')',                            '=',(unsigned char)('M'|0x80),

		' ','(','N',')',' ',                    '=','E','H','4',(unsigned char)('N'|0x80),
		'E','(','N','G',')','+',                '=','N',(unsigned char)('J'|0x80),
		'(','N','G',')','R',                    '=','N','X',(unsigned char)('G'|0x80),
		'(','N','G',')','#',                    '=','N','X',(unsigned char)('G'|0x80),
		'(','N','G','L',')','%',                '=','N','X','G','U',(unsigned char)('L'|0x80),
		'(','N','G',')',                        '=','N',(unsigned char)('X'|0x80),
		'(','N','K',')',                        '=','N','X',(unsigned char)('K'|0x80),
		' ','(','N','O','W',')',' ',            '=','N','A','W',(unsigned char)('4'|0x80),
		'N','(','N',')',(unsigned char)('='|0x80),
		'(','N','O','N',')','E',                '=','N','A','H','4',(unsigned char)('N'|0x80),
		'(','N',')',                            '=',(unsigned char)('N'|0x80),

		' ','(','O',')',' ',                    '=','O','H','4',(unsigned char)('W'|0x80),
		'(','O','F',')',' ',                    '=','A','H',(unsigned char)('V'|0x80),
		' ','(','O','H',')',' ',                '=','O','W',(unsigned char)('5'|0x80),
		'(','O','R','O','U','G','H',')',        '=','E','R','4','O',(unsigned char)('W'|0x80),
		'#',':','(','O','R',')',' ',            '=','E',(unsigned char)('R'|0x80),
		'#',':','(','O','R','S',')',' ',        '=','E','R',(unsigned char)('Z'|0x80),
		'(','O','R',')',                        '=','A','O',(unsigned char)('R'|0x80),
		' ','(','O','N','E',')',                '=','W','A','H',(unsigned char)('N'|0x80),
		'#','(','O','N','E',')',' ',            '=','W','A','H',(unsigned char)('N'|0x80),
		'(','O','W',')',                        '=','O',(unsigned char)('W'|0x80),
		' ','(','O','V','E','R',')',            '=','O','W','5','V','E',(unsigned char)('R'|0x80),
		'P','R','(','O',')','V',                '=','U','W',(unsigned char)('4'|0x80),
		'(','O','V',')',                        '=','A','H','4',(unsigned char)('V'|0x80),
		'(','O',')','^','%',                    '=','O','W',(unsigned char)('5'|0x80),
		'(','O',')','^','E','N',                '=','O',(unsigned char)('W'|0x80),
		'(','O',')','^','I','#',                '=','O','W',(unsigned char)('5'|0x80),
		'(','O','L',')','D',                    '=','O','W','4',(unsigned char)('L'|0x80),
		'(','O','U','G','H','T',')',            '=','A','O','5',(unsigned char)('T'|0x80),
		'(','O','U','G','H',')',                '=','A','H','5',(unsigned char)('F'|0x80),
		' ','(','O','U',')',                    '=','A',(unsigned char)('W'|0x80),
		'H','(','O','U',')','S','#',            '=','A','W',(unsigned char)('4'|0x80),
		'(','O','U','S',')',                    '=','A','X',(unsigned char)('S'|0x80),
		'(','O','U','R',')',                    '=','O','H',(unsigned char)('R'|0x80),
		'(','O','U','L','D',')',                '=','U','H','5',(unsigned char)('D'|0x80),
		'(','O','U',')','^','L',                '=','A','H',(unsigned char)('5'|0x80),
		'(','O','U','P',')',                    '=','U','W','5',(unsigned char)('P'|0x80),
		'(','O','U',')',                        '=','A',(unsigned char)('W'|0x80),
		'(','O','Y',')',                        '=','O',(unsigned char)('Y'|0x80),
		'(','O','I','N','G',')',                '=','O','W','4','I','H','N',(unsigned char)('X'|0x80),
		'(','O','I',')',                        '=','O','Y',(unsigned char)('5'|0x80),
		'(','O','O','R',')',                    '=','O','H','5',(unsigned char)('R'|0x80),
		'(','O','O','K',')',                    '=','U','H','5',(unsigned char)('K'|0x80),
		'F','(','O','O','D',')',                '=','U','W','5',(unsigned char)('D'|0x80),
		'L','(','O','O','D',')',                '=','A','H','5',(unsigned char)('D'|0x80),
		'M','(','O','O','D',')',                '=','U','W','5',(unsigned char)('D'|0x80),
		'(','O','O','D',')',                    '=','U','H','5',(unsigned char)('D'|0x80),
		'F','(','O','O','T',')',                '=','U','H','5',(unsigned char)('T'|0x80),
		'(','O','O',')',                        '=','U','W',(unsigned char)('5'|0x80),
		'(','O','\'',')',                        '=','O',(unsigned char)('H'|0x80),
		'(','O',')','E',                        '=','O',(unsigned char)('W'|0x80),
		'(','O',')',' ',                        '=','O',(unsigned char)('W'|0x80),
		'(','O','A',')',                        '=','O','W',(unsigned char)('4'|0x80),
		' ','(','O','N','L','Y',')',            '=','O','W','4','N','L','I',(unsigned char)('Y'|0x80),
		' ','(','O','N','C','E',')',            '=','W','A','H','4','N',(unsigned char)('S'|0x80),
		'(','O','N','\'','T',')',                '=','O','W','4','N',(unsigned char)('T'|0x80),
		'C','(','O',')','N',                    '=','A',(unsigned char)('A'|0x80),
		'(','O',')','N','G',                    '=','A',(unsigned char)('O'|0x80),
		' ',':','^','(','O',')','N',            '=','A',(unsigned char)('H'|0x80),
		'I','(','O','N',')',                    '=','U',(unsigned char)('N'|0x80),
		'#',':','(','O','N',')',                '=','U',(unsigned char)('N'|0x80),
		'#','^','(','O','N',')',                '=','U',(unsigned char)('N'|0x80),
		'(','O',')','S','T',                    '=','O',(unsigned char)('W'|0x80),
		'(','O','F',')','^',                    '=','A','O','4',(unsigned char)('F'|0x80),
		'(','O','T','H','E','R',')',            '=','A','H','5','D','H','E',(unsigned char)('R'|0x80),
		'R','(','O',')','B',                    '=','R','A',(unsigned char)('A'|0x80),
		'^','R','(','O',')',':','#',            '=','O','W',(unsigned char)('5'|0x80),
		'(','O','S','S',')',' ',                '=','A','O','5',(unsigned char)('S'|0x80),
		'#',':','^','(','O','M',')',            '=','A','H',(unsigned char)('M'|0x80),
		'(','O',')',                            '=','A',(unsigned char)('A'|0x80),

		' ','(','P',')',' ',                    '=','P','I','Y',(unsigned char)('4'|0x80),
		'(','P','H',')',                        '=',(unsigned char)('F'|0x80),
		'(','P','E','O','P','L',')',            '=','P','I','Y','5','P','U',(unsigned char)('L'|0x80),
		'(','P','O','W',')',                    '=','P','A','W',(unsigned char)('4'|0x80),
		'(','P','U','T',')',' ',                '=','P','U','H',(unsigned char)('T'|0x80),
		'(','P',')','P',(unsigned char)('='|0x80),
		'(','P',')','S',(unsigned char)('='|0x80),
		'(','P',')','N',(unsigned char)('='|0x80),
		'(','P','R','O','F','.',')',            '=','P','R','O','H','F','E','H','4','S','E',(unsigned char)('R'|0x80),
		'(','P',')',                            '=',(unsigned char)('P'|0x80),

		' ','(','Q',')',' ',                    '=','K','Y','U','W',(unsigned char)('4'|0x80),
		'(','Q','U','A','R',')',                '=','K','W','O','H','5',(unsigned char)('R'|0x80),
		'(','Q','U',')',                        '=','K',(unsigned char)('W'|0x80),
		'(','Q',')',                            '=',(unsigned char)('K'|0x80),

		' ','(','R',')',' ',                    '=','A','A','5',(unsigned char)('R'|0x80),
		' ','(','R','E',')','^','#',            '=','R','I',(unsigned char)('Y'|0x80),
		'(','R',')','R',(unsigned char)('='|0x80),
		'(','R',')',                            '=',(unsigned char)('R'|0x80),

		' ','(','S',')',' ',                    '=','E','H','4',(unsigned char)('S'|0x80),
		'(','S','H',')',                        '=','S',(unsigned char)('H'|0x80),
		'#','(','S','I','O','N',')',            '=','Z','H','U',(unsigned char)('N'|0x80),
		'(','S','O','M','E',')',                '=','S','A','H',(unsigned char)('M'|0x80),
		'#','(','S','U','R',')','#',            '=','Z','H','E',(unsigned char)('R'|0x80),
		'(','S','U','R',')','#',                '=','S','H','E',(unsigned char)('R'|0x80),
		'#','(','S','U',')','#',                '=','Z','H','U',(unsigned char)('W'|0x80),
		'#','(','S','S','U',')','#',            '=','S','H','U',(unsigned char)('W'|0x80),
		'#','(','S','E','D',')',                '=','Z',(unsigned char)('D'|0x80),
		'#','(','S',')','#',                    '=',(unsigned char)('Z'|0x80),
		'(','S','A','I','D',')',                '=','S','E','H',(unsigned char)('D'|0x80),
		'^','(','S','I','O','N',')',            '=','S','H','U',(unsigned char)('N'|0x80),
		'(','S',')','S',(unsigned char)('='|0x80),
		'.','(','S',')',' ',                    '=',(unsigned char)('Z'|0x80),
		'#',':','.','E','(','S',')',' ',        '=',(unsigned char)('Z'|0x80),
		'#',':','^','#','(','S',')',' ',        '=',(unsigned char)('S'|0x80),
		'U','(','S',')',' ',                    '=',(unsigned char)('S'|0x80),
		' ',':','#','(','S',')',' ',            '=',(unsigned char)('Z'|0x80),
		'#','#','(','S',')',' ',                '=',(unsigned char)('Z'|0x80),
		' ','(','S','C','H',')',                '=','S',(unsigned char)('K'|0x80),
		'(','S',')','C','+',(unsigned char)('='|0x80),
		'#','(','S','M',')',                    '=','Z','U',(unsigned char)('M'|0x80),
		'#','(','S','N',')','\'',                '=','Z','U',(unsigned char)('M'|0x80),
		'(','S','T','L','E',')',                '=','S','U',(unsigned char)('L'|0x80),
		'(','S',')',                            '=',(unsigned char)('S'|0x80),

		' ','(','T',')',' ',                    '=','T','I','Y',(unsigned char)('4'|0x80),
		' ','(','T','H','E',')',' ','#',        '=','D','H','I',(unsigned char)('Y'|0x80),
		' ','(','T','H','E',')',' ',            '=','D','H','A',(unsigned char)('X'|0x80),
		'(','T','O',')',' ',                    '=','T','U',(unsigned char)('X'|0x80),
		' ','(','T','H','A','T',')',            '=','D','H','A','E',(unsigned char)('T'|0x80),
		' ','(','T','H','I','S',')',' ',        '=','D','H','I','H',(unsigned char)('S'|0x80),
		' ','(','T','H','E','Y',')',            '=','D','H','E',(unsigned char)('Y'|0x80),
		' ','(','T','H','E','R','E',')',        '=','D','H','E','H',(unsigned char)('R'|0x80),
		'(','T','H','E','R',')',                '=','D','H','E',(unsigned char)('R'|0x80),
		'(','T','H','E','I','R',')',            '=','D','H','E','H',(unsigned char)('R'|0x80),
		' ','(','T','H','A','N',')',' ',        '=','D','H','A','E',(unsigned char)('N'|0x80),
		' ','(','T','H','E','M',')',' ',        '=','D','H','A','E',(unsigned char)('N'|0x80),
		'(','T','H','E','S','E',')',' ',        '=','D','H','I','Y',(unsigned char)('Z'|0x80),
		' ','(','T','H','E','N',')',            '=','D','H','E','H',(unsigned char)('N'|0x80),
		'(','T','H','R','O','U','G','H',')',    '=','T','H','R','U','W',(unsigned char)('4'|0x80),
		'(','T','H','O','S','E',')',            '=','D','H','O','H',(unsigned char)('Z'|0x80),
		'(','T','H','O','U','G','H',')',' ',    '=','D','H','O',(unsigned char)('W'|0x80),
		'(','T','O','D','A','Y',')',            '=','T','U','X','D','E',(unsigned char)('Y'|0x80),
		'(','T','O','M','O',')','R','R','O','W','=','T','U','M','A','A',(unsigned char)('5'|0x80),
		'(','T','O',')','T','A','L',            '=','T','O','W',(unsigned char)('5'|0x80),
		' ','(','T','H','U','S',')',            '=','D','H','A','H','4',(unsigned char)('S'|0x80),
		'(','T','H',')',                        '=','T',(unsigned char)('H'|0x80),
		'#',':','(','T','E','D',')',            '=','T','I','X',(unsigned char)('D'|0x80),
		'S','(','T','I',')','#','N',            '=','C',(unsigned char)('H'|0x80),
		'(','T','I',')','O',                    '=','S',(unsigned char)('H'|0x80),
		'(','T','I',')','A',                    '=','S',(unsigned char)('H'|0x80),
		'(','T','I','E','N',')',                '=','S','H','U',(unsigned char)('N'|0x80),
		'(','T','U','R',')','#',                '=','C','H','E',(unsigned char)('R'|0x80),
		'(','T','U',')','A',                    '=','C','H','U',(unsigned char)('W'|0x80),
		' ','(','T','W','O',')',                '=','T','U',(unsigned char)('W'|0x80),
		'&','(','T',')','E','N',' ',(unsigned char)('='|0x80),
		'(','T',')',                            '=',(unsigned char)('T'|0x80),

		' ','(','U',')',' ',                    '=','Y','U','W',(unsigned char)('4'|0x80),
		' ','(','U','N',')','I',                '=','Y','U','W',(unsigned char)('N'|0x80),
		' ','(','U','N',')',                    '=','A','H',(unsigned char)('N'|0x80),
		' ','(','U','P','O','N',')',            '=','A','X','P','A','O',(unsigned char)('N'|0x80),
		'@','(','U','R',')','#',                '=','U','H','4',(unsigned char)('R'|0x80),
		'(','U','R',')','#',                    '=','Y','U','H','4',(unsigned char)('R'|0x80),
		'(','U','R',')',                        '=','E',(unsigned char)('R'|0x80),
		'(','U',')','^',' ',                    '=','A',(unsigned char)('H'|0x80),
		'(','U',')','^','^',                    '=','A','H',(unsigned char)('5'|0x80),
		'(','U','Y',')',                        '=','A','Y',(unsigned char)('5'|0x80),
		' ','G','(','U',')','#',(unsigned char)('='|0x80),
		'G','(','U',')','%',(unsigned char)('='|0x80),
		'G','(','U',')','#',                    '=',(unsigned char)('W'|0x80),
		'#','N','(','U',')',                    '=','Y','U',(unsigned char)('W'|0x80),
		'@','(','U',')',                        '=','U',(unsigned char)('W'|0x80),
		'(','U',')',                            '=','Y','U',(unsigned char)('W'|0x80),

		' ','(','V',')',' ',                    '=','V','I','Y',(unsigned char)('4'|0x80),
		'(','V','I','E','W',')',                '=','V','Y','U','W',(unsigned char)('5'|0x80),
		'(','V',')',                            '=',(unsigned char)('V'|0x80),

		' ','(','W',')',' ',                    '=','D','A','H','4','B','U','L','Y','U',(unsigned char)('W'|0x80),
		' ','(','W','E','R','E',')',            '=','W','E',(unsigned char)('R'|0x80),
		'(','W','A',')','S','H',                '=','W','A',(unsigned char)('A'|0x80),
		'(','W','A',')','S','T',                '=','W','E',(unsigned char)('Y'|0x80),
		'(','W','A',')','S',                    '=','W','A',(unsigned char)('H'|0x80),
		'(','W','A',')','T',                    '=','W','A',(unsigned char)('A'|0x80),
		'(','W','H','E','R','E',')',            '=','W','H','E','H',(unsigned char)('R'|0x80),
		'(','W','H','A','T',')',                '=','W','H','A','H',(unsigned char)('T'|0x80),
		'(','W','H','O','L',')',                '=','/','H','O','W',(unsigned char)('L'|0x80),
		'(','W','H','O',')',                    '=','/','H','U',(unsigned char)('W'|0x80),
		'(','W','H',')',                        '=','W',(unsigned char)('H'|0x80),
		'(','W','A','R',')','#',                '=','W','E','H',(unsigned char)('R'|0x80),
		'(','W','A','R',')',                    '=','W','A','O',(unsigned char)('R'|0x80),
		'(','W','O','R',')','^',                '=','W','E',(unsigned char)('R'|0x80),
		'(','W','R',')',                        '=',(unsigned char)('R'|0x80),
		'(','W','O','M',')','A',                '=','W','U','H',(unsigned char)('M'|0x80),
		'(','W','O','M',')','E',                '=','W','I','H',(unsigned char)('M'|0x80),
		'(','W','E','A',')','R',                '=','W','E',(unsigned char)('H'|0x80),
		'(','W','A','N','T',')',                '=','W','A','A','5','N',(unsigned char)('T'|0x80),
		'A','N','S','(','W','E','R',')',        '=','E',(unsigned char)('R'|0x80),
		'(','W',')',                            '=',(unsigned char)('W'|0x80),

		' ','(','X',')',' ',                    '=','E','H','4','K',(unsigned char)('R'|0x80),
		' ','(','X',')',                        '=',(unsigned char)('Z'|0x80),
		'(','X',')',                            '=','K',(unsigned char)('S'|0x80),

		' ','(','Y',')',' ',                    '=','W','A','Y',(unsigned char)('4'|0x80),
		'(','Y','O','U','N','G',')',            '=','Y','A','H','N',(unsigned char)('X'|0x80),
		' ','(','Y','O','U','R',')',            '=','Y','O','H',(unsigned char)('R'|0x80),
		' ','(','Y','O','U',')',                '=','Y','U',(unsigned char)('W'|0x80),
		' ','(','Y','E','S',')',                '=','Y','E','H',(unsigned char)('S'|0x80),
		' ','(','Y',')',                        '=',(unsigned char)('Y'|0x80),
		'F','(','Y',')',                        '=','A',(unsigned char)('Y'|0x80),
		'P','S','(','Y','C','H',')',            '=','A','Y',(unsigned char)('K'|0x80),
		'#',':','^','(','Y',')',                '=','I',(unsigned char)('Y'|0x80),
		'#',':','^','(','Y',')','I',            '=','I',(unsigned char)('Y'|0x80),
		' ',':','(','Y',')',' ',                '=','A',(unsigned char)('Y'|0x80),
		' ',':','(','Y',')','#',                '=','A',(unsigned char)('Y'|0x80),
		' ',':','(','Y',')','^','+',':','#',    '=','I',(unsigned char)('H'|0x80),
		' ',':','(','Y',')','^','#',            '=','A',(unsigned char)('Y'|0x80),
		'(','Y',')',                            '=','I',(unsigned char)('H'|0x80),

		' ','(','Z',')',' ',                    '=','Z','I','Y',(unsigned char)('4'|0x80),
		'(','Z',')',                            '=',(unsigned char)('Z'|0x80),
		0
	};

	static const unsigned char rules2[] = { (unsigned char)(0x80),
		'(','!',')',                            '=',(unsigned char)('.'|0x80),
		'(','"',')',' ',                        '=','-','A','H','5','N','K','W','O','W','T',(unsigned char)('-'|0x80),
		'(','"',')',                            '=','K','W','O','W','4','T',(unsigned char)('-'|0x80),
		'(','#',')',                            '=',' ','N','A','H','4','M','B','E',(unsigned char)('R'|0x80),
		'(','$',')',                            '=',' ','D','A','A','4','L','E',(unsigned char)('R'|0x80),
		'(','%',')',                            '=',' ','P','E','R','S','E','H','4','N',(unsigned char)('T'|0x80),
		'(','&',')',                            '=',' ','A','E','N',(unsigned char)('D'|0x80),
		'(','\'',')',                           (unsigned char)('='|0x80),
		'(','*',')',                            '=',' ','A','E','4','S','T','E','R','I','H','S',(unsigned char)('K'|0x80),
		'(','+',')',                            '=',' ','P','L','A','H','4',(unsigned char)('S'|0x80),
		'(',',',')',                            '=',(unsigned char)(','|0x80),
		' ','(','-',')',' ',                    '=',(unsigned char)('-'|0x80),
		'(','-',')',                            (unsigned char)('='|0x80),
		'(','.',')',                            '=',' ','P','O','Y','N',(unsigned char)('T'|0x80),
		'(','/',')',                            '=',' ','S','L','A','E','4','S',(unsigned char)('H'|0x80),
		'(','0',')',                            '=',' ','Z','I','Y','4','R','O',(unsigned char)('W'|0x80),
		' ','(','1','S','T',')',                '=','F','E','R','4','S',(unsigned char)('T'|0x80),
		' ','(','1','0','T','H',')',            '=','T','E','H','4','N','T',(unsigned char)('H'|0x80),
		'(','1',')',                            '=',' ','W','A','H','4',(unsigned char)('N'|0x80),
		' ','(','2','N','D',')',                '=','S','E','H','4','K','U','N',(unsigned char)('D'|0x80),
		'(','2',')',                            '=',' ','T','U','W',(unsigned char)('4'|0x80),
		' ','(','3','R','D',')',                '=','T','H','E','R','4',(unsigned char)('D'|0x80),
		'(','3',')',                            '=',' ','T','H','R','I','Y',(unsigned char)('4'|0x80),
		'(','4',')',                            '=',' ','F','O','H','4',(unsigned char)('R'|0x80),
		' ','(','5','T','H',')',                '=','F','I','H','4','F','T',(unsigned char)('H'|0x80),
		'(','5',')',                            '=',' ','F','A','Y','4',(unsigned char)('V'|0x80),
		' ','(','6','4',')',' ',                '=','S','I','H','4','K','S','T','I','Y',' ','F','O','H',(unsigned char)('R'|0x80),
		'(','6',')',                            '=',' ','S','I','H','4','K',(unsigned char)('S'|0x80),
		'(','7',')',                            '=',' ','S','E','H','4','V','U',(unsigned char)('N'|0x80),
		' ','(','8','T','H',')',                '=','E','Y','4','T',(unsigned char)('H'|0x80),
		'(','8',')',                            '=',' ','E','Y','4',(unsigned char)('T'|0x80),
		'(','9',')',                            '=',' ','N','A','Y','4',(unsigned char)('N'|0x80),
		'(',':',')',                            '=',(unsigned char)('.'|0x80),
		'(',';',')',                            '=',(unsigned char)('.'|0x80),
		'(','<',')',                            '=',' ','L','E','H','4','S',' ','D','H','A','E',(unsigned char)('N'|0x80),
		'(','=',')',                            '=',' ','I','Y','4','K','W','U','L',(unsigned char)('Z'|0x80),
		'(','>',')',                            '=',' ','G','R','E','Y','4','T','E','R',' ','D','H','A','E',(unsigned char)('N'|0x80),
		'(','?',')',                            '=',(unsigned char)('?'|0x80),
		'(','@',')',                            '=',' ','A','E','6',(unsigned char)('T'|0x80),
		'(','^',')',                            '=',' ','K','A','E','4','R','I','X',(unsigned char)('T'|0x80),
		0
	};

	#ifdef TINYSAM_SAM_COMPATIBILITY
	_tinysam__insert(ts, ts->phonemesCount, ' ', 0, 222);
	#endif

	for (const unsigned char *pStart = (const unsigned char*)str, *p = pStart; *p; p++)
	{
		#define _TS_UC(c) ((unsigned char)((c)^(((c)/96)<<5)^((c)&128)))
		unsigned char first = _TS_UC(*p); //make upper case
		if (first == '.' && !(charflags[_TS_UC(p[1])] & FLAG_NUMERIC))
		{
			_tinysam__insert(ts, ts->phonemesCount, '.', 0, 222);
			continue;
		}

		if (!charflags[first] || *p == '`' || *p > 'z')
		{
			_tinysam__insert(ts, ts->phonemesCount, ' ', 0, 222);
			continue;
		}

		const unsigned char *rCursor;
		if      (charflags[first] & FLAG_RULESET2)      rCursor = rules2;
		else if (charflags[first] & FLAG_ALPHA_OR_QUOT) rCursor = rules1;
		else { TINYSAM_ASSERT(0); return 0; }

		for (const unsigned char *rOpen, *r, *next; *rCursor; rCursor++)
		{
			if (rCursor[0] != '(' || rCursor[1] != first) continue;
			for (rOpen = rCursor, rCursor += 2; *rCursor != ')' && *rCursor == _TS_UC(p[rCursor - rOpen - 1]); rCursor++);
			if (*rCursor != ')') continue;

			int ok = 1;
			for (int dir = -1, rEndBits; ok && dir <= 1; dir += 2)
			{
				if (dir == -1) { r = rOpen;   next = p - 1;                     rEndBits = 0x80; }
				else           { r = rCursor; next = p + (rCursor - rOpen) - 1; rEndBits = '=';  }
				for (; ok && (*(r += dir) & rEndBits) != rEndBits; next += dir)
				{
					ok = 0;
					int nextchr = (next < pStart ? ' ' : _TS_UC(*next));
					unsigned char nextflags = charflags[nextchr];
					switch (*r)
					{
						case ' ': ok = !(nextflags & FLAG_ALPHA_OR_QUOT); break;     // char must not be alpha or quotation mark
						case '#': ok =  (nextflags & FLAG_VOWEL_OR_Y);    break;     // char must be a vowel or Y
						case '.': ok =  (nextflags & FLAG_0X08);          break;     // char is one of B D G J L M N R V W Z
						case '^': ok =  (nextflags & FLAG_CONSONANT);     break;     // char must be a consonant
						case '&': // char must be a dipthong or 'H' and char thereafter must be 'C' or 'S'
							ok = (nextflags & FLAG_DIPTHONG)
								|| (nextchr == 'H' && (next += dir) > pStart && ((nextchr = _TS_UC(*next)) == 'C' || nextchr == 'S'))
							; break;
						case '@': // char must be voiced or 'H' and char thereafter must be 'T', 'C' or 'S' (second check was bugged in original code)
							ok = (nextflags & FLAG_VOICED)
								#ifndef TINYSAM_SAM_COMPATIBILITY
								|| (nextchr == 'H' && (next += dir) > pStart && ((nextchr = _TS_UC(*next)) == 'T' || nextchr == 'C' || nextchr == 'S'))
								#endif
							; break;
						case '+': // char must be either 'E', 'I' or 'Y'.
							ok = (nextchr == 'E' || nextchr == 'I' || nextchr == 'Y')
							; break;
						case ':': // walk in input position until we hit a non consonant or begin of string.
							while (next >= pStart && (charflags[_TS_UC(*next)] & FLAG_CONSONANT)) next += dir;
							next -= dir; //step one back after encountering non-consonant
							ok = 1;
							break;
						case '%': // special: 'ING', 'E' not followed by alpha or quot, 'ER', 'ES', 'ED', 'EFUL' or 'ELY'
							if (dir < 0) {} //this check only applies to checks on the right
							else if (nextchr == 'I' && _TS_UC(next[1]) == 'N' && _TS_UC(next[2]) == 'G') { next += 2; ok = 1; }
							else if (nextchr != 'E') {}
							else if (!*++next || !(charflags[nextchr = _TS_UC(*next)] & FLAG_ALPHA_OR_QUOT)) ok = 1;
							else if (nextchr== 'R' || nextchr == 'S' || nextchr == 'D') ok = 1;
							else if (nextchr == 'F' && _TS_UC(next[1]) == 'U' && _TS_UC(next[1]) == 'L') { next += 2; ok = 1; }
							else if (nextchr == 'L' && _TS_UC(next[1]) == 'Y') { next++; ok = 1; }
							break;
						default: ok = (nextchr == *r && (nextflags & FLAG_ALPHA_OR_QUOT)); // character match
					}
				}
			}
			if (!ok) continue;

			TINYSAM_ASSERT((*r & 0x7F) == '=');
			while (!(*(r++) & 0x80))
			{
				_tinysam__insert(ts, ts->phonemesCount, (*r & 0x7F), 0, 222);
			}
			p += (rCursor - rOpen) - 2;
			break;
		}
		if (!*rCursor) return 0; //found no rule
		#undef _TS_UC
	}
	tinysam_speak_phonetic(ts, NULL);
	return 1;
}

#define _TS_BREAK 254

TINYSAMDEF int tinysam_speak_phonetic(tinysam* ts, const char* input)
{
	enum
	{
		pR    = 23,
		pD    = 57,
		pT    = 69,

		FLAG_PLOSIVE  = 0x0001,
		FLAG_STOPCONS = 0x0002, /* stop consonant */
		FLAG_VOICED   = 0x0004,
		FLAG_DIPTHONG = 0x0010,
		FLAG_DIP_YX   = 0x0020, /* dipthong ending with YX */
		FLAG_CONSONANT= 0x0040,
		FLAG_VOWEL    = 0x0080,
		FLAG_PUNCT    = 0x0100,
		FLAG_ALVEOLAR = 0x0400,
		FLAG_NASAL    = 0x0800,
		FLAG_LIQUIC   = 0x1000,  /* liquic consonant */
		FLAG_FRICATIVE= 0x2000,
	};

	//----------------------------------------------------------------------------------------------------------------------
	//  Ind  | phoneme |  flags       DIPTHONGS                     UNVOICED CONSONANTS           UNVOICED CONSONANTS
	// ------|---------|----------    Ind  | phoneme |  flags       Ind  | phoneme |  flags       Ind  | phoneme |  flags
	//  0    |   *     | 00000000    ------|---------|----------   ------|---------|----------   ------|---------|----------
	//  1    |  .*     | 00000000     48   |  EY     | 10110100     32   |  S*     | 01000000     55   |  **     | 01001110
	//  2    |  ?*     | 00000000     49   |  AY     | 10110100     33   |  SH     | 01000000     56   |  **     | 01001110
	//  3    |  ,*     | 00000000     50   |  OY     | 10110100     34   |  F*     | 01000000     58   |  **     | 01001110
	//  4    |  -*     | 00000000     51   |  AW     | 10010100     35   |  TH     | 01000000     59   |  **     | 01001110
	//  							  52   |  OW     | 10010100     66   |  P*     | 01001011     61   |  **     | 01001110
	//  VOWELS                        53   |  UW     | 10010100     69   |  T*     | 01001011     62   |  **     | 01001110
	//  5    |  IY     | 10100100                                   72   |  K*     | 01001011     63   |  GX     | 01001110 
	//  6    |  IH     | 10100100                                   42   |  CH     | 01001000     64   |  **     | 01001110 
	//  7    |  EH     | 10100100     21   |  YX     | 10000100     36   |  /H     | 01000000     65   |  **     | 01001110
	//  8    |  AE     | 10100100     20   |  WX     | 10000100     43   |  **     | 01000000     67   |  **     | 01001011
	//  9    |  AA     | 10100100     18   |  RX     | 10000100     45   |  **     | 01000100     68   |  **     | 01001011
	//  10   |  AH     | 10100100     19   |  LX     | 10000100     46   |  **     | 00000000     70   |  **     | 01001011
	//  11   |  AO     | 10000100     37   |  /X     | 01000000     47   |  **     | 00000000     71   |  **     | 01001011
	//  17   |  OH     | 10000100     30   |  DX     | 01001000                                   73   |  **     | 01001011
	//  12   |  UH     | 10000100                                   SPECIAL                       74   |  **     | 01001011
	//  16   |  UX     | 10000100                                   78   |  UL     | 10000000     75   |  KX     | 01001011
	//  15   |  ER     | 10000100     22   |  WH     | 01000100     79   |  UM     | 11000001     76   |  **     | 01001011
	//  13   |  AX     | 10100100                                   80   |  UN     | 11000001     77   |  **     | 01001011
	//  14   |  IX     | 10100100                                   31   |  Q*     | 01001100
	//----------------------------------------------------------------------------------------------------------------------
	static const unsigned short flags[81] = { 0x8000, 0xC100, 0xC100, 0xC100, 0xC100, 0x00A4, 0x00A4, 0x00A4, 0x00A4, 0x00A4, 0x00A4, 0x0084, 0x0084, 0x00A4, 0x00A4, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0044, 0x1044, 0x1044, 0x1044, 0x1044, 0x084C, 0x0C4C, 0x084C, 0x0448, 0x404C, 0x2440, 0x2040, 0x2040, 0x2440, 0x0040, 0x0040, 0x2444, 0x2044, 0x2044, 0x2444, 0x2048, 0x2040, 0x004C, 0x2044, 0x0000, 0x0000, 0x00B4, 0x00B4, 0x00B4, 0x0094, 0x0094, 0x0094, 0x004E, 0x004E, 0x004E, 0x044E, 0x044E, 0x044E, 0x004E, 0x004E, 0x004E, 0x004E, 0x004E, 0x004E, 0x004B, 0x004B, 0x004B, 0x044B, 0x044B, 0x044B, 0x004B, 0x004B, 0x004B, 0x004B, 0x004B, 0x004B, 0x0080, 0x00C1, 0x00C1 };
	static const unsigned char phonemeLengthTable[80] = { 0, 0x12, 0x12, 0x12, 8, 8, 8, 8, 8, 0xB, 6, 0xC, 0xA, 5, 5, 0xB, 0xA, 0xA, 0xA, 9, 8, 7, 9, 7, 6, 8, 6, 7, 7, 7, 2, 5, 2, 2, 2, 2, 2, 2, 6, 6, 7, 6, 6, 2, 8, 3, 1, 0x1E, 0xD, 0xC, 0xC, 0xC, 0xE, 9, 6, 1, 2, 5, 1, 1, 6, 1, 2, 6, 1, 2, 8, 2, 2, 4, 2, 2, 6, 1, 4, 6, 1, 4, 5, 5 }; //(probably unused) last values were originally 0xC7,0xFF
	static const unsigned char signInputTable1[81] = { ' ', '.', '?', ',', '-', 'I', 'I', 'E', 'A', 'A', 'A', 'A', 'U', 'A', 'I', 'E', 'U', 'O', 'R', 'L', 'W', 'Y', 'W', 'R', 'L', 'W', 'Y', 'M', 'N', 'N', 'D', 'Q', 'S', 'S', 'F', 'T', '/', '/', 'Z', 'Z', 'V', 'D', 'C', '*', 'J', '*', '*', '*', 'E', 'A', 'O', 'A', 'O', 'U', 'B', '*', '*', 'D', '*', '*', 'G', '*', '*', 'G', '*', '*', 'P', '*', '*', 'T', '*', '*', 'K', '*', '*', 'K', '*', '*', 'U', 'U', 'U' };
	static const unsigned char signInputTable2[81] = { '*', '*', '*', '*', '*', 'Y', 'H', 'H', 'E', 'A', 'H', 'O', 'H', 'X', 'X', 'R', 'X', 'H', 'X', 'X', 'X', 'X', 'H', '*', '*', '*', '*', '*', '*', 'X', 'X', '*', '*', 'H', '*', 'H', 'H', 'X', '*', 'H', '*', 'H', 'H', '*', '*', '*', '*', '*', 'Y', 'Y', 'Y', 'W', 'W', 'W', '*', '*', '*', '*', '*', '*', '*', '*', '*', 'X', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', '*', 'X', '*', '*', 'L', 'M', 'N' };
	static const unsigned char phonemeStressedLengthTable[80] = { 0x00, 0x12, 0x12, 0x12, 8, 0xB, 9, 0xB, 0xE, 0xF, 0xB, 0x10, 0xC, 6, 6, 0xE, 0xC, 0xE, 0xC, 0xB, 8, 8, 0xB, 0xA, 9, 8, 8, 8, 8, 8, 3, 5, 2, 2, 2, 2, 2, 2, 6, 6, 8, 6, 6, 2, 9, 4, 2, 1, 0xE, 0xF, 0xF, 0xF, 0xE, 0xE, 8, 2, 2, 7, 2, 1, 7, 2, 2, 7, 2, 2, 8, 2, 2, 6, 2, 2, 7, 2, 4, 7, 1, 4, 5, 5 };

	int phonemesStart = ts->phonemesCount;
	if (!input)
	{
		// called by tinysam_speak_english
		for (int i = ts->phonemesCount; i-- && ts->phonemes[i].stress == 222;)
			ts->phonemes[phonemesStart = i].stress = 0;
	}
	else
	{
		for (const unsigned char* p = (const unsigned char*)input; *p; p++)
			if (*p < 0x80)
				_tinysam__insert(ts, ts->phonemesCount, (*p >= 'a' && *p <= 'z' ? *p&0x5F : *p) , 0, 0);
	}

	// The input contains a string of phonemes and stress markers along the lines of:
	//
	//     DHAX KAET IHZ AH5GLIY.
	//
	// Some phonemes are 2 bytes long, such as "DH" and "AX". Others are 1 byte long,
	// such as "T" and "Z". There are also stress markers, such as "5" and ".".
	//
	// The first character of the phonemes are stored in the table signInputTable1[].
	// The second character of the phonemes are stored in the table signInputTable2[].
	// The stress characters are arranged in low to high stress order in stressInputTable[].
	// 
	// The following process is used to parse the input:
	// 
	//        First, a search is made for a 2 character match for phonemes that do not
	//        end with the '*' (wildcard) character. On a match, the index of the phoneme 
	//        is added and the input position is advanced 2 bytes.
	//
	//        If this fails, a search is made for a 1 character match against all
	//        phoneme names ending with a '*' (wildcard). If this succeeds, the 
	//        phoneme is added and the input position is advanced 1 byte.
	// 
	//        If this fails, search for a 1 character match in the stressInputTable[].
	//        If this succeeds, the stress value is placed in the last stress value
	//        at the same index of the last added phoneme, and the input position is
	//        advanced by 1 byte.
	int dstpos = phonemesStart;
	for (int srcpos = phonemesStart; srcpos != ts->phonemesCount; srcpos++)
	{
		signed int match;
		unsigned char sign1 = ts->phonemes[srcpos].index;
		unsigned char sign2 = (srcpos + 1 == ts->phonemesCount ? 0 : ts->phonemes[srcpos + 1].index);

		match = -1;
		for (int Y = 0; Y != 81; Y++)
		{
			// GET FIRST CHARACTER AT POSITION Y IN signInputTable
			// --> should change name to PhonemeNameTable1
			unsigned char A = TINYSAM_SA(signInputTable1, Y);

			if (A == sign1)
			{
				A = TINYSAM_SA(signInputTable2, Y);
				// NOT A SPECIAL AND MATCHES SECOND CHARACTER?
				if ((A != '*') && (A == sign2)) { match = Y; break; }
			}
		}
		if (match != -1)
		{
			// Matched both characters (no wildcards)
			ts->phonemes[dstpos++].index = (unsigned char)match;
			srcpos++; // Skip the second character of the input as we've matched it
		}
		else
		{
			for (int Z = 0; Z != 81; Z++)
			{
				if (TINYSAM_SA(signInputTable2, Z) == '*')
				{
					if (TINYSAM_SA(signInputTable1, Z) == sign1) { match = Z; break; }
				}
			}
			if (match != -1)
			{
				// Matched just the first character (with second character matching '*'
				ts->phonemes[dstpos++].index = (unsigned char)match;
			}
			else if (dstpos > phonemesStart)
			{
				// Should be a stress character
				match = sign1 - '0';
				TINYSAM_ASSERT(match >= 1 && match <= 8);
				if (match >= 1 && match <= 8) ts->phonemes[dstpos - 1].stress = (unsigned char)match; // Set stress for prior phoneme
			}
		}
		TINYSAM_ASSERT(dstpos <= ts->phonemesCount && srcpos <= ts->phonemesCount);
	}
	ts->phonemesCount = dstpos;

	// Rewrites the phonemes using the following rules:
	//   <DIPTHONG ENDING WITH WX> -> <DIPTHONG ENDING WITH WX> WX
	//   <DIPTHONG NOT ENDING WITH WX> -> <DIPTHONG NOT ENDING WITH WX> YX
	//   UL -> AX L
	//   UM -> AX M
	//   <STRESSED VOWEL> <SILENCE> <STRESSED VOWEL> -> <STRESSED VOWEL> <SILENCE> Q <VOWEL>
	//   T R -> CH R
	//   D R -> J R
	//   <VOWEL> R -> <VOWEL> RX
	//   <VOWEL> L -> <VOWEL> LX
	//   G S -> G Z
	//   K <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> KX <VOWEL OR DIPTHONG NOT ENDING WITH IY>
	//   G <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPTHONG NOT ENDING WITH IY>
	//   S P -> S B
	//   S T -> S D
	//   S K -> S G
	//   S KX -> S GX
	//   <ALVEOLAR> UW -> <ALVEOLAR> UX
	//   CH -> CH CH' (CH requires two phonemes to represent it)
	//   J -> J J' (J requires two phonemes to represent it)
	//   <UNSTRESSED VOWEL> T <PAUSE> -> <UNSTRESSED VOWEL> DX <PAUSE>
	//   <UNSTRESSED VOWEL> D <PAUSE>  -> <UNSTRESSED VOWEL> DX <PAUSE>
	for (int pos = phonemesStart; pos != ts->phonemesCount; pos++)
	{
		unsigned char p = ts->phonemes[pos].index;
		unsigned short pf;
		unsigned char prior;

		if (p == 0)
		{
			// Is phoneme pause?
			continue;
		}

		pf = TINYSAM_SA(flags, p);
		prior = (pos != phonemesStart ? ts->phonemes[pos - 1].index : 0);

		if ((pf & FLAG_DIPTHONG))
		{
			// <DIPTHONG ENDING WITH WX> -> <DIPTHONG ENDING WITH WX> WX
			// <DIPTHONG NOT ENDING WITH WX> -> <DIPTHONG NOT ENDING WITH WX> YX
			// Example: OIL, COW

			// If ends with IY, use YX, else use WX
			unsigned char A = (pf & FLAG_DIP_YX) ? 21 : 20; // 'WX' = 20 'YX' = 21

			// Insert at WX or YX following, copying the stress
			_tinysam__insert(ts, pos + 1, A, 0, ts->phonemes[pos].stress);

			if      (p == 53 && pos != phonemesStart) //rule_alveolar_uw, Example: NEW, DEW, SUE, ZOO, THOO, TOO
			{
				if (TINYSAM_SA(flags, ts->phonemes[pos - 1].index) & FLAG_ALVEOLAR) ts->phonemes[pos].index = 16;
			}
			else if (p == 42) //rule_ch, Example: CHEW
			{
				_tinysam__insert(ts, pos + 1, 43, 0, ts->phonemes[pos].stress);
			}
			else if (p == 44) //rule_j, Example: JAY
			{
				_tinysam__insert(ts, pos + 1, 45, 0, ts->phonemes[pos].stress);
			}
		}
		else if (p >= 78 && p <= 80)
		{
			ts->phonemes[pos].index = 13; //rule;
			if      (p == 78) _tinysam__insert(ts, pos + 1, 24, 0, ts->phonemes[pos].stress); //"UL -> AX L", Example: MEDDLE
			else if (p == 79) _tinysam__insert(ts, pos + 1, 27, 0, ts->phonemes[pos].stress); //"UM -> AX M", Example: ASTRONOMY
			else if (p == 80) _tinysam__insert(ts, pos + 1, 28, 0, ts->phonemes[pos].stress); //"UN -> AX N", Example: FUNCTION
		}
		else if ((pf & FLAG_VOWEL) && ts->phonemes[pos].stress)
		{
			// RULE:
			//       <STRESSED VOWEL> <SILENCE> <STRESSED VOWEL> -> <STRESSED VOWEL> <SILENCE> Q <VOWEL>
			// EXAMPLE: AWAY EIGHT
			if (pos + 2 < ts->phonemesCount && !ts->phonemes[pos + 1].index)
			{
				// If following phoneme is a pause, get next
				p = ts->phonemes[pos + 2].index;
				if ((TINYSAM_SA(flags, p) & FLAG_VOWEL) && ts->phonemes[pos + 2].stress)
				{
					_tinysam__insert(ts, pos + 2, 31, 0, 0); // 31 = 'Q'
				}
			}
		}
		else if (p == pR)
		{
			// RULES FOR PHONEMES BEFORE R
			if (prior == pT) ts->phonemes[pos - 1].index = 42; //"T R -> CH R", Example: TRACK
			else if (prior == pD) ts->phonemes[pos - 1].index = 44; //"D R -> J R", Example: DRY
			else if (TINYSAM_SA(flags, prior) & FLAG_VOWEL) ts->phonemes[pos].index = 18; //"<VOWEL> R -> <VOWEL> RX", Example: ART
		}
		else if (p == 24 && (TINYSAM_SA(flags, prior) & FLAG_VOWEL)) ts->phonemes[pos].index = 19; //"<VOWEL> L -> <VOWEL> LX", Example: ALL
		else if (prior == 60 && p == 32) // 'G' 'S'
		{
			// Can't get to fire -
			//   1. The G -> GX rule intervenes
			//   2. Reciter already replaces GS -> GZ
			ts->phonemes[pos].index = 38; //"G S -> G Z"
		}
		else if (p == 60) //rule_g, G <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPTHONG NOT ENDING WITH IY>, Example: GO
		{
			// If dipthong ending with YX, move continue processing next phoneme
			if (pos + 1 != ts->phonemesCount && ((TINYSAM_SA(flags, ts->phonemes[pos + 1].index) & FLAG_DIP_YX) == 0))
			{
				// replace G with GX and continue processing next phoneme
				ts->phonemes[pos].index = 63; // 'GX'
			}
		}
		else
		{
			if (p == 72) // K <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> KX <VOWEL OR DIPTHONG NOT ENDING WITH IY>, Example: COW
			{
				// If at end, replace current phoneme with KX
				if (pos + 1 == ts->phonemesCount || (TINYSAM_SA(flags, ts->phonemes[pos + 1].index) & FLAG_DIP_YX) == 0) // VOWELS AND DIPTHONGS ENDING WITH IY SOUND flag set?
				{
					ts->phonemes[pos].index = 75;
					p = 75;
					pf = TINYSAM_SA(flags, p);
				}
			}

			// Replace with softer version?
			if ((TINYSAM_SA(flags, p) & FLAG_PLOSIVE) && (prior == 32)) // 'S'
			{
				// RULE:
				//      S P -> S B
				//      S T -> S D
				//      S K -> S G
				//      S KX -> S GX
				// Examples: SPY, STY, SKY, SCOWL

				ts->phonemes[pos].index = p - 12;
			}
			else if (!(pf & FLAG_PLOSIVE))
			{
				p = ts->phonemes[pos].index;
				if (p == 53 && pos != phonemesStart) //rule_alveolar_uw, Example: NEW, DEW, SUE, ZOO, THOO, TOO
				{
					if (TINYSAM_SA(flags, ts->phonemes[pos - 1].index) & FLAG_ALVEOLAR) ts->phonemes[pos].index = 16;
				}
				else if (p == 42) //rule_ch, Example: CHEW
				{
					_tinysam__insert(ts, pos + 1, 43, 0, ts->phonemes[pos].stress);
				}
				else if (p == 44) //rule_j, Example: JAY
				{
					_tinysam__insert(ts, pos + 1, 45, 0, ts->phonemes[pos].stress);
				}
			}

			if (p == 69 || p == 57) // 'T', 'D'
			{
				// RULE: Soften T following vowel
				// NOTE: This rule fails for cases such as "ODD"
				//       <UNSTRESSED VOWEL> T <PAUSE> -> <UNSTRESSED VOWEL> DX <PAUSE>
				//       <UNSTRESSED VOWEL> D <PAUSE>  -> <UNSTRESSED VOWEL> DX <PAUSE>
				// Example: PARTY, TARDY
				if (pos != phonemesStart && (TINYSAM_SA(flags, ts->phonemes[pos - 1].index) & FLAG_VOWEL) && pos + 1 < ts->phonemesCount)
				{
					p = ts->phonemes[pos + 1].index;
					if (!p && pos + 2 < ts->phonemesCount) p = ts->phonemes[pos + 2].index;
					if ((TINYSAM_SA(flags, p) & FLAG_VOWEL) && !ts->phonemes[pos + 1].stress) ts->phonemes[pos].index = 30; //"Soften T or D following vowel or ER and preceding a pause -> DX"
				}
			}
		}
	}

	// Iterates through the phoneme buffer, copying the stress value from
	// the following phoneme under the following circumstance:
	//
	//     1. The current phoneme is voiced, excluding plosives and fricatives
	//     2. The following phoneme is voiced, excluding plosives and fricatives, and
	//     3. The following phoneme is stressed
	//
	//  In those cases, the stress value+1 from the following phoneme is copied.
	//
	// For example, the word LOITER is represented as LOY5TER, with as stress
	// of 5 on the dipthong OY. This routine will copy the stress value of 6 (5+1)
	// to the L that precedes it.
	for (int pos = phonemesStart; pos != ts->phonemesCount; pos++)
	{
		unsigned char Y = ts->phonemes[pos].index;
		// if CONSONANT_FLAG set, skip - only vowels get stress
		if (pos + 1 != ts->phonemesCount && TINYSAM_SA(flags, Y) & 64)
		{
			// if the following phoneme is the end, or a vowel, skip
			Y = ts->phonemes[pos + 1].index;
			if ((TINYSAM_SA(flags, Y) & 128) != 0)
			{
				// get the stress value at the next position
				Y = ts->phonemes[pos + 1].stress;
				if (Y && !(Y & 128))
				{
					// if next phoneme is stressed, and a VOWEL OR ER copy stress from next phoneme to this one
					ts->phonemes[pos].stress = Y + 1;
				}
			}
		}
	}

	//change phonemelength depedending on stress
	for (int pos = phonemesStart; pos != ts->phonemesCount; pos++)
	{
		unsigned char A = ts->phonemes[pos].stress;
		if ((A == 0) || ((A & 128) != 0))
		{
			ts->phonemes[pos].length = TINYSAM_SA(phonemeLengthTable, ts->phonemes[pos].index);
		}
		else
		{
			ts->phonemes[pos].length = TINYSAM_SA(phonemeStressedLengthTable, ts->phonemes[pos].index);
		}
	}

	// Applies various rules that adjust the lengths of phonemes
	//   Lengthen <FRICATIVE> or <VOICED> between <VOWEL> and <PUNCTUATION> by 1.5
	//   <VOWEL> <RX | LX> <CONSONANT> - decrease <VOWEL> length by 1
	//   <VOWEL> <UNVOICED PLOSIVE> - decrease vowel by 1/8th
	//   <VOWEL> <UNVOICED CONSONANT> - increase vowel by 1/2 + 1
	//   <NASAL> <STOP CONSONANT> - set nasal = 5, consonant = 6
	//   <VOICED STOP CONSONANT> {optional silence} <STOP CONSONANT> - shorten both to 1/2 + 1
	//   <LIQUID CONSONANT> <DIPTHONG> - decrease by 2
	//
	// LENGTHEN VOWELS PRECEDING PUNCTUATION
	//
	// Search for punctuation. If found, back up to the first vowel, then
	// process all phonemes between there and up to (but not including) the punctuation.
	// If any phoneme is found that is a either a fricative or voiced, the duration is
	// increased by (length * 1.5) + 1
	for (int pos = phonemesStart; pos != ts->phonemesCount; pos++)
	{
		unsigned char index = ts->phonemes[pos].index;
		int loopIndex;

		// not punctuation?
		if ((TINYSAM_SA(flags, index) & FLAG_PUNCT) == 0)
		{
			//++pos;
			continue;
		}

		loopIndex = pos;

		while (pos != phonemesStart && --pos != phonemesStart && !(TINYSAM_SA(flags, ts->phonemes[pos].index) & FLAG_VOWEL)); // back up while not a vowel
		if (pos == phonemesStart) break;

		do
		{
			// test for vowel
			index = ts->phonemes[pos].index;

			// test for fricative/unvoiced or not voiced
			if (!(TINYSAM_SA(flags, index) & FLAG_FRICATIVE) || (TINYSAM_SA(flags,index) & FLAG_VOICED))
			{
				//check again
				unsigned char A = ts->phonemes[pos].length;
				// change phoneme length to (length * 1.5) + 1
				ts->phonemes[pos].length = (A >> 1) + A + 1;
			}
		} while (++pos != loopIndex);
	}

	// Similar to the above routine, but shorten vowels under some circumstances
	for (int pos = phonemesStart; pos != ts->phonemesCount; pos++)
	{
		unsigned char index = ts->phonemes[pos].index;

		if (TINYSAM_SA(flags, index) & FLAG_VOWEL)
		{
			index = (pos + 1 == ts->phonemesCount ? 0 : ts->phonemes[pos + 1].index);
			if (pos + 1 != ts->phonemesCount && !(TINYSAM_SA(flags, index) & FLAG_CONSONANT))
			{
				if ((index == 18) || (index == 19))
				{
					// 'RX', 'LX'
					if (pos + 2 == ts->phonemesCount || (TINYSAM_SA(flags, ts->phonemes[pos + 2].index) & FLAG_CONSONANT))
					{
						ts->phonemes[pos].length--;
					}
				}
			}
			else
			{
				// Got here if not <VOWEL>
				unsigned short flag = (pos + 1 == ts->phonemesCount ? 65 : TINYSAM_SA(flags, index)); // 65 if end marker

				if (!(flag & FLAG_VOICED))
				{
					// Unvoiced
					// *, .*, ?*, ,*, -*, DX, S*, SH, F*, TH, /H, /X, CH, P*, T*, K*, KX
					if ((flag & FLAG_PLOSIVE))
					{
						// unvoiced plosive
						// RULE: <VOWEL> <UNVOICED PLOSIVE>
						// <VOWEL> <P*, T*, K*, KX>
						ts->phonemes[pos].length -= (ts->phonemes[pos].length >> 3);
					}
				}
				else
				{
					// decrease length
					unsigned char A = ts->phonemes[pos].length;
					ts->phonemes[pos].length = (A >> 2) + A + 1; // 5/4*A + 1
				}
			}
		}
		else if ((TINYSAM_SA(flags, index) & FLAG_NASAL) != 0)
		{
			// nasal?
			// RULE: <NASAL> <STOP CONSONANT>
			//       Set punctuation length to 6
			//       Set stop consonant length to 5
			if (pos + 1 != ts->phonemesCount && (TINYSAM_SA(flags, ts->phonemes[pos + 1].index) & FLAG_STOPCONS))
			{
				ts->phonemes[pos + 1].length = 6; // set stop consonant length to 6
				ts->phonemes[pos].length = 5; // set nasal length to 5
			}
		}
		else if ((TINYSAM_SA(flags, index) & FLAG_STOPCONS))
		{
			// (voiced) stop consonant?
			// RULE: <VOICED STOP CONSONANT> {optional silence} <STOP CONSONANT>
			//       Shorten both to (length/2 + 1)

			// move past silence
			int X = pos;
			while (++X != ts->phonemesCount && (index = ts->phonemes[X].index) == 0);

			if (X != ts->phonemesCount && (TINYSAM_SA(flags, index) & FLAG_STOPCONS))
			{
				// FIXME, this looks wrong?
				// RULE: <UNVOICED STOP CONSONANT> {optional silence} <STOP CONSONANT>
				ts->phonemes[X].length = (ts->phonemes[X].length >> 1) + 1;
				ts->phonemes[pos].length = (ts->phonemes[pos].length >> 1) + 1;
			}
		}
		else if ((TINYSAM_SA(flags, index) & FLAG_LIQUIC))
		{
			// liquic consonant?
			// RULE: <VOICED NON-VOWEL> <DIPTHONG>
			//       Decrease <DIPTHONG> by 2
			ts->phonemes[pos].length -= 2; // 20ms
		}
	}

	// Some further special processing
	for (int pos = phonemesStart; pos != ts->phonemesCount; pos++)
	{
		unsigned char index = ts->phonemes[pos].index;

		if ((TINYSAM_SA(flags, index) & FLAG_STOPCONS))
		{
			if ((TINYSAM_SA(flags, index) & FLAG_PLOSIVE))
			{
				int X = pos;
				while (++X != ts->phonemesCount && ts->phonemes[X].index == 0); // Skip pause
				if (X != ts->phonemesCount)
				{
					unsigned char A = ts->phonemes[X].index;
					if ((TINYSAM_SA(flags, A) & 8) || (A == 36) || (A == 37)) continue; // '/H' '/X'
				}
			}
			_tinysam__insert(ts, pos + 1, index + 1, TINYSAM_SA(phonemeLengthTable, index + 1), ts->phonemes[pos].stress);
			_tinysam__insert(ts, pos + 2, index + 2, TINYSAM_SA(phonemeLengthTable, index + 2), ts->phonemes[pos].stress);
			pos += 2;
		}
	}

	// Insert Breath
	for (int pos = phonemesStart, posLastPause = -1, len = 0; pos != ts->phonemesCount; pos++)
	{
		unsigned char index = ts->phonemes[pos].index;
		TINYSAM_ASSERT(index < 80);
		len += ts->phonemes[pos].length;
		if (len < 232)
		{
			if (index == _TS_BREAK) { }
			else if (!(TINYSAM_SA(flags, index) & FLAG_PUNCT))
			{
				if (index == 0) posLastPause = pos;
			}
			else
			{
				len = 0;
				_tinysam__insert(ts, ++pos, _TS_BREAK, 0, 0);
			}
		}
		else if (posLastPause > 0 && pos > posLastPause && ts->phonemes[posLastPause + 1].index != _TS_BREAK)
		{
			pos = posLastPause;
			ts->phonemes[pos].index = 31; // 'Q*' glottal stop
			ts->phonemes[pos].length = 4;
			ts->phonemes[pos].stress = 0;

			len = 0;
			_tinysam__insert(ts, ++pos, _TS_BREAK, 0, 0);
		}
	}

	return 1;
}

static int _tinysam__feedframes(tinysam* ts)
{
	static const unsigned char stressToPitch[] = { 0, 0, 0xE0, 0xE6, 0xEC, 0xF3, 0xF9, 0, 6, 0xC, 6 };
	static const unsigned char sampledConsonantFlags[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF1, 0xE2, 0xD3, 0xBB, 0x7C, 0x95, 1, 2, 3, 3, 0, 0x72, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1B, 0, 0, 0x19, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	static const unsigned char freq3data[] = { 0x00, 0x5B, 0x5B, 0x5B, 0x5B, 0x6E, 0x5D, 0x5B, 0x58, 0x59, 0x57, 0x58, 0x52, 0x59, 0x5D, 0x3E, 0x52, 0x58, 0x3E, 0x6E, 0x50, 0x5D, 0x5A, 0x3C, 0x6E, 0x5A, 0x6E, 0x51, 0x79, 0x65, 0x79, 0x5B, 0x63, 0x6A, 0x51, 0x79, 0x5D, 0x52, 0x5D, 0x67, 0x4C, 0x5D, 0x65, 0x65, 0x79, 0x65, 0x79, 0x00, 0x5A, 0x58, 0x58, 0x58, 0x58, 0x52, 0x51, 0x51, 0x51, 0x79, 0x79, 0x79, 0x70, 0x6E, 0x6E, 0x5E, 0x5E, 0x5E, 0x51, 0x51, 0x51, 0x79, 0x79, 0x79, 0x65, 0x65, 0x70, 0x5E, 0x5E, 0x5E, 0x08, 0x01 };
	static const unsigned char ampl1data[] = { 0, 0, 0, 0, 0, 0xD, 0xD, 0xE, 0xF, 0xF, 0xF, 0xF, 0xF, 0xC, 0xD, 0xC, 0xF, 0xF, 0xD, 0xD, 0xD, 0xE, 0xD, 0xC, 0xD, 0xD, 0xD, 0xC, 9, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0xB, 0xB, 0xB, 0xB, 0, 0, 1, 0xB, 0, 2, 0xE, 0xF, 0xF, 0xF, 0xF, 0xD, 2, 4, 0, 2, 4, 0, 1, 4, 0, 1, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0xC, 0, 0, 0, 0, 0xF, 0xF };
	static const unsigned char ampl2data[] = { 0, 0, 0, 0, 0, 0xA, 0xB, 0xD, 0xE, 0xD, 0xC, 0xC, 0xB, 9, 0xB, 0xB, 0xC, 0xC, 0xC, 8, 8, 0xC, 8, 0xA, 8, 8, 0xA, 3, 9, 6, 0, 0, 0, 0, 0, 0, 0, 0, 3, 5, 3, 4, 0, 0, 0, 5, 0xA, 2, 0xE, 0xD, 0xC, 0xD, 0xC, 8, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0xA, 0, 0, 0xA, 0, 0, 0 };
	static const unsigned char ampl3data[] = { 0, 0, 0, 0, 0, 8, 7, 8, 8, 1, 1, 0, 1, 0, 7, 5, 1, 0, 6, 1, 0, 7, 0, 5, 1, 0, 8, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0xE, 1, 9, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 5, 0, 0x13, 0x10 };
	static const unsigned char blendRank[80] = { 0, 0x1F, 0x1F, 0x1F, 0x1F, 2, 2, 2, 2, 2, 2, 2, 2, 2, 5, 5, 2, 0xA, 2, 8, 5, 5, 0xB, 0xA, 9, 8, 8, 0xA0, 8, 8, 0x17, 0x1F, 0x12, 0x12, 0x12, 0x12, 0x1E, 0x1E, 0x14, 0x14, 0x14, 0x14, 0x17, 0x17, 0x1A, 0x1A, 0x1D, 0x1D, 2, 2, 2, 2, 2, 2, 0x1A, 0x1D, 0x1B, 0x1A, 0x1D, 0x1B, 0x1A, 0x1D, 0x1B, 0x1A, 0x1D, 0x1B, 0x17, 0x1D, 0x17, 0x17, 0x1D, 0x17, 0x17, 0x1D, 0x17, 0x17, 0x1D, 0x17, 0x17, 0x17 };
	static const unsigned char outBlendLength[80] = { 0, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 2, 4, 4, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 0, 1, 0, 1, 0, 5, 5, 5, 5, 5, 4, 4, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 2, 2, 0, 1, 3, 0, 2, 3, 0, 2, 0xA0, 0xA0 };
	static const unsigned char inBlendLength[80] = { 0, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 4, 4, 3, 3, 3, 3, 3, 1, 2, 3, 2, 1, 3, 3, 3, 3, 1, 1, 3, 3, 3, 2, 2, 3, 2, 3, 0, 0, 5, 5, 5, 5, 4, 4, 2, 0, 2, 2, 0, 3, 2, 0, 4, 2, 0, 3, 2, 0, 2, 2, 0, 2, 3, 0, 3, 3, 0, 3, 0xB0, 0xA0 };

	unsigned char *mouthFreqData1 = ts->mouthFreqData, *mouthFreqData2 = ts->mouthFreqData + 80;

	while (ts->framesRendered == ts->framesTransitioned)
	{
		struct _tinysam__phoneme phoneme;
		if (ts->phonemesCursor != ts->phonemesCount) phoneme = ts->phonemes[ts->phonemesCursor];
		else phoneme.index = _TS_BREAK;

		if (phoneme.index != _TS_BREAK)
		{
			ts->phonemesCursor++;
			if (phoneme.index == 0) continue;

			enum { PHONEME_PERIOD = 1, PHONEME_QUESTION = 2, QUESTION_INFLECTION = 255, PERIOD_INFLECTION = 1 };
			if ((phoneme.index == PHONEME_PERIOD || phoneme.index == PHONEME_QUESTION) && ts->framesCount)
			{
				unsigned char inflection = (phoneme.index == PHONEME_PERIOD ? PERIOD_INFLECTION : QUESTION_INFLECTION);

				// Create a rising or falling inflection 30 frames prior to index X. A rising inflection is used for questions, and a falling inflection is used for statements.
				int end = ts->framesCount, pos = ts->framesCount - 30;
				if (pos < ts->framesRendered) pos = ts->framesRendered;

				// FIXME: Explain this fix better, it's not obvious
				// ML : A =, fixes a problem with invalid pitch with '.'
				unsigned char A;
				while ((A = ts->frames[pos].pitch) == 127) pos++;

				while (pos != end)
				{
					// add the inflection direction
					A += inflection;

					// set the inflection
					ts->frames[pos].pitch = A;

					while ((++pos != end) && ts->frames[pos].pitch == 255);
				}
			}
		
			// get the stress amount (more stress = higher pitch)
			unsigned char framePitch = ts->pitch + TINYSAM_SA(stressToPitch, phoneme.stress + 1);

			TINYSAM_DEBUGPRINT(("PHONEME [%2d] - CREATING FRAMES %2d ~ %2d\n", ts->phonemesCursor - 1, ts->framesCount, ts->framesCount + phoneme.length - 1));

			// get number of frames to write and copy from the source to the frames list
			for (int i = 0; i != phoneme.length; i++)
			{
				if (ts->framesCount + 1 > ts->framesReserve)
				{
					ts->frames = (struct _tinysam__frame*)TINYSAM_REALLOC(ts->frames, (ts->framesReserve += 16) * sizeof(*ts->frames));
				}

				struct _tinysam__frame* pFrame = &ts->frames[ts->framesCount++];
				pFrame->pitch      = framePitch;
				pFrame->frequency1 = TINYSAM_DA(mouthFreqData1, phoneme.index, 80);
				pFrame->frequency2 = TINYSAM_DA(mouthFreqData2, phoneme.index, 80);
				pFrame->frequency3 = TINYSAM_SA(freq3data, phoneme.index);
				pFrame->amplitude1 = TINYSAM_SA(ampl1data, phoneme.index);
				pFrame->amplitude2 = TINYSAM_SA(ampl2data, phoneme.index);
				pFrame->amplitude3 = TINYSAM_SA(ampl3data, phoneme.index);
				pFrame->consonant  = TINYSAM_SA(sampledConsonantFlags, phoneme.index);
			}
		}
		else if (ts->framesRendered == ts->framesCount)
		{
			// if terminal phoneme, exit the loop
			if (ts->phonemesCursor == ts->phonemesCount)
			{
				return 1;
			}
			ts->framesRendered = ts->framesCount = ts->framesTransitioned = 0;
			ts->phonemesCursor++;
			continue;
		}

		// CREATE TRANSITIONS
		//
		// Linear transitions are now created to smoothly connect each
		// phoeneme. This transition is spread between the ending frames
		// of the old phoneme (outBlendLength), and the beginning frames 
		// of the new phoneme (inBlendLength).
		//
		// To determine how many frames to use, the two phonemes are 
		// compared using the blendRank[] table. The phoneme with the 
		// smaller score is used. In case of a tie, a blend of each is used.
		// Blend lengths can't be less than zero.
		//
		// For most of the parameters, SAM interpolates over the range of the last
		// outBlendFrames-1 and the first inBlendFrames.
		//
		// The exception to this is the Pitch[] parameter, which is interpolates the
		// pitch from the center of the current phoneme to the center of the next
		// phoneme.
		int transitionsBehind = (phoneme.index == _TS_BREAK ? 0 : 31);
		for (int ft = ts->framesTransitioned; ft + transitionsBehind <= ts->framesCount && ts->phonemesTransitioned < ts->phonemesCursor && ts->phonemesTransitioned < ts->phonemesCount; ts->phonemesTransitioned++, ts->framesTransitioned = ft)
		{
			const struct _tinysam__phoneme prev_phoneme = ts->phonemes[ts->phonemesTransitioned];
			if (!prev_phoneme.index || prev_phoneme.index == _TS_BREAK)
			{
				TINYSAM_DEBUGPRINT(("PHONEME [%2d -> ?] - SKIPPING TRANSITION FROM THIS - T => %d (+%d)\n", ts->phonemesTransitioned, ts->framesTransitioned + prev_phoneme.length, prev_phoneme.length));
				continue;
			}
			ft += prev_phoneme.length;
			if (ft + transitionsBehind > ts->framesCount)
			{
				break;
			}
			TINYSAM_ASSERT(ft <= ts->framesCount);

			int next = ts->phonemesTransitioned;
			while (++next != ts->phonemesCount && ts->phonemes[next].index == 0);
			if (next == ts->phonemesCount || ts->phonemes[next].index == _TS_BREAK)
			{
				TINYSAM_DEBUGPRINT(("PHONEME [%2d -> ^%d] - SKIPPING TRANSITION FROM THIS - T => %d (+%d)\n", ts->phonemesTransitioned, next, ts->framesTransitioned + prev_phoneme.length, prev_phoneme.length));
				continue;
			}
			const struct _tinysam__phoneme next_phoneme = ts->phonemes[next];

			// half the width of the current and next phoneme
			const unsigned char cur_width  = prev_phoneme.length / 2;
			const unsigned char next_width = next_phoneme.length / 2;
			unsigned char pitchWidth = cur_width + next_width;

			// get the ranking of each phoneme
			// compare the rank - lower rank value is stronger
			int phase1, phase2;
			const unsigned char prev_rank = TINYSAM_SA(blendRank, prev_phoneme.index), next_rank = TINYSAM_SA(blendRank, next_phoneme.index);
			if (prev_rank == next_rank)
			{
				// same rank, so use out blend lengths from each phoneme
				phase1 = TINYSAM_SA(outBlendLength, prev_phoneme.index);
				phase2 = TINYSAM_SA(outBlendLength, next_phoneme.index);
			}
			else if (prev_rank < next_rank)
			{
				// next phoneme is stronger, so us its blend lengths
				phase1 = TINYSAM_SA(inBlendLength, next_phoneme.index);
				phase2 = TINYSAM_SA(outBlendLength, next_phoneme.index);
			}
			else
			{
				// current phoneme is stronger, so use its blend lengths note the out/in are swapped
				phase1 = TINYSAM_SA(outBlendLength, prev_phoneme.index);
				phase2 = TINYSAM_SA(inBlendLength, prev_phoneme.index);
			}
			int ampfreqWidth = phase1 + phase2; // total transition?
			if (ampfreqWidth <= 1)
			{
				TINYSAM_DEBUGPRINT(("PHONEME [%2d -> %2d] - NO TRANSITION FRAMES (%2d ~ %2d) - T => %d\n", ts->phonemesTransitioned, next, ft - phase1 + 1, (ft - phase1 + (pitchWidth > ampfreqWidth ? pitchWidth : ampfreqWidth) - 1), ft));
				continue;
			}

			int transitionStart = ft - phase1, ampfreqTo = ft + phase2, pitchFrom = ft - cur_width, pitchTo = ft + next_width;

			int transitionEnd = transitionStart + (pitchWidth > ampfreqWidth ? pitchWidth : ampfreqWidth);
			if (transitionsBehind == 0 && transitionEnd > ts->framesCount)
			{
				//weird edge case with a transititon at the end that goes past the last frame
				transitionEnd = ts->framesCount;
			}
			if (transitionEnd + transitionsBehind > ts->framesCount) break;
			if (pitchTo   >= ts->framesCount) pitchTo   = ts->framesCount - 1;
			if (ampfreqTo >= ts->framesCount) ampfreqTo = ts->framesCount - 1;

			TINYSAM_DEBUGPRINT(("PHONEME [%2d -> %2d] - TRANSITIONING FRAMES %2d ~ %2d - T => %d\n", ts->phonemesTransitioned, next, transitionStart + 1, (transitionStart + (pitchWidth > ampfreqWidth ? pitchWidth : ampfreqWidth) - 1), ft));

			// interpolate pitch, unlike the other values, the pitches interpolates from the middle of the current phoneme to the middle of the next phoneme
			for (int tableNum = 0; tableNum != 7; tableNum++)
			{
				#define _TS_TBLVAL(n) (((unsigned char*)&ts->frames[TINYSAM_ASSERT((n) < ts->framesCount),(n)])[tableNum])
				int tableVal = (tableNum == 0 ? (_TS_TBLVAL(pitchTo) - _TS_TBLVAL(pitchFrom)) : (_TS_TBLVAL(ampfreqTo) - _TS_TBLVAL(transitionStart)));
				int tableWidth = (tableNum == 0 ? pitchWidth : ampfreqWidth);
				int sign = (tableVal < 0), remainder = (sign ? -tableVal : tableVal) % tableWidth, div = tableVal / tableWidth;
				int ipos = transitionStart + 1, iposend = transitionStart + tableWidth;
				if (iposend > ts->framesCount) iposend = ts->framesCount;

				// linearly interpolate values
				for (int ival = _TS_TBLVAL(transitionStart) + div, error = 0; ipos != iposend; ipos++, ival += div)
				{
					error += remainder;
					if (error >= tableWidth)
					{
						// accumulated a whole integer error, so adjust output
						error -= tableWidth;
						if (sign) ival--;
						else if (ival) ival++; // if input is 0, we always leave it alone
					}
					TINYSAM_ASSERT(ipos >= ft - prev_phoneme.length);
					_TS_TBLVAL(ipos) = ival; // Write updated value back to next frame.
				}
				#undef _TS_TBLVAL
			}
		}
	}
	return 0;
}

#undef _TS_BREAK

static void* _tinysam__writebytes(unsigned char* p, int count, unsigned char val, struct tinysam* ts)
{
	unsigned char v = (unsigned char)((val - 0x80) * ts->globalVolume) + 0x80, *p2;
	switch (ts->outputmode)
	{
		#define _TS_MIX(ptr) { int m = *(ptr) + v - 0x80; *((ptr)++) = (m < 0 ? 0 : (m > 0xFF ? 0xFF : m)); }
		case     TINYSAM_STEREO_INTERLEAVED:                          do {  *(p++) = v; *(p++) = v; } while (--count); break;
		case     TINYSAM_STEREO_UNWEAVED: p2 = p + ts->renderSamples; do { *(p2++) = v; *(p++) = v; } while (--count); break;
		case     TINYSAM_MONO:                                        do {              *(p++) = v; } while (--count); break;
		case 0x4|TINYSAM_STEREO_INTERLEAVED:                          do { _TS_MIX(p);  _TS_MIX(p); } while (--count); break;
		case 0x4|TINYSAM_STEREO_UNWEAVED: p2 = p + ts->renderSamples; do { _TS_MIX(p2); _TS_MIX(p); } while (--count); break;
		case 0x4|TINYSAM_MONO:                                        do {              _TS_MIX(p); } while (--count); break;
		#undef _TS_MIX
	}
	return p;
}

static void* _tinysam__writeshorts(short* p, int count, unsigned char val, struct tinysam* ts)
{
	short v = (short)((val < 0x80 ? ((val - 0x80) * 256.00703125f) : ((val - 0x80) * 258.01496062992125984251968503937f)) * ts->globalVolume), *p2;
	switch (ts->outputmode)
	{
		#define _TS_MIX(ptr) { int m = *(ptr) + v; *((ptr)++) = (m < -0x8000 ? -0x8000 : (m > 0x7FFF ? 0x7FFF : m)); }
		case     TINYSAM_STEREO_INTERLEAVED:                          do {  *(p++) = v; *(p++) = v; } while (--count); break;
		case     TINYSAM_STEREO_UNWEAVED: p2 = p + ts->renderSamples; do { *(p2++) = v; *(p++) = v; } while (--count); break;
		case     TINYSAM_MONO:                                        do {              *(p++) = v; } while (--count); break;
		case 0x4|TINYSAM_STEREO_INTERLEAVED:                          do { _TS_MIX(p);  _TS_MIX(p); } while (--count); break;
		case 0x4|TINYSAM_STEREO_UNWEAVED: p2 = p + ts->renderSamples; do { _TS_MIX(p2); _TS_MIX(p); } while (--count); break;
		case 0x4|TINYSAM_MONO:                                        do {              _TS_MIX(p); } while (--count); break;
		#undef _TS_MIX
	}
	return p;
}

static void* _tinysam__writefloats(float* p, int count, unsigned char val, struct tinysam* ts)
{
	float v = ((val < 0x80 ? ((val - 0x80) * 0.0078125f) : ((val - 0x80) * 0.007874015748031496062992125984252f)) * ts->globalVolume), *p2;
	switch (ts->outputmode)
	{
		#define _TS_MIX(ptr) { float m = *(ptr) + v; *((ptr)++) = (m < -1.f ? -1.f : (m > 1.f ? 1.f : m)); }
		case     TINYSAM_STEREO_INTERLEAVED:                          do {  *(p++) = v; *(p++) = v; } while (--count); break;
		case     TINYSAM_STEREO_UNWEAVED: p2 = p + ts->renderSamples; do { *(p2++) = v; *(p++) = v; } while (--count); break;
		case     TINYSAM_MONO:                                        do {              *(p++) = v; } while (--count); break;
		case 0x4|TINYSAM_STEREO_INTERLEAVED:                          do { _TS_MIX(p);  _TS_MIX(p); } while (--count); break;
		case 0x4|TINYSAM_STEREO_UNWEAVED: p2 = p + ts->renderSamples; do { _TS_MIX(p2); _TS_MIX(p); } while (--count); break;
		case 0x4|TINYSAM_MONO:                                        do {              _TS_MIX(p); } while (--count); break;
		#undef _TS_MIX
	}
	return p;
}

static int _tinysam__writesamples(struct _tinysam__output* pOutput, unsigned short* historyStart, unsigned short** historyCursor, tinysam* ts)
{
	#ifdef TINYSAM_SAM_COMPATIBILITY
	enum { _TS_TRUNCATE = 0xF0 };
	#else
	enum { _TS_TRUNCATE = 0xFF };
	#endif
	struct _tinysam__output output = *pOutput;
	int old50 = output.bufferPos / 50, new50 = old50;
	for (unsigned short *p = historyStart, *pEnd = (*historyCursor)-1; p != pEnd; p++, old50 = new50)
	{
		new50 = (output.bufferPos += ts->timetable[*p>>8][p[1]>>8]) / 50;
		int count = (new50 - old50);
		if (count == 0) continue;
		//printf("RENDERING SAMPLES %d to %d (flow: %d)\n", old50, new50, output.flow);
		if (count > output.flow)
		{
			if (output.flow < 0)
			{
				// before buffer start, flow will be -(count already written of this frame)
				output.flow += count;
				if (output.flow < 0) continue;
				count = output.flow;
				output.flow = ts->renderSamples;
				if (count >= output.flow)
				{
					// Very special case when requested samples are too small to even hold count once
					ts->lastFrameRenderedUntil += ts->renderSamples;
					goto WRITE_TILL_END;
				}
				if (count == 0) continue;
			}
			else
			{
				// about to fill beyond buffer, set lastRenderedUntil to (count already written of this frame)
				ts->lastFrameRenderedUntil = (old50 - ts->lastFramebufferPos / 50 + output.flow);
				WRITE_TILL_END:
				if (output.flow) output.buffer = output.writefunc(output.buffer, output.flow, (*p & _TS_TRUNCATE), ts);
				*pOutput = output;
				return 1;
			}
		}
		output.buffer = output.writefunc(output.buffer, count, (*p & _TS_TRUNCATE), ts);
		output.flow -= count;
	}
	historyStart[0] = (*historyCursor)[-1];
	*historyCursor = historyStart + 1;
	*pOutput = output;
	return 0;
}

static int _tinysam__render(tinysam* ts, void* bufferStart, int samples, int mix, int widthShift, int zero, void*(*writefunc)(void*, int, unsigned char, struct tinysam*))
{
	// PROCESS THE FRAMES
	//
	// In traditional vocal synthesis, the glottal pulse drives filters, which
	// are attenuated to the frequencies of the formants.
	//
	// SAM generates these formants directly with sin and rectangular waves.
	// To simulate them being driven by the glottal pulse, the waveforms are
	// reset at the beginning of each glottal pulse.
	//
	//Code48227
	// Render a sampled sound from the sampleTable.
	//
	//   Phoneme   Sample Start   Sample End
	//   32: S*    15             255
	//   33: SH    257            511
	//   34: F*    559            767
	//   35: TH    583            767
	//   36: /H    903            1023
	//   37: /X    1135           1279
	//   38: Z*    84             119
	//   39: ZH    340            375
	//   40: V*    596            639
	//   41: DH    596            631
	//
	//   42: CH
	//   43: **    399            511
	//
	//   44: J*
	//   45: **    257            276
	//   46: **
	// 
	//   66: P*
	//   67: **    743            767
	//   68: **
	//
	//   69: T*
	//   70: **    231            255
	//   71: **
	//
	// The SampledPhonemesTable[] holds flags indicating if a phoneme is
	// voiced or not. If the upper 5 bits are zero, the sample is voiced.
	//
	// Samples in the sampleTable are compressed, with bits being converted to
	// bytes from high bit to low, as follows:
	//
	//   unvoiced 0 bit   -> X
	//   unvoiced 1 bit   -> 5
	//
	//   voiced 0 bit     -> 6
	//   voiced 1 bit     -> 24
	//
	// Where X is a value from the table:
	//
	//   { 0x18, 0x1A, 0x17, 0x17, 0x17 };
	//
	// The index into this table is determined by masking off the lower
	// 3 bits from the SampledPhonemesTable:
	//
	//        index = (SampledPhonemesTable[i] & 7) - 1;
	//
	// For voices samples, samples are interleaved between voiced output.
	static const unsigned char sinus[]     = { 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xB0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00 };
	static const unsigned char rectangle[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70 };
	static const unsigned char multtable[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06, 0x06, 0x07, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x03, 0x04, 0x06, 0x07, 0x09, 0x0A, 0x0C, 0x0D, 0x0F, 0x10, 0x12, 0x13, 0x15, 0x16, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E, 0x00, 0x02, 0x05, 0x07, 0x0A, 0x0C, 0x0F, 0x11, 0x14, 0x16, 0x19, 0x1B, 0x1E, 0x20, 0x23, 0x25, 0x00, 0x03, 0x06, 0x09, 0x0C, 0x0F, 0x12, 0x15, 0x18, 0x1B, 0x1E, 0x21, 0x24, 0x27, 0x2A, 0x2D, 0x00, 0x03, 0x07, 0x0A, 0x0E, 0x11, 0x15, 0x18, 0x1C, 0x1F, 0x23, 0x26, 0x2A, 0x2D, 0x31, 0x34, 0x00, 0xFC, 0xF8, 0xF4, 0xF0, 0xEC, 0xE8, 0xE4, 0xE0, 0xDC, 0xD8, 0xD4, 0xD0, 0xCC, 0xC8, 0xC4, 0x00, 0xFC, 0xF9, 0xF5, 0xF2, 0xEE, 0xEB, 0xE7, 0xE4, 0xE0, 0xDD, 0xD9, 0xD6, 0xD2, 0xCF, 0xCB, 0x00, 0xFD, 0xFA, 0xF7, 0xF4, 0xF1, 0xEE, 0xEB, 0xE8, 0xE5, 0xE2, 0xDF, 0xDC, 0xD9, 0xD6, 0xD3, 0x00, 0xFD, 0xFB, 0xF8, 0xF6, 0xF3, 0xF1, 0xEE, 0xEC, 0xE9, 0xE7, 0xE4, 0xE2, 0xDF, 0xDD, 0xDA, 0x00, 0xFE, 0xFC, 0xFA, 0xF8, 0xF6, 0xF4, 0xF2, 0xF0, 0xEE, 0xEC, 0xEA, 0xE8, 0xE6, 0xE4, 0xE2, 0x00, 0xFE, 0xFD, 0xFB, 0xFA, 0xF8, 0xF7, 0xF5, 0xF4, 0xF2, 0xF1, 0xEF, 0xEE, 0xEC, 0xEB, 0xE9, 0x00, 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0x00, 0xFF, 0xFF, 0xFE, 0xFE, 0xFD, 0xFD, 0xFC, 0xFC, 0xFB, 0xFB, 0xFA, 0xFA, 0xF9, 0xF9, 0xF8 };

	// Rescale volume from a linear scale to decibels.
	static const unsigned char amplitudeRescale[] = { 0, 1, 2, 2, 2, 3, 3, 4,  4, 5, 6, 8, 9, 0xB, 0xD, 0xF, 0 }; //17elements?

	//random data ?
	enum { SAMPLE_TABLE_SIZE = 0x500 };
	static const unsigned char sampleTable[SAMPLE_TABLE_SIZE] = {
		0x38, 0x84, 0x6B, 0x19, 0xC6, 0x63, 0x18, 0x86, 0x73, 0x98, 0xC6, 0xB1, 0x1C, 0xCA, 0x31, 0x8C, 0xC7, 0x31, 0x88, 0xC2, 0x30, 0x98, 0x46, 0x31, 0x18, 0xC6, 0x35, 0xC, 0xCA, 0x31, 0xC, 0xC6,
		0x21, 0x10, 0x24, 0x69, 0x12, 0xC2, 0x31, 0x14, 0xC4, 0x71, 8, 0x4A, 0x22, 0x49, 0xAB, 0x6A, 0xA8, 0xAC, 0x49, 0x51, 0x32, 0xD5, 0x52, 0x88, 0x93, 0x6C, 0x94, 0x22, 0x15, 0x54, 0xD2, 0x25,
		0x96, 0xD4, 0x50, 0xA5, 0x46, 0x21, 8, 0x85, 0x6B, 0x18, 0xC4, 0x63, 0x10, 0xCE, 0x6B, 0x18, 0x8C, 0x71, 0x19, 0x8C, 0x63, 0x35, 0xC, 0xC6, 0x33, 0x99, 0xCC, 0x6C, 0xB5, 0x4E, 0xA2, 0x99,
		0x46, 0x21, 0x28, 0x82, 0x95, 0x2E, 0xE3, 0x30, 0x9C, 0xC5, 0x30, 0x9C, 0xA2, 0xB1, 0x9C, 0x67, 0x31, 0x88, 0x66, 0x59, 0x2C, 0x53, 0x18, 0x84, 0x67, 0x50, 0xCA, 0xE3, 0xA, 0xAC, 0xAB, 0x30,
		0xAC, 0x62, 0x30, 0x8C, 0x63, 0x10, 0x94, 0x62, 0xB1, 0x8C, 0x82, 0x28, 0x96, 0x33, 0x98, 0xD6, 0xB5, 0x4C, 0x62, 0x29, 0xA5, 0x4A, 0xB5, 0x9C, 0xC6, 0x31, 0x14, 0xD6, 0x38, 0x9C, 0x4B, 0xB4,
		0x86, 0x65, 0x18, 0xAE, 0x67, 0x1C, 0xA6, 0x63, 0x19, 0x96, 0x23, 0x19, 0x84, 0x13, 8, 0xA6, 0x52, 0xAC, 0xCA, 0x22, 0x89, 0x6E, 0xAB, 0x19, 0x8C, 0x62, 0x34, 0xC4, 0x62, 0x19, 0x86, 0x63,
		0x18, 0xC4, 0x23, 0x58, 0xD6, 0xA3, 0x50, 0x42, 0x54, 0x4A, 0xAD, 0x4A, 0x25, 0x11, 0x6B, 0x64, 0x89, 0x4A, 0x63, 0x39, 0x8A, 0x23, 0x31, 0x2A, 0xEA, 0xA2, 0xA9, 0x44, 0xC5, 0x12, 0xCD, 0x42,
		0x34, 0x8C, 0x62, 0x18, 0x8C, 0x63, 0x11, 0x48, 0x66, 0x31, 0x9D, 0x44, 0x33, 0x1D, 0x46, 0x31, 0x9C, 0xC6, 0xB1, 0xC, 0xCD, 0x32, 0x88, 0xC4, 0x73, 0x18, 0x86, 0x73, 8, 0xD6, 0x63, 0x58,
		7, 0x81, 0xE0, 0xF0, 0x3C, 7, 0x87, 0x90, 0x3C, 0x7C, 0xF, 0xC7, 0xC0, 0xC0, 0xF0, 0x7C, 0x1E, 7, 0x80, 0x80, 0, 0x1C, 0x78, 0x70, 0xF1, 0xC7, 0x1F, 0xC0, 0xC, 0xFE, 0x1C, 0x1F,
		0x1F, 0xE, 0xA, 0x7A, 0xC0, 0x71, 0xF2, 0x83, 0x8F, 3, 0xF, 0xF, 0xC, 0, 0x79, 0xF8, 0x61, 0xE0, 0x43, 0xF, 0x83, 0xE7, 0x18, 0xF9, 0xC1, 0x13, 0xDA, 0xE9, 0x63, 0x8F, 0xF, 0x83,
		0x83, 0x87, 0xC3, 0x1F, 0x3C, 0x70, 0xF0, 0xE1, 0xE1, 0xE3, 0x87, 0xB8, 0x71, 0xE, 0x20, 0xE3, 0x8D, 0x48, 0x78, 0x1C, 0x93, 0x87, 0x30, 0xE1, 0xC1, 0xC1, 0xE4, 0x78, 0x21, 0x83, 0x83, 0xC3,
		0x87, 6, 0x39, 0xE5, 0xC3, 0x87, 7, 0xE, 0x1C, 0x1C, 0x70, 0xF4, 0x71, 0x9C, 0x60, 0x36, 0x32, 0xC3, 0x1E, 0x3C, 0xF3, 0x8F, 0xE, 0x3C, 0x70, 0xE3, 0xC7, 0x8F, 0xF, 0xF, 0xE, 0x3C,
		0x78, 0xF0, 0xE3, 0x87, 6, 0xF0, 0xE3, 7, 0xC1, 0x99, 0x87, 0xF, 0x18, 0x78, 0x70, 0x70, 0xFC, 0xF3, 0x10, 0xB1, 0x8C, 0x8C, 0x31, 0x7C, 0x70, 0xE1, 0x86, 0x3C, 0x64, 0x6C, 0xB0, 0xE1,
		0xE3, 0xF, 0x23, 0x8F, 0xF, 0x1E, 0x3E, 0x38, 0x3C, 0x38, 0x7B, 0x8F, 7, 0xE, 0x3C, 0xF4, 0x17, 0x1E, 0x3C, 0x78, 0xF2, 0x9E, 0x72, 0x49, 0xE3, 0x25, 0x36, 0x38, 0x58, 0x39, 0xE2, 0xDE,
		0x3C, 0x78, 0x78, 0xE1, 0xC7, 0x61, 0xE1, 0xE1, 0xB0, 0xF0, 0xF0, 0xC3, 0xC7, 0xE, 0x38, 0xC0, 0xF0, 0xCE, 0x73, 0x73, 0x18, 0x34, 0xB0, 0xE1, 0xC7, 0x8E, 0x1C, 0x3C, 0xF8, 0x38, 0xF0, 0xE1,
		0xC1, 0x8B, 0x86, 0x8F, 0x1C, 0x78, 0x70, 0xF0, 0x78, 0xAC, 0xB1, 0x8F, 0x39, 0x31, 0xDB, 0x38, 0x61, 0xC3, 0xE, 0xE, 0x38, 0x78, 0x73, 0x17, 0x1E, 0x39, 0x1E, 0x38, 0x64, 0xE1, 0xF1, 0xC1,
		0x4E, 0xF, 0x40, 0xA2, 2, 0xC5, 0x8F, 0x81, 0xA1, 0xFC, 0x12, 8, 0x64, 0xE0, 0x3C, 0x22, 0xE0, 0x45, 7, 0x8E, 0xC, 0x32, 0x90, 0xF0, 0x1F, 0x20, 0x49, 0xE0, 0xF8, 0xC, 0x60, 0xF0,
		0x17, 0x1A, 0x41, 0xAA, 0xA4, 0xD0, 0x8D, 0x12, 0x82, 0x1E, 0x1E, 3, 0xF8, 0x3E, 3, 0xC, 0x73, 0x80, 0x70, 0x44, 0x26, 3, 0x24, 0xE1, 0x3E, 4, 0x4E, 4, 0x1C, 0xC1, 9, 0xCC,
		0x9E, 0x90, 0x21, 7, 0x90, 0x43, 0x64, 0xC0, 0xF, 0xC6, 0x90, 0x9C, 0xC1, 0x5B, 3, 0xE2, 0x1D, 0x81, 0xE0, 0x5E, 0x1D, 3, 0x84, 0xB8, 0x2C, 0xF, 0x80, 0xB1, 0x83, 0xE0, 0x30, 0x41,
		0x1E, 0x43, 0x89, 0x83, 0x50, 0xFC, 0x24, 0x2E, 0x13, 0x83, 0xF1, 0x7C, 0x4C, 0x2C, 0xC9, 0xD, 0x83, 0xB0, 0xB5, 0x82, 0xE4, 0xE8, 6, 0x9C, 7, 0xA0, 0x99, 0x1D, 7, 0x3E, 0x82, 0x8F,
		0x70, 0x30, 0x74, 0x40, 0xCA, 0x10, 0xE4, 0xE8, 0xF, 0x92, 0x14, 0x3F, 6, 0xF8, 0x84, 0x88, 0x43, 0x81, 0xA, 0x34, 0x39, 0x41, 0xC6, 0xE3, 0x1C, 0x47, 3, 0xB0, 0xB8, 0x13, 0xA, 0xC2,
		0x64, 0xF8, 0x18, 0xF9, 0x60, 0xB3, 0xC0, 0x65, 0x20, 0x60, 0xA6, 0x8C, 0xC3, 0x81, 0x20, 0x30, 0x26, 0x1E, 0x1C, 0x38, 0xD3, 1, 0xB0, 0x26, 0x40, 0xF4, 0xB, 0xC3, 0x42, 0x1F, 0x85, 0x32,
		0x26, 0x60, 0x40, 0xC9, 0xCB, 1, 0xEC, 0x11, 0x28, 0x40, 0xFA, 4, 0x34, 0xE0, 0x70, 0x4C, 0x8C, 0x1D, 7, 0x69, 3, 0x16, 0xC8, 4, 0x23, 0xE8, 0xC6, 0x9A, 0xB, 0x1A, 3, 0xE0,
		0x76, 6, 5, 0xCF, 0x1E, 0xBC, 0x58, 0x31, 0x71, 0x66, 0, 0xF8, 0x3F, 4, 0xFC, 0xC, 0x74, 0x27, 0x8A, 0x80, 0x71, 0xC2, 0x3A, 0x26, 6, 0xC0, 0x1F, 5, 0xF, 0x98, 0x40, 0xAE,
		1, 0x7F, 0xC0, 7, 0xFF, 0, 0xE, 0xFE, 0, 3, 0xDF, 0x80, 3, 0xEF, 0x80, 0x1B, 0xF1, 0xC2, 0, 0xE7, 0xE0, 0x18, 0xFC, 0xE0, 0x21, 0xFC, 0x80, 0x3C, 0xFC, 0x40, 0xE, 0x7E,
		0, 0x3F, 0x3E, 0, 0xF, 0xFE, 0, 0x1F, 0xFF, 0, 0x3E, 0xF0, 7, 0xFC, 0, 0x7E, 0x10, 0x3F, 0xFF, 0, 0x3F, 0x38, 0xE, 0x7C, 1, 0x87, 0xC, 0xFC, 0xC7, 0, 0x3E, 4,
		0xF, 0x3E, 0x1F, 0xF, 0xF, 0x1F, 0xF, 2, 0x83, 0x87, 0xCF, 3, 0x87, 0xF, 0x3F, 0xC0, 7, 0x9E, 0x60, 0x3F, 0xC0, 3, 0xFE, 0, 0x3F, 0xE0, 0x77, 0xE1, 0xC0, 0xFE, 0xE0, 0xC3,
		0xE0, 1, 0xDF, 0xF8, 3, 7, 0, 0x7E, 0x70, 0, 0x7C, 0x38, 0x18, 0xFE, 0xC, 0x1E, 0x78, 0x1C, 0x7C, 0x3E, 0xE, 0x1F, 0x1E, 0x1E, 0x3E, 0, 0x7F, 0x83, 7, 0xDB, 0x87, 0x83,
		7, 0xC7, 7, 0x10, 0x71, 0xFF, 0, 0x3F, 0xE2, 1, 0xE0, 0xC1, 0xC3, 0xE1, 0, 0x7F, 0xC0, 5, 0xF0, 0x20, 0xF8, 0xF0, 0x70, 0xFE, 0x78, 0x79, 0xF8, 2, 0x3F, 0xC, 0x8F, 3,
		0xF, 0x9F, 0xE0, 0xC1, 0xC7, 0x87, 3, 0xC3, 0xC3, 0xB0, 0xE1, 0xE1, 0xC1, 0xE3, 0xE0, 0x71, 0xF0, 0, 0xFC, 0x70, 0x7C, 0xC, 0x3E, 0x38, 0xE, 0x1C, 0x70, 0xC3, 0xC7, 3, 0x81, 0xC1,
		0xC7, 0xE7, 0, 0xF, 0xC7, 0x87, 0x19, 9, 0xEF, 0xC4, 0x33, 0xE0, 0xC1, 0xFC, 0xF8, 0x70, 0xF0, 0x78, 0xF8, 0xF0, 0x61, 0xC7, 0, 0x1F, 0xF8, 1, 0x7C, 0xF8, 0xF0, 0x78, 0x70, 0x3C,
		0x7C, 0xCE, 0xE, 0x21, 0x83, 0xCF, 8, 7, 0x8F, 8, 0xC1, 0x87, 0x8F, 0x80, 0xC7, 0xE3, 0, 7, 0xF8, 0xE0, 0xEF, 0, 0x39, 0xF7, 0x80, 0xE, 0xF8, 0xE1, 0xE3, 0xF8, 0x21, 0x9F,
		0xC0, 0xFF, 3, 0xF8, 7, 0xC0, 0x1F, 0xF8, 0xC4, 4, 0xFC, 0xC4, 0xC1, 0xBC, 0x87, 0xF0, 0xF, 0xC0, 0x7F, 5, 0xE0, 0x25, 0xEC, 0xC0, 0x3E, 0x84, 0x47, 0xF0, 0x8E, 3, 0xF8, 3,
		0xFB, 0xC0, 0x19, 0xF8, 7, 0x9C, 0xC, 0x17, 0xF8, 7, 0xE0, 0x1F, 0xA1, 0xFC, 0xF, 0xFC, 1, 0xF0, 0x3F, 0, 0xFE, 3, 0xF0, 0x1F, 0, 0xFD, 0, 0xFF, 0x88, 0xD, 0xF9, 1,
		0xFF, 0, 0x70, 7, 0xC0, 0x3E, 0x42, 0xF3, 0xD, 0xC4, 0x7F, 0x80, 0xFC, 7, 0xF0, 0x5E, 0xC0, 0x3F, 0, 0x78, 0x3F, 0x81, 0xFF, 1, 0xF8, 1, 0xC3, 0xE8, 0xC, 0xE4, 0x64, 0x8F,
		0xE4, 0xF, 0xF0, 7, 0xF0, 0xC2, 0x1F, 0, 0x7F, 0xC0, 0x6F, 0x80, 0x7E, 3, 0xF8, 7, 0xF0, 0x3F, 0xC0, 0x78, 0xF, 0x82, 7, 0xFE, 0x22, 0x77, 0x70, 2, 0x76, 3, 0xFE, 0,
		0xFE, 0x67, 0, 0x7C, 0xC7, 0xF1, 0x8E, 0xC6, 0x3B, 0xE0, 0x3F, 0x84, 0xF3, 0x19, 0xD8, 3, 0x99, 0xFC, 9, 0xB8, 0xF, 0xF8, 0, 0x9D, 0x24, 0x61, 0xF9, 0xD, 0, 0xFD, 3, 0xF0,
		0x1F, 0x90, 0x3F, 1, 0xF8, 0x1F, 0xD0, 0xF, 0xF8, 0x37, 1, 0xF8, 7, 0xF0, 0xF, 0xC0, 0x3F, 0, 0xFE, 3, 0xF8, 0xF, 0xC0, 0x3F, 0, 0xFA, 3, 0xF0, 0xF, 0x80, 0xFF, 1,
		0xB8, 7, 0xF0, 1, 0xFC, 1, 0xBC, 0x80, 0x13, 0x1E, 0, 0x7F, 0xE1, 0x40, 0x7F, 0xA0, 0x7F, 0xB0, 0, 0x3F, 0xC0, 0x1F, 0xC0, 0x38, 0xF, 0xF0, 0x1F, 0x80, 0xFF, 1, 0xFC, 3,
		0xF1, 0x7E, 1, 0xFE, 1, 0xF0, 0xFF, 0, 0x7F, 0xC0, 0x1D, 7, 0xF0, 0xF, 0xC0, 0x7E, 6, 0xE0, 7, 0xE0, 0xF, 0xF8, 6, 0xC1, 0xFE, 1, 0xFC, 3, 0xE0, 0xF, 0, 0xFC,
	};

	
	struct _tinysam__output output;
	struct _tinysam__renderstate rs;
	unsigned short history[512], *pHistory, *pHistoryFlush;

	output.buffer    = bufferStart;
	if (!ts->phonemesCount) goto FINISH_RENDER;
	output.writefunc = writefunc;
	output.bufferPos = ts->lastFramebufferPos;
	output.flow      = (ts->lastFrameRenderedUntil ? -ts->lastFrameRenderedUntil : samples);

	pHistory = history+1;
	pHistoryFlush = history + 200;
	rs = ts->lastFrameRenderState;
	#ifdef TINYSAM_SAM_COMPATIBILITY
	enum { _TS_ZERO_OUTPUT_VAL = 0x00 };
	#else
	enum { _TS_ZERO_OUTPUT_VAL = 0x80 };
	#endif
	history[0] = (ts->lastFramebufferPos ? ts->lastFrameFirstHistory : _TS_ZERO_OUTPUT_VAL);

	if (mix) ts->outputmode |= 0x4;
	ts->renderSamples = samples;

	int speedcounter;
	if (rs.glottalPulse) { speedcounter = 0; goto RESUME_GLOTTAL_PULSE; }

	for (;;)
	{
		#define _TS_OUTPUT(table, val) { TINYSAM_ASSERT(pHistory < history+512 && (val) >= 0 && (val) <= 255); *(pHistory++) = ((table)<<8)|(val); }
		struct _tinysam__frame frame;

		if (_tinysam__feedframes(ts) && ts->framesRendered == ts->framesCount)
		{
			#ifndef TINYSAM_SAM_COMPATIBILITY
			if (history[0] != _TS_ZERO_OUTPUT_VAL)
			{
				// add one more entry to write out the actual last value (not done by original prorgam)
				_TS_OUTPUT(0, _TS_ZERO_OUTPUT_VAL);
				if (_tinysam__writesamples(&output, history, &pHistory, ts)) goto FINISH_RENDER;
			}
			#endif
			// clear up we're finished for now until more phonemes are generated (reset memory usage without freeing)
			ts->phonemesCount = ts->phonemesCursor = ts->phonemesTransitioned = 0;
			ts->framesCount = ts->framesTransitioned = ts->framesRendered = 0;
			ts->lastFramebufferPos = 0;
			ts->lastFrameRenderedUntil = 0;
			TINYSAM_MEMSET(&ts->lastFrameRenderState, 0, sizeof(ts->lastFrameRenderState));
			goto FINISH_RENDER;
		}

		if (!rs.glottalPulse || !ts->framesRendered)
		{
			//TINYSAM_DEBUGPRINT(("[RENDER] Starting frame %2d (fed @ %2d)\n", ts->frameRendered, ts->frameFed));
			TINYSAM_ASSERT(ts->framesRendered < ts->framesTransitioned);
			frame = ts->frames[ts->framesRendered];
			if (!ts->singmode) frame.pitch -= (frame.frequency1 >> 1);
			frame.amplitude1 = TINYSAM_SA(amplitudeRescale, frame.amplitude1);
			frame.amplitude2 = TINYSAM_SA(amplitudeRescale, frame.amplitude2);
			frame.amplitude3 = TINYSAM_SA(amplitudeRescale, frame.amplitude3);

			rs.glottalPulse = frame.pitch;
			rs.glottalQuick = rs.glottalPulse - (rs.glottalPulse >> 2); // rs.glottalPulse * 0.75
			rs.phase1 = rs.phase2 = rs.phase3 = 0; // reset the formant wave generators to keep them in sync with the glottal pulse
			if (!ts->framesRendered) rs.glottalOff = 0;
		}

		rs.consonant = frame.consonant;
		if (rs.consonant & 248) // unvoiced sampled phoneme?
		{
			// mask low three bits and subtract 1 get value to convert 0 bits on unvoiced samples.
			unsigned char hibyte = (rs.consonant & 7)-1;
			unsigned short hi = hibyte*256;

			// determine which offset to use from table { 0x18, 0x1A, 0x17, 0x17, 0x17 }
			// T, S, Z                0          0x18
			// CH, J, SH, ZH          1          0x1A
			// P, F*, V, TH, DH       2          0x17
			// /H                     3          0x17
			// /X                     4          0x17
			//static const unsigned char tab48426[5] = { 0x18, 0x1A, 0x17, 0x17, 0x17 };
			static const unsigned char tab48426[5] = { 0x80, 0xA0, 0x70, 0x70, 0x70 };
			unsigned char mem53 = TINYSAM_SA(tab48426, hibyte);
			unsigned char pitchl = rs.consonant & 248;
			for (pitchl ^= 255; pitchl; pitchl++)
			{
				for (unsigned char bit = 0, sample = TINYSAM_SA(sampleTable, hi + pitchl); bit != 8; bit++, sample <<= 1)
				{
					if ((sample & 128) != 0) _TS_OUTPUT(2, 0x50)
					else                     _TS_OUTPUT(1, mem53)
				}
				if (pHistory > pHistoryFlush && _tinysam__writesamples(&output, history, &pHistory, ts)) goto FINISH_RENDER;
			}

			// skip an entry of the phoneme buffer
			if (_tinysam__writesamples(&output, history, &pHistory, ts)) goto FINISH_RENDER;
			rs.glottalPulse = 0;
			ts->framesRendered += 2;
			TINYSAM_ASSERT(ts->framesRendered <= ts->framesCount);
			ts->lastFramebufferPos = output.bufferPos;
			ts->lastFrameFirstHistory = history[0];
			ts->lastFrameRenderState = rs;
			continue;
		}

		for (speedcounter = ts->speed; speedcounter;)
		{
			//Combine glottal and formants
			unsigned int tmp;
			tmp  = TINYSAM_SA(multtable, TINYSAM_SA(sinus, rs.phase1)     | frame.amplitude1);
			tmp += TINYSAM_SA(multtable, TINYSAM_SA(sinus, rs.phase2)     | frame.amplitude2);
			tmp += tmp > 255 ? 1 : 0; // if addition above overflows, we for some reason add one;
			tmp += TINYSAM_SA(multtable, TINYSAM_SA(rectangle, rs.phase3) | frame.amplitude3);
			tmp += 136;
			_TS_OUTPUT(0, tmp & 0xff)

			if (--speedcounter == 0)
			{
				//go to next amplitude
				if (_tinysam__writesamples(&output, history, &pHistory, ts)) goto FINISH_RENDER;
				ts->framesRendered += 1;
				TINYSAM_ASSERT(ts->framesRendered <= ts->framesCount);
				ts->lastFramebufferPos = output.bufferPos;
				ts->lastFrameFirstHistory = history[0];
				ts->lastFrameRenderState = rs;
				RESUME_GLOTTAL_PULSE:
				//TINYSAM_DEBUGPRINT(("[RENDER] Starting frame %2d during glottal pulse (fed @ %2d)\n", ts->frameRendered, ts->frameFed));
				if (_tinysam__feedframes(ts) && ts->framesRendered == ts->framesCount) break;
				TINYSAM_ASSERT(ts->framesRendered < ts->framesTransitioned);
				frame = ts->frames[ts->framesRendered];
				if (!ts->singmode) frame.pitch -= (frame.frequency1 >> 1);
				frame.amplitude1 = TINYSAM_SA(amplitudeRescale, frame.amplitude1);
				frame.amplitude2 = TINYSAM_SA(amplitudeRescale, frame.amplitude2);
				frame.amplitude3 = TINYSAM_SA(amplitudeRescale, frame.amplitude3);
			}

			// within the first 75% of the glottal pulse?
			// is the count non-zero and the sampled flag is zero?
			if ((--rs.glottalQuick == 0) && (rs.consonant != 0))
			{
				// Voiced sampled phonemes interleave the sample with the glottal pulse.
				// The sample flag is non-zero, so render the sample for the phoneme.

				// mask low three bits and subtract 1 get value to  convert 0 bits on unvoiced samples.
				unsigned char hibyte = (rs.consonant & 7)-1;
				unsigned short hi = hibyte*256;

				// voiced phoneme: Z*, ZH, V*, DH
				unsigned char pitchl = (frame.pitch >> 4);
				for (pitchl ^= 255; pitchl; pitchl++, rs.glottalOff++)
				{
					for (unsigned char bit = 0, sample = TINYSAM_SA(sampleTable, (hi+rs.glottalOff)%SAMPLE_TABLE_SIZE); bit != 8; bit++, sample <<= 1)
					{
						if ((sample & 128) != 0) _TS_OUTPUT(3, (26&0xf)<<4)
						else                     _TS_OUTPUT(4,  6<<4)
					}
					if (pHistory > pHistoryFlush && _tinysam__writesamples(&output, history, &pHistory, ts)) goto FINISH_RENDER;
				}

				rs.glottalPulse = 1;
			}

			if (--rs.glottalPulse == 0)
			{
				// reset the formant wave generators to keep them in sync with the glottal pulse
				rs.glottalPulse = frame.pitch;
				rs.glottalQuick = rs.glottalPulse - (rs.glottalPulse >> 2); // mem44 * 0.75
				rs.phase1 = rs.phase2 = rs.phase3 = 0;
			}
			else
			{
				// not finished with a glottal pulse, reset the phase of the formants to match the pulse
				rs.phase1 += frame.frequency1;
				rs.phase2 += frame.frequency2;
				rs.phase3 += frame.frequency3;
			}
		}
		#undef _TS_OUTPUT
	}

	FINISH_RENDER:
	if (mix) ts->outputmode &= 0x3;
	widthShift += (ts->outputmode == TINYSAM_STEREO_INTERLEAVED);
	char* bufferEnd = (char*)output.buffer;
	int len = (int)(bufferEnd - (char*)bufferStart), fill;
	if (!mix && (fill = ((samples<<widthShift) - len)) > 0)
	{
		memset(bufferEnd, zero, fill);
		if (ts->outputmode == TINYSAM_STEREO_UNWEAVED) memset(bufferEnd + (samples<<widthShift), zero, fill);
	}
	return (len >> widthShift);
}

TINYSAMDEF int tinysam_render_byte(tinysam* ts, unsigned char* buffer, int samples, int flag_mixing)
{
	return _tinysam__render(ts, buffer, samples, flag_mixing, 0, 0x80, (void*(*)(void*, int, unsigned char, struct tinysam*))_tinysam__writebytes);
}

TINYSAMDEF int tinysam_render_short(tinysam* ts, short* buffer, int samples, int flag_mixing)
{
	return _tinysam__render(ts, buffer, samples, flag_mixing, 1, 0, (void*(*)(void*, int, unsigned char, struct tinysam*))_tinysam__writeshorts);
}

TINYSAMDEF int tinysam_render_float(tinysam* ts, float* buffer, int samples, int flag_mixing)
{
	return _tinysam__render(ts, buffer, samples, flag_mixing, 2, 0, (void*(*)(void*, int, unsigned char, struct tinysam*))_tinysam__writefloats);
}

#undef TINYSAM_MALLOC
#undef TINYSAM_FREE
#undef TINYSAM_REALLOC
#undef TINYSAM_MEMCPY
#undef TINYSAM_MEMSET
#undef TINYSAM_MEMMOVE
#undef TINYSAM_ASSERT
#undef TINYSAM_SA
#undef TINYSAM_DEBUGPRINT

#ifdef __cplusplus
}
#endif
#endif //TINYSAM_IMPLEMENTATION
