#include "lmusic.h"
#include "lsound.h"
#include <TFE_Asset/gmidAsset.h>
#include <TFE_Audio/midiPlayer.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_Game/igame.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_System/parser.h>
#include <assert.h>

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

		GMidiAsset* gmidi;
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
		TFE_MidiPlayer::stop();
		TFE_MidiPlayer::setVolume(s_baseVolume);
	}

	s32 lmusic_startCutscene(s32 newSeq)
	{
		if (newSeq > SEQUENCE_COUNT) { return s_curSeq; }
		s_baseVolume = TFE_MidiPlayer::getVolume();
		s_volume = s_baseVolume;
		
		if (s_curSeq != newSeq)
		{
			s_curSeq = newSeq;
			TFE_MidiPlayer::stop();

			s_curCuePoint = 0;
			if (s_curSeq)
			{
				for (s32 x = 0; x < MAX_CUE_POINTS; x++)
				{
					if (s_sequences[s_curSeq - 1][x].name[0])
					{
						char soundFile[32];
						const char* soundName = s_sequences[s_curSeq - 1][x].name;
						sprintf(soundFile, "%s.GMD", soundName);
						
						s_sequences[s_curSeq - 1][x].gmidi = TFE_GmidAsset::get(soundFile);
					}
				}
			}
		}

		return s_curSeq;
	}

	GMidiAsset* getMidi(const char* name)
	{
		char soundFile[32];
		sprintf(soundFile, "%s.GMD", name);
		return TFE_GmidAsset::get(soundFile);
	}

	s32 lmusic_setCuePoint(s32 newCuePoint)
	{
		static GMidiAsset* saveSound = nullptr;
		
		if (s_curSeq && newCuePoint < MAX_CUE_POINTS && newCuePoint != s_curCuePoint)
		{
			if (!newCuePoint)
			{
				s_curCuePoint = newCuePoint;
				TFE_MidiPlayer::stop();
				saveSound = nullptr;
			}
			else
			{
				GMidiAsset* oldSound = getMidi(s_sequences[s_curSeq-1][s_curCuePoint-1].name);

				if (s_curCuePoint)
				{
					char* newTitle = s_sequences[s_curSeq - 1][newCuePoint - 1].name;
					char oldChunk  = s_sequences[s_curSeq - 1][newCuePoint - 1].xChunk;
					char newChunk  = s_sequences[s_curSeq - 1][newCuePoint - 1].yChunk;
					s32 oldMeasure = s_sequences[s_curSeq - 1][newCuePoint - 1].xMeasure;
					s32 newMeasure = s_sequences[s_curSeq - 1][newCuePoint - 1].yMeasure;

					if (oldChunk >= '@') { oldChunk -= '@'; }
					if (newChunk >= '@') { newChunk -= '@'; }

					if (oldChunk == 26)
					{
						TFE_MidiPlayer::stop();
						saveSound = nullptr;
					}
					else if (oldChunk == '?')
					{
						// Fade: oldSound, masterVol, 0, 60*((rand()%3)+1));
						s_volume = 0.0f;
						TFE_MidiPlayer::setVolume(s_volume);
					}
					else if (oldChunk == '.')
					{
						// Fade: oldSound, masterVol, 0, 120);
						s_volume = 0.0f;
						TFE_MidiPlayer::setVolume(s_volume);
					}
					else if (oldChunk == '!')
					{
						// Fade: oldSound, masterVol, 0, 60);
						s_volume = 0.0f;
						TFE_MidiPlayer::setVolume(s_volume);
					}
					else if (oldChunk == '0')
					{
						// Nothing.
					}
					else if (oldChunk == '1' || (oldMeasure == 0 && newMeasure == 0))
					{
						GMidiAsset* newSound = getMidi(newTitle);
						if (oldSound != newSound)
						{
							TFE_MidiPlayer::stop();

							s_volume = s_baseVolume;
							TFE_MidiPlayer::setVolume(s_volume);
							TFE_MidiPlayer::playSong(newSound, false);
						}
						else
						{
							// hooks?
						}
					}
					else
					{
						GMidiAsset* newSound = getMidi(newTitle);
						if (oldSound != newSound)
						{
							TFE_MidiPlayer::stop();

							s_volume = s_baseVolume;
							TFE_MidiPlayer::setVolume(s_volume);
							TFE_MidiPlayer::playSong(newSound, false);
						}
						else
						{
							s_volume = s_baseVolume;
							TFE_MidiPlayer::setVolume(s_volume);
							TFE_MidiPlayer::midiJump(newChunk, newMeasure, 1);
						}
					}
				}
				else
				{
					saveSound = s_sequences[s_curSeq - 1][newCuePoint - 1].gmidi;

					s_volume = s_baseVolume;
					TFE_MidiPlayer::setVolume(s_volume);
					TFE_MidiPlayer::playSong(saveSound, false);
				}

				s_curCuePoint = newCuePoint;
			}
		}
		return s_curCuePoint;
	}
}  // namespace TFE_DarkForces