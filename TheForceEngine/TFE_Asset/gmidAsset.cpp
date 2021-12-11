#include <cstring>

#include "gmidAsset.h"
#include <TFE_System/system.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Archive/archive.h>
#include <TFE_System/parser.h>
#include <TFE_FileSystem/filestream.h>
#include <assert.h>
#include <map>
#include <algorithm>

namespace TFE_GmidAsset
{
#define DARK_FORCES_SYSEX 0x7D

	typedef std::map<std::string, GMidiAsset*> GMidMap;
	static GMidMap s_gmidAssets;
	static std::vector<u8> s_buffer;

	bool parseGMidi(GMidiAsset* midi);

	GMidiAsset* get(const char* name)
	{
		GMidMap::iterator iGMid = s_gmidAssets.find(name);
		if (iGMid != s_gmidAssets.end())
		{
			return iGMid->second;
		}

		FilePath filePath;
		if (!TFE_Paths::getFilePath(name, &filePath))
		{
			return nullptr;
		}

		FileStream gmidAsset;
		if (!gmidAsset.open(&filePath, FileStream::MODE_READ))
		{
			return nullptr;
		}
		size_t size = gmidAsset.getSize();
		s_buffer.resize(size + 1);
		gmidAsset.readBuffer(s_buffer.data(), (u32)size);
		gmidAsset.close();

		GMidiAsset* midi = new GMidiAsset;
		if (!parseGMidi(midi))
		{
			delete midi;
			return nullptr;
		}

		s_gmidAssets[name] = midi;
		strcpy(midi->name, name);
		return midi;
	}

	void free(GMidiAsset* asset)
	{
	}

	void freeAll()
	{
		GMidMap::iterator iMidi = s_gmidAssets.begin();
		for (; iMidi != s_gmidAssets.end(); ++iMidi)
		{
			GMidiAsset* midi = iMidi->second;
			delete midi;
		}
		s_gmidAssets.clear();
	}

	struct GmdChunk
	{
		u32 type;
		u32 size;
	};

	void readChunk(const u8*& buffer, GmdChunk* chunk)
	{
		memcpy(&chunk->type, buffer, 4);
		buffer += 4;
		chunk->size = (u32)buffer[3] | ((u32)buffer[2] << 8u) | ((u32)buffer[1] << 16u) | ((u32)buffer[0] << 24u);
		buffer += 4;
	}

	u32 readU32(const u8*& buffer)
	{
		const u32 value = (u32)buffer[3] | ((u32)buffer[2] << 8u) | ((u32)buffer[1] << 16u) | ((u32)buffer[0] << 24u);
		buffer += 4;

		return value;
	}

	u32 readU24(const u8*& buffer)
	{
		const u32 value = (u32)buffer[2] | ((u32)buffer[1] << 8u) | ((u32)buffer[0] << 16u);
		buffer += 3;

		return value;
	}

	u32 readU16(const u8*& buffer)
	{
		const u32 value = (u32)buffer[1] | ((u32)buffer[0] << 8u);
		buffer += 2;

		return value;
	}

	u32 readU8(const u8*& buffer)
	{
		const u32 value = (u32)buffer[0];
		buffer++;
		return value;
	}

	u32 readVariableLength(const u8*& buffer)
	{
		u32 value = 0u;
		for (u32 i = 0; i < 4; i++)
		{
			const u8 partial = *buffer;
			buffer++;

			value = (value << 7u) | (partial & 0x7f);
			if (!(partial & 0x80)) { break; }
		}
		return value;
	}

	#define CHUNK_ID(c0, c1, c2, c3) ((u32)c0 | ((u32)c1 << 8u) | ((u32)c2 << 16u) | ((u32)c3 << 24u))
	static const u32 c_MIDI = CHUNK_ID('M', 'I', 'D', 'I');
	static const u32 c_MDpg = CHUNK_ID('M', 'D', 'p', 'g');
	static const u32 c_MThd = CHUNK_ID('M', 'T', 'h', 'd');
	static const u32 c_MTrk = CHUNK_ID('M', 'T', 'r', 'k');
		
	void addMidiEvent(Track* track, u32 tick, u8 evt, s8 channel, u8 data1, u8 data2)
	{
		const u32 index = (u32)track->midiEvents.size();
		track->eventList.push_back({ tick, MTK_MIDI, index });
		track->midiEvents.push_back({ evt, channel, data1, data2 });
	}

	void addTempoEvent(Track* track, u32 tick, f32 msPerTick)
	{
		const u32 index = (u32)track->tempoEvents.size();
		track->eventList.push_back({ tick, MTK_TEMPO, index });
		track->tempoEvents.push_back({ msPerTick });
	}

