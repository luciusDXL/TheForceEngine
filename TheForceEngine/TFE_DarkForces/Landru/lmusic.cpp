#include "lmusic.h"
#include <TFE_Audio/midiPlayer.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/IMuse/imuse.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_System/parser.h>
#include <TFE_System/system.h>
#include <assert.h>

using namespace TFE_Jedi;
#define SHOW_MUSIC_MSG 0

#if SHOW_MUSIC_MSG
#define LMUSIC_MSG(fmt, ...) TFE_System::logWrite(LOG_MSG, "Landru Music", fmt, __VA_ARGS__)
#else
#define LMUSIC_MSG(fmt, ...)
#endif

namespace TFE_DarkForces
{
	enum MusicConstants
	{
		LEVEL_COUNT    = 14,
		SEQUENCE_COUNT = 20,
		MAX_CUE_POINTS = 20,
	};

	struct Sequence
	{
		char name[10];
		s8   xChunk;
		s32  xMeasure;
		s8   yChunk;
		s32  yMeasure;
	};
	static Sequence s_sequences[SEQUENCE_COUNT][MAX_CUE_POINTS];

	static ImSoundId s_saveSound = IM_NULL_SOUNDID;
	static s32 s_curSeq = 0;
	static s32 s_curCuePoint = 0;
	static f32 s_volume = 1.0f;
	static f32 s_baseVolume = 1.0f;
	
	void lmusic_init()
	{
		s_curSeq = 0;
		s_curCuePoint = 0;
		memset(s_sequences, 0, sizeof(Sequence) * SEQUENCE_COUNT * MAX_CUE_POINTS);

		FilePath filePath;
		FileStream file;
		if (!TFE_Paths::getFilePath("cutmuse.txt", &filePath)) { return; }
		if (!file.open(&filePath, Stream::MODE_READ)) { return; }

		size_t size = file.getSize();
		char* buffer = (char*)landru_alloc(size);
		if (!buffer) { return; }

		file.readBuffer(buffer, (u32)size);
		file.close();

		TFE_Parser parser;
		parser.addCommentString("//");
		parser.addCommentString("#");
		parser.init(buffer, size);

		size_t bufferPos = 0;
		const char* fileData = parser.readLine(bufferPos);

		s32 index = 0;
		char name[80];
		while (fileData)
		{
			s32 sequence;
			if (sscanf(fileData, "SEQUENCE: %d", &sequence) == 1)
			{
				index = 0;
			}
			else if (sscanf(fileData, "CUE: %s", name) == 1)
			{
				fileData = parser.readLine(bufferPos);

				char xChunk, yChunk;
				s32 xMeasure, yMeasure;
				if (sscanf(fileData, "%c %d %c %d", &xChunk, &xMeasure, &yChunk, &yMeasure) == 4)
				{
					if (sequence && sequence <= SEQUENCE_COUNT && index < MAX_CUE_POINTS)
					{
						strncpy(s_sequences[sequence - 1][index].name, name, 9);
						s_sequences[sequence - 1][index].name[9] = 0;

						s_sequences[sequence-1][index].xChunk = xChunk;
						s_sequences[sequence-1][index].xMeasure = xMeasure;
						s_sequences[sequence-1][index].yChunk = yChunk;
						s_sequences[sequence-1][index].yMeasure = yMeasure;
						index++;
					}
				}
			}
			fileData = parser.readLine(bufferPos);
		}

		landru_free(buffer);
	}

	void lmusic_destroy()
	{
		s_saveSound = IM_NULL_SOUNDID;
		s_curSeq = 0;
		s_curCuePoint = 0;
		s_volume = s_baseVolume;
		memset(s_sequences, 0, sizeof(Sequence) * SEQUENCE_COUNT * MAX_CUE_POINTS);
	}
		
	void lmusic_reset()
	{
		s_saveSound = IM_NULL_SOUNDID;
		s_curSeq = 0;
		s_curCuePoint = 0;
		s_volume = s_baseVolume;
	}

	void lmusic_stop()
	{
		ImStopAllSounds();
		ImUnloadAll();
	}

	s32 lmusic_setSequence(s32 newSeq)
	{
		if (newSeq > SEQUENCE_COUNT || s_curSeq == newSeq) { return s_curSeq; }
		s32 oldSeq = s_curSeq;
		s_curSeq = newSeq;

		if (s_curSeq != oldSeq)
		{
			LMUSIC_MSG("Set Seq %lu...", newSeq);
			ImStopAllSounds();
			ImUnloadAll();
			s_curCuePoint = 0;		// Reset cue point on sequence change.
			if (s_curSeq)
			{
				for (s32 i = 0; i < MAX_CUE_POINTS; i++)
				{
					if (s_sequences[s_curSeq - 1][i].name[0])
					{
						ImLoadMidi(s_sequences[s_curSeq - 1][i].name);
					}
				}
			}
		}
		return s_curSeq;
	}

