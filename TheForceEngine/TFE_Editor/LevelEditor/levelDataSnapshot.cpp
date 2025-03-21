#include "levelEditorData.h"
#include "levelDataSnapshot.h"
#include "sharedState.h"
#include "guidelines.h"
#include <TFE_Editor/snapshotReaderWriter.h>
#include <TFE_Editor/history.h>
#include <TFE_Editor/errorMessages.h>

#include <climits>
#include <algorithm>
#include <vector>
#include <string>

using namespace TFE_Editor;

namespace LevelEditor
{
	void writeEntityVar(const std::vector<EntityVar>& varList)
	{
		const s32 varCount = (s32)varList.size();
		const EntityVar* var = varList.data();

		writeS32(varCount);
		for (s32 v = 0; v < varCount; v++, var++)
		{
			writeS32(var->defId);
			writeS32(var->value.iValue);
			writeString(var->value.sValue);
			writeString(var->value.sValue1);
		}
	}

	void writeEntityLogic(const std::vector<EntityLogic>& logicList)
	{
		const s32 count = (s32)logicList.size();
		const EntityLogic* logic = logicList.data();
		writeS32(count);
		for (s32 i = 0; i < count; i++, logic++)
		{
			writeString(logic->name);
			writeEntityVar(logic->var);
		}
	}

	void readEntityVar(std::vector<EntityVar>& varList)
	{
		const s32 varCount = readS32();
		varList.resize(varCount);

		EntityVar* var = varList.data();
		for (s32 v = 0; v < varCount; v++, var++)
		{
			var->defId = readS32();
			var->value.iValue = readS32();
			readString(var->value.sValue);
			readString(var->value.sValue1);
		}
	}

	void readEntityLogic(std::vector<EntityLogic>& logicList)
	{
		const s32 count = readS32();
		logicList.resize(count);

		EntityLogic* logic = logicList.data();
		for (s32 i = 0; i < count; i++, logic++)
		{
			readString(logic->name);
			readEntityVar(logic->var);
		}
	}
		
	s32 addUniqueTexture(s32 id, std::vector<UniqueTexture>& uniqueTex)
	{
		const s32 count = (s32)uniqueTex.size();
		UniqueTexture* tex = uniqueTex.data();
		for (s32 i = 0; i < count; i++, tex++)
		{
			if (tex->originalId == id)
			{
				return tex->newId;
			}
		}

		UniqueTexture newTex;
		newTex.originalId = id;
		newTex.newId = (s32)uniqueTex.size();
		newTex.name = s_level.textures[id].name;
		uniqueTex.push_back(newTex);
		return newTex.newId;
	}

	s32 addUniqueEntity(s32 id, std::vector<UniqueEntity>& uniqueEntity)
	{
		const s32 count = (s32)uniqueEntity.size();
		UniqueEntity* uentity = uniqueEntity.data();
		for (s32 i = 0; i < count; i++, uentity++)
		{
			if (uentity->originalId == id)
			{
				return uentity->newId;
			}
		}

		UniqueEntity newEntity;
		newEntity.originalId = id;
		newEntity.newId = (s32)uniqueEntity.size();
		newEntity.entity = s_level.entities[id];
		uniqueEntity.push_back(newEntity);
		return newEntity.newId;
	}
		
	void writeEntityToSnapshot(const Entity* entity)
	{
		writeS32(entity->id);
		writeS32(entity->categories);
		writeString(entity->name);
		writeString(entity->assetName);
		writeS32((s32)entity->type);

		writeEntityLogic(entity->logic);
		writeEntityVar(entity->var);
		writeData(entity->bounds, sizeof(Vec3f) * 2);
		writeData(&entity->offset, sizeof(Vec3f));
		writeData(&entity->offsetAdj, sizeof(Vec3f));

		// Sprite and obj data derived from type + assetName
	}

	void writeSectorToSnapshot(const EditorSector* sector)
	{
		writeU32(sector->id);
		writeU32(sector->groupId);
		writeU32(sector->groupIndex);
		writeString(sector->name);
		writeData(&sector->floorTex, sizeof(LevelTexture));
		writeData(&sector->ceilTex, sizeof(LevelTexture));
		writeF32(sector->floorHeight);
		writeF32(sector->ceilHeight);
		writeF32(sector->secHeight);
		writeU32(sector->ambient);
		writeS32(sector->layer);
		writeData(sector->flags, sizeof(u32) * 3);
		writeU32((u32)sector->vtx.size());
		writeU32((u32)sector->walls.size());
		writeU32((u32)sector->obj.size());
		writeData(sector->vtx.data(), u32(sizeof(Vec2f) * sector->vtx.size()));
		writeData(sector->walls.data(), u32(sizeof(EditorWall) * sector->walls.size()));
		writeData(sector->obj.data(), u32(sizeof(EditorObject) * sector->obj.size()));
	}

	void writeSectorAttribSnapshot(const EditorSector* sector)
	{
		writeU32(sector->groupId);
		writeU32(sector->groupIndex);
		writeString(sector->name);
		writeData(&sector->floorTex, sizeof(LevelTexture));
		writeData(&sector->ceilTex, sizeof(LevelTexture));
		writeF32(sector->floorHeight);
		writeF32(sector->ceilHeight);
		writeF32(sector->secHeight);
		writeU32(sector->ambient);
		writeS32(sector->layer);
		writeData(sector->flags, sizeof(u32) * 3);
	}