	void addMidiMarker(Track* track, u32 tick, const char* text)
	{
		const u32 index = (u32)track->markers.size();
		track->eventList.push_back({ tick, MTK_MARKER, index });
		track->markers.push_back({});
		strcpy(track->markers[index].name, text);
	}

#define TICKS_PER_BEAT 480

	const char* c_iMuseCommandStrings[]=
	{
		"start new",		//IMUSE_START_NEW = 0,
		"stalk trans",		//IMUSE_STALK_TRANS,
		"fight trans",		//IMUSE_FIGHT_TRANS,
		"engage trans",		//IMUSE_ENGAGE_TRANS,
		"from fight",		//IMUSE_FROM_FIGHT,
		"from stalk",		//IMUSE_FROM_STALK,
		"from boss",		//IMUSE_FROM_BOSS,
		"clear callback",	//IMUSE_CLEAR_CALLBACK,
		"to",				//IMUSE_TO
	};
		
	void addSysExEvent(Track* track, u32 tick, u8 type, u32 size, const char* data)
	{
		const u32 index = (u32)track->imuseEvents.size();
		track->eventList.push_back({ tick, MTK_IMUSE, index });
		iMuseEvent evt = {};

		if (type == 1)
		{
			// For now just treat this as loop points.
			if (track->loopStart < 0)
			{
				track->loopStart = tick;
				evt.cmd = IMUSE_LOOP_START;
				evt.arg[0].nArg = tick;
			}
			else
			{
				track->loopEnd = tick;
				evt.cmd = IMUSE_LOOP_END;
				evt.arg[0].nArg = track->loopStart;
				evt.arg[1].nArg = tick;
			}
		}
		else
		{
			// parse the command.
			TFE_Parser parser;
			TokenList tokens;
			
			evt.cmd = IMUSE_UNKNOWN;
			for (u32 i = 0; i < TFE_ARRAYSIZE(c_iMuseCommandStrings); i++)
			{
				if (strncasecmp(data, c_iMuseCommandStrings[i], strlen(c_iMuseCommandStrings[i])) == 0)
				{
					parser.tokenizeLine(&data[strlen(c_iMuseCommandStrings[i])], tokens);
					evt.cmd = iMuseCommand(i);
				}
			}
			if (evt.cmd == IMUSE_UNKNOWN)
			{
				return;
			}
			
			char* endPtr = nullptr;
			switch (evt.cmd)
			{
				case IMUSE_START_NEW:
				{
					// No data.
				} break;
				case IMUSE_STALK_TRANS:
				{
					assert(tokens.size() >= 1 && tokens.size() < 3);
					if (tokens.size() >= 1)
					{
						evt.arg[0].fArg = (f32)strtod(tokens[0].c_str(), &endPtr);
						if (tokens.size() >= 2)
							evt.arg[1].fArg = (f32)strtod(tokens[1].c_str(), &endPtr);
						else
							evt.arg[1].fArg = 0.0f;
					}
				} break;
				case IMUSE_FIGHT_TRANS:
				{
					assert(tokens.size() >= 1 && tokens.size() <= 3);
					if (tokens.size() >= 1)
					{
						evt.arg[0].fArg = (f32)strtod(tokens[0].c_str(), &endPtr);
						
						if (tokens.size() >= 2)
							evt.arg[1].fArg = (f32)strtod(tokens[1].c_str(), &endPtr);
						else
							evt.arg[1].fArg = 0.0f;

						if (tokens.size() >= 3)
							evt.arg[2].fArg = (f32)strtod(tokens[2].c_str(), &endPtr);
						else
							evt.arg[2].fArg = 0.0f;
					}
				} break;
				case IMUSE_ENGAGE_TRANS:
				{
					assert(tokens.size() == 1);
					if (tokens.size() >= 1)
					{
						evt.arg[0].fArg = (f32)strtod(tokens[0].c_str(), &endPtr);
					}
				} break;
				case IMUSE_FROM_FIGHT:
				{
					assert(tokens.size() >= 1);
					if (tokens.size() >= 1)
					{
						evt.arg[0].fArg = (f32)strtod(tokens[0].c_str(), &endPtr);

						if (tokens.size() >= 2)
							evt.arg[1].fArg = (f32)strtod(tokens[1].c_str(), &endPtr);
						else
							evt.arg[1].fArg = 0.0f;

						if (tokens.size() >= 3)
							evt.arg[2].fArg = (f32)strtod(tokens[2].c_str(), &endPtr);
						else
							evt.arg[2].fArg = 0.0f;
					}
				} break;
				case IMUSE_FROM_STALK:
				{
					assert(tokens.size() >= 1);
					if (tokens.size() >= 1)
					{
						evt.arg[0].fArg = (f32)strtod(tokens[0].c_str(), &endPtr);

						if (tokens.size() >= 2)
							evt.arg[1].fArg = (f32)strtod(tokens[1].c_str(), &endPtr);
						else
							evt.arg[1].fArg = 0.0f;

						if (tokens.size() >= 3)
							evt.arg[2].fArg = (f32)strtod(tokens[2].c_str(), &endPtr);
						else
							evt.arg[2].fArg = 0.0f;
					}
				} break;
				case IMUSE_FROM_BOSS:
				{
					assert(tokens.size() >= 1 && tokens.size() < 3);
					if (tokens.size() >= 1)
					{
						evt.arg[0].fArg = (f32)strtod(tokens[0].c_str(), &endPtr);
						if (tokens.size() >= 2)
							evt.arg[1].fArg = (f32)strtod(tokens[1].c_str(), &endPtr);
						else
							evt.arg[1].fArg = 0.0f;

						evt.arg[1].fArg = 0.0f;
						evt.arg[2].fArg = 0.0f;
					}
				} break;
				case IMUSE_CLEAR_CALLBACK:
				{
					// No data.
				} break;
				case IMUSE_TO:
				{
					assert(tokens.size() >= 1);
					const char* X[] = { "A", "B", "C", "D", "E" };

					if (tokens.size() >= 2)
					{
						assert(strcasecmp(tokens[1].c_str(), "slow") == 0);
						evt.arg[0].nArg = -1;
						evt.arg[1].nArg = -1;
						for (u32 i = 0; i < 5; i++)
						{
							if (strcasecmp(tokens[0].c_str(), X[i]) == 0)
							{
								evt.arg[0].nArg = i;
								evt.arg[1].nArg = 1;	//slow
							}
						}
						assert(evt.arg[0].nArg >= 0 && evt.arg[1].nArg >= 0);
					}
					else if (tokens.size() >= 1)
					{
						const char* Xslow[] = { "Aslow", "Bslow", "Cslow", "Dslow", "Eslow" };
						
						evt.arg[0].nArg = -1;
						evt.arg[1].nArg = -1;
						for (u32 i = 0; i < 5; i++)
						{
							if (strcasecmp(tokens[0].c_str(), Xslow[i]) == 0)
							{
								evt.arg[0].nArg = i;
								evt.arg[1].nArg = 1;	//slow
							}
							else if (strcasecmp(tokens[0].c_str(), X[i]) == 0)
							{
								evt.arg[0].nArg = i;
								evt.arg[1].nArg = 0;	//normal
							}
						}
						assert(evt.arg[0].nArg >= 0 && evt.arg[1].nArg >= 0);
					}
				} break;
			}
		}
		assert(type == 1 || type == 3);
		track->imuseEvents.push_back(evt);
	}