	s32 lmusic_setCuePoint(s32 newCuePoint)
	{
		ImPause();

		if (s_curSeq && (u32)newCuePoint < MAX_CUE_POINTS && newCuePoint != s_curCuePoint)
		{
			if (!newCuePoint)
			{
				ImStopAllSounds();
				s_saveSound = IM_NULL_SOUNDID;
			}
			else
			{
				if (s_curCuePoint)
				{
					ImClearTrigger(-1, -1, -1);	// Clear pending midi transitions.

					const char* oldTitle = s_sequences[s_curSeq - 1][s_curCuePoint - 1].name;
					ImSoundId oldSound = ImFindMidi(oldTitle);
					if (!oldSound) { oldSound = s_saveSound; }
					else { s_saveSound = oldSound; }

					char* newTitle = s_sequences[s_curSeq - 1][newCuePoint - 1].name;
					char oldChunk  = s_sequences[s_curSeq - 1][newCuePoint - 1].xChunk;
					char newChunk  = s_sequences[s_curSeq - 1][newCuePoint - 1].yChunk;
					s32 oldMeasure = s_sequences[s_curSeq - 1][newCuePoint - 1].xMeasure;
					s32 newMeasure = s_sequences[s_curSeq - 1][newCuePoint - 1].yMeasure;

					// A = 1, B = 2, ...
					if (oldChunk > '@') { oldChunk -= '@'; }
					if (newChunk > '@') { newChunk -= '@'; }

					if (oldChunk == 26)
					{
						// End of sequence marker.
						ImStopSound(oldSound);
						s_saveSound = IM_NULL_SOUNDID;
					}
					else if (oldChunk == '?')
					{
						// Fade randomly over 1 - 3 seconds.
						ImFadeParam(oldSound, soundVol, 0, 60 * ((rand() % 3) + 1));
					}
					else if (oldChunk == '.')
					{
						// Fade slowly over 2 seconds.
						ImFadeParam(oldSound, soundVol, 0, 120);
					}
					else if (oldChunk == '!')
					{
						// Fade over 1 second.
						ImFadeParam(oldSound, soundVol, 0, 60);
					}
					else if (oldChunk == '0')
					{
						// Do nothing.
					}
					else if (oldChunk == '1' || (oldMeasure == 0 && newMeasure == 0))
					{
						// This is in the original code as a printf
						LMUSIC_MSG("oc: %d om: %d ", ImGetParam(oldSound, midiChunk), ImGetParam(oldSound, midiMeasure));
						ImSoundId newSound = ImFindMidi(newTitle);
						if (oldSound != newSound)
						{
							ImStopSound(oldSound);
							ImStartSound(newSound, 64);
							ImSetHook(oldSound, 0);
							TFE_System::logWrite(LOG_WARNING, "Landru Music", "jhk foulup: old: %d new: %d", oldSound, newSound);
						}
						else
						{
							// This will take the next '1' value jump (this is used to break out of loops, etc.).
							ImSetHook(oldSound, 1);
							LMUSIC_MSG("jump hook 1 set... %d", newCuePoint);
						}
					}
					else  // If current pos < X or in a different chunk, jump to Y.
					{
						ImSoundId newSound = ImFindMidi(newTitle);
						if (oldSound != newSound)
						{
							ImStopSound(oldSound);
							ImStartSound(newSound, 64);
							ImSetHook(oldSound, 0);
							LMUSIC_MSG("Switch files old: 0x%x new: 0x%x", oldSound, newSound);
						}
						else
						{
							if (ImGetParam(oldSound, midiChunk) < oldChunk ||
							   (ImGetParam(oldSound, midiChunk) == oldChunk && ImGetParam(oldSound, midiMeasure) < oldMeasure))
							{
								LMUSIC_MSG("oc: %d om: %d ", oldChunk, oldMeasure);
								LMUSIC_MSG("cc: %d cm: %d ", ImGetParam(oldSound, midiChunk), ImGetParam(oldSound, midiMeasure));
								LMUSIC_MSG("nc: %d nm: %d ", newChunk, newMeasure);
								ImJumpMidi(newSound, newChunk, newMeasure, 1, 0, 0);
								ImSetHook(oldSound, 0);
							}
							else
							{
								TFE_System::logWrite(LOG_WARNING, "Landru Music", "nogo");
							}
						}
					}
				}
				else
				{
					ImSoundId newSound = ImFindMidi(s_sequences[s_curSeq - 1][newCuePoint - 1].name);
					ImStartSound(newSound, 64);
					s_saveSound = newSound;
				}
			}

			// Update the cue point.
			s_curCuePoint = newCuePoint;
		}
		ImResume();

		return s_curCuePoint;
	}
}  // namespace TFE_DarkForces