	void writeLevelNoteToSnapshot(const LevelNote* note)
	{
		writeS32(note->id);
		writeU32(note->groupId);
		writeU32(note->groupIndex);
		writeU32(note->flags);
		writeU32(note->iconColor);
		writeU32(note->textColor);

		writeData(&note->pos, sizeof(Vec3f));
		writeData(&note->fade, sizeof(Vec2f));
		writeString(note->note);
	}

	void writeGuidelineToSnapshot(const Guideline* guideline)
	{
		const s32 vtxCount = (s32)guideline->vtx.size();
		const s32 knotCount = (s32)guideline->knots.size();
		const s32 edgeCount = (s32)guideline->edge.size();
		const s32 offsetCount = (s32)guideline->offsets.size();
		
		writeS32(guideline->id);
		writeS32(vtxCount);
		writeS32(knotCount);
		writeS32(edgeCount);
		writeS32(offsetCount);
		
		writeU32(guideline->flags);
		writeF32(guideline->maxOffset);
		writeF32(guideline->maxHeight);
		writeF32(guideline->minHeight);
		writeF32(guideline->maxSnapRange);
		writeF32(guideline->subDivLen);

		writeData(guideline->bounds.m, sizeof(Vec4f));
		writeData(guideline->vtx.data(), sizeof(Vec2f) * vtxCount);
		writeData(guideline->knots.data(), sizeof(Vec4f) * knotCount);
		writeData(guideline->edge.data(), sizeof(GuidelineEdge) * edgeCount);
		writeData(guideline->offsets.data(), sizeof(f32) * offsetCount);
	}

	void readEntityFromSnapshot(Entity* entity)
	{
		entity->id = readS32();
		entity->categories = readS32();
		readString(entity->name);
		readString(entity->assetName);
		entity->type = (EntityType)readS32();

		readEntityLogic(entity->logic);
		readEntityVar(entity->var);
		readData(entity->bounds, sizeof(Vec3f) * 2);
		readData(&entity->offset, sizeof(Vec3f));
		readData(&entity->offsetAdj, sizeof(Vec3f));
	}

	void readSectorFromSnapshot(EditorSector* sector)
	{
		sector->id = readU32();
		sector->groupId = readU32();
		sector->groupIndex = readU32();
		readString(sector->name);
		readData(&sector->floorTex, sizeof(LevelTexture));
		readData(&sector->ceilTex, sizeof(LevelTexture));
		sector->floorHeight = readF32();
		sector->ceilHeight = readF32();
		sector->secHeight = readF32();
		sector->ambient = readU32();
		sector->layer = readS32();
		readData(sector->flags, sizeof(u32) * 3);

		const u32 vtxCount = readU32();
		const u32 wallCount = readU32();
		const u32 objCount = readU32();
		sector->vtx.resize(vtxCount);
		sector->walls.resize(wallCount);
		sector->obj.resize(objCount);

		readData(sector->vtx.data(), u32(sizeof(Vec2f) * sector->vtx.size()));
		readData(sector->walls.data(), u32(sizeof(EditorWall) * sector->walls.size()));
		readData(sector->obj.data(), u32(sizeof(EditorObject) * sector->obj.size()));
	}

	void readFromSectorAttribSnapshot(EditorSector* sector)
	{
		sector->groupId = readU32();
		sector->groupIndex = readU32();
		readString(sector->name);
		readData(&sector->floorTex, sizeof(LevelTexture));
		readData(&sector->ceilTex, sizeof(LevelTexture));
		sector->floorHeight = readF32();
		sector->ceilHeight = readF32();
		sector->secHeight = readF32();
		sector->ambient = readU32();
		sector->layer = readS32();
		readData(sector->flags, sizeof(u32) * 3);
	}

	void readLevelNoteFromSnapshot(LevelNote* note)
	{
		note->id = readS32();
		note->groupId = readU32();
		note->groupIndex = readU32();
		note->flags = readU32();
		note->iconColor = readU32();
		note->textColor = readU32();

		readData(&note->pos, sizeof(Vec3f));
		readData(&note->fade, sizeof(Vec2f));
		readString(note->note);
	}

	void readGuidelineFromSnapshot(Guideline* guideline)
	{
		guideline->id = readS32();
		const s32 vtxCount = readS32();
		const s32 knotCount = readS32();
		const s32 edgeCount = readS32();
		const s32 offsetCount = readS32();
		guideline->vtx.resize(vtxCount);
		guideline->knots.resize(knotCount);
		guideline->edge.resize(edgeCount);
		guideline->offsets.resize(offsetCount);

		guideline->flags = readU32();
		guideline->maxOffset = readF32();
		guideline->maxHeight = readF32();
		guideline->minHeight = readF32();
		guideline->maxSnapRange = readF32();
		guideline->subDivLen = readF32();

		readData(guideline->bounds.m, sizeof(Vec4f));
		readData(guideline->vtx.data(), sizeof(Vec2f) * vtxCount);
		readData(guideline->knots.data(), sizeof(Vec4f) * knotCount);
		readData(guideline->edge.data(), sizeof(GuidelineEdge) * edgeCount);
		readData(guideline->offsets.data(), sizeof(f32) * offsetCount);

		guideline_computeSubdivision(guideline);
	}
}
