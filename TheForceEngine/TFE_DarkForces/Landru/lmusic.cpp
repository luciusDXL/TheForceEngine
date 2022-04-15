#include "lmusic.h"
#include "lsound.h"
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
		if (!file.open(&filePath, FileStream::MODE_READ)) { return; }

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
						size_t len = strlen(s_sequences[sequence - 1][index].name);
						s_sequences[sequence-1][index].name[len] = 0;

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
		s_curSeq = 0;
		s_curCuePoint = 0;
		memset(s_sequences, 0, sizeof(Sequence) * SEQUENCE_COUNT * MAX_CUE_POINTS);
	}
		
	void lmusic_reset()
	{
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
			TFE_System::logWrite(LOG_MSG, "Landru Music", "Set Seq %lu...", newSeq);
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
		static ImSoundId saveSound = 0;
		ImPause();

		if (s_curSeq && newCuePoint < MAX_CUE_POINTS && newCuePoint != s_curCuePoint)
		{
			if (!newCuePoint)
			{
				s_curCuePoint = newCuePoint;
				ImStopAllSounds();
				saveSound = 0;
			}
			else
			{
				if (s_curCuePoint)
				{
					ImClearTrigger(-1, -1, -1);	// Clear pending midi transitions.

					const char* oldTitle = s_sequences[s_curSeq - 1][s_curCuePoint - 1].name;
					ImSoundId oldSound = ImFindMidi(oldTitle);
					if (!oldSound) { oldSound = saveSound; }
					else { saveSound = oldSound; }

					char* newTitle = s_sequences[s_curSeq - 1][newCuePoint - 1].name;
					char oldChunk  = s_sequences[s_curSeq - 1][newCuePoint - 1].xChunk;
					char newChunk  = s_sequences[s_curSeq - 1][newCuePoint - 1].yChunk;
					s32 oldMeasure = s_sequences[s_curSeq - 1][newCuePoint - 1].xMeasure;
					s32 newMeasure = s_sequences[s_curSeq - 1][newCuePoint - 1].yMeasure;

					if (oldChunk > '@') { oldChunk -= '@'; }
					if (newChunk > '@') { newChunk -= '@'; }

					if (oldChunk == 26)
					{
						// End of sequence marker.
						ImStopSound(oldSound);
						saveSound = 0;
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
						// Nothing.
					}
					else if (oldChunk == '1' || (oldMeasure == 0 && newMeasure == 0))
					{
						// This is in the original code as a printf
						TFE_System::logWrite(LOG_MSG, "Landru Music", "oc: %d om: %d ", ImGetParam(oldSound, midiChunk), ImGetParam(oldSound, midiMeasure));
						s32 newSound = ImFindMidi(newTitle);
						if (oldSound != newSound)
						{
							ImStopSound(oldSound);
							ImStartSound(newSound, 64);
							ImSetHook(oldSound, 0);
							TFE_System::logWrite(LOG_WARNING, "Landru Music", "jhk foulup: old: %d new: %d", oldSound, newSound);
						}
						else
						{
							// Intra-file transition
							ImSetHook(oldSound, 1);
							TFE_System::logWrite(LOG_MSG, "Landru Music", "jump hook 1 set... %d", newCuePoint);
						}
					}
					else  // If current pos < X, jump to Y (or if in different chunk)
					{
						ImSoundId newSound = ImFindMidi(newTitle);
						if (oldSound != newSound)
						{
							ImStopSound(oldSound);
							ImStartSound(newSound, 64);
							ImSetHook(oldSound, 0);
							TFE_System::logWrite(LOG_MSG, "Landru Music", "Switch files  old: %d new: %d", oldSound, newSound);
						}
						else
						{
							if (ImGetParam(oldSound, midiChunk) < oldChunk ||
							   (ImGetParam(oldSound, midiChunk) == oldChunk && ImGetParam(oldSound, midiMeasure) < oldMeasure))
							{
								TFE_System::logWrite(LOG_MSG, "Landru Music", "oc: %d om: %d ", oldChunk, oldMeasure);
								TFE_System::logWrite(LOG_MSG, "Landru Music", "cc: %d cm: %d ", ImGetParam(oldSound, midiChunk), ImGetParam(oldSound, midiMeasure));
								TFE_System::logWrite(LOG_MSG, "Landru Music", "nc: %d nm: %d ", newChunk, newMeasure);
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
					saveSound = newSound;
				}

				s_curCuePoint = newCuePoint;
			}
		}
		ImResume();

		return s_curCuePoint;
	}
}  // namespace TFE_DarkForces