	bool parseGMidi(GMidiAsset* midi)
	{
		const u8* buffer = s_buffer.data();
		const u32 size = (u32)s_buffer.size();
		const u8* end = buffer + size;

		// Read the header.
		GmdChunk header;
		readChunk(buffer, &header);
		// Check the header type and make sure it is correct.
		if (header.type != c_MIDI) { return false; }

		u32 tick = 0;
		u32 trackIndex = 0;
		u32 ticksPerQuarterNote = 0;
		Track* curTrack = nullptr;
		midi->length = 0u;
		bool endReached = false;
		while (buffer < end && !endReached)
		{
			// Read each chunk in order.
			GmdChunk chunk;
			readChunk(buffer, &chunk);
			
			// Sometimes there is a bad chunk at the end, so detect that case.
			if (buffer >= end)
			{
				break;
			}

			const u8* chunkData = buffer;
			switch (chunk.type)
			{
				case c_MDpg:
				{
				} break;
				case c_MThd:
				{
					const u32 format = readU16(chunkData);
					midi->trackCount = readU16(chunkData);
					const u32 division = readU16(chunkData);
					if (division & 0x8000)
					{
						assert(0);
						TFE_System::logWrite(LOG_ERROR, "Midi", "Time code not supported.");
					}
					else
					{
						ticksPerQuarterNote = division & 0x7fff;
					}
					midi->tracks.resize(midi->trackCount);
					curTrack = &midi->tracks[0];

					curTrack->name[0] = 0;
					curTrack->length = 0;
					curTrack->id = 0;
					curTrack->pad16 = 0;
					curTrack->loopStart = -1;
					curTrack->loopEnd = -1;

					curTrack->ticksPerBeat = ticksPerQuarterNote;
					curTrack->ticksPerMeasure = ticksPerQuarterNote * 4;
				} break;
				case c_MTrk:
				{
					const u8* chunkDataEnd = chunkData + chunk.size;
					while (chunkData < chunkDataEnd)
					{
						u32 deltaTime = readVariableLength(chunkData);
						u8  midiEvent = readU8(chunkData);

						tick += deltaTime;
						if (midiEvent == 0)
						{
							// We are done now.
							if (curTrack->length == 0)
							{
								curTrack->length = tick;
							}
							break;
						}
						else if (midiEvent == 0xff)
						{
							// Meta Event
							const MetaType metaType = (MetaType)readU8(chunkData);
							const u32 metaLength = readVariableLength(chunkData);
							const u8* eventData  = chunkData;

							if (metaType == META_SEQ_NAME || metaType == META_TEXT_EVENT || metaType == META_MARKER_TEXT)
							{
								char text[256];
								assert(metaLength < 256);
								memcpy(text, eventData, metaLength);
								text[metaLength] = 0;

								if (metaType == META_SEQ_NAME)
								{
									strcpy(curTrack->name, text);
								}
								else if (metaType == META_MARKER_TEXT)
								{
									addMidiMarker(curTrack, tick, text);
								}
							}
							else if (metaType == META_TEMPO)
							{
								// microseconds per quarter note (i.e. seconds per note = microsec / 1,000,000)
								// 1 - 16777215
								// 500,000 = 120 beats per minute
								u32 tempo = readU24(eventData);
								f64 msPerTick = 0.001 * (f64)tempo / (f64)ticksPerQuarterNote;

								// TODO: Is storing ms per tick in float form good enough?
								// If not than integer form can be used (x100000).
								if (tick == 0u)
								{
									curTrack->msPerTick = (f32)msPerTick;
								}
								else
								{
									addTempoEvent(curTrack, tick, (f32)msPerTick);
								}
							}
							else if (metaType == META_TIME_SIG)
							{
								u32 num = eventData[0];
								u32 denom = 1u << (u32)eventData[1];
								u32 midiClocksPerMetrononeClick = eventData[2];		// 24 = quarter note.
								u32 thirtySecPerQuarter = eventData[3];				//  8 = standard 32nd notes.
								// For now assume 4/4 time, 24 ticks per quarter note, 8 32nd notes per quarter note.
								// This seems to match all of the data in Dark Forces.
								assert(num == 4 && denom == 4 && midiClocksPerMetrononeClick == 24 && thirtySecPerQuarter == 8);
							}
							else if (metaType == META_END_OF_TRACK)
							{
								curTrack->length = tick;
								midi->length = std::max(midi->length, curTrack->length);

								tick = 0u;
								trackIndex++;
								if (trackIndex < midi->trackCount)
								{
									curTrack = &midi->tracks[trackIndex];

									curTrack->name[0] = 0;
									curTrack->msPerTick = midi->tracks[0].msPerTick;
									curTrack->id = trackIndex;
									curTrack->length = 0;
									curTrack->pad16 = 0;
									curTrack->loopStart = -1;
									curTrack->loopEnd = -1;
								}
							}
							chunkData += metaLength;
						}
						else if (midiEvent == 0xf0 || midiEvent == 0xf7)
						{
							// SysEx Event
							const u32 size = readVariableLength(chunkData);
							if (*chunkData == DARK_FORCES_SYSEX)
							{
								addSysExEvent(curTrack, tick, *(chunkData+1), size - 3, (const char*)chunkData + 2);
							}
							while (*chunkData != 0xf7) { chunkData++; }
							assert(*chunkData == 0xf7);
							chunkData++;
						}
						else if (midiEvent & 0x80)
						{
							assert(midiEvent < 0xf0);
							const u32 evtType    = midiEvent & 0xf0;
							const u32 evtChannel = midiEvent & 0x0f;
							const u32 midiEvtLen = (evtType == MID_PROGRAM_CHANGE || evtType == MID_CHANNEL_PRESSURE) ? 1 : 2;
																					
							const u8 data1 = readU8(chunkData);
							const u8 data2 = midiEvtLen > 1 ? readU8(chunkData) : 0;

							addMidiEvent(curTrack, tick, evtType, evtChannel, data1, data2);
						}
						else
						{
							TFE_System::logWrite(LOG_ERROR, "Midi", "Running status not supported: 0x%x", midiEvent);
							chunkData++;
						}
					}
				} break;
				default:
				{
					const char* typeChar = (char*)&chunk.type;
					TFE_System::logWrite(LOG_ERROR, "Midi", "Unknown GMID chunk type: %c%c%c%c", typeChar[0], typeChar[1], typeChar[2], typeChar[3]);
				}
			}

			buffer += chunk.size;
		}

		return midi->trackCount > 0;
	}
}
