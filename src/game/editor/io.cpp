/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/client.h>
#include <engine/console.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>
#include <game/gamecore.h> // StrToInts, IntsToStr
#include "editor.h"

template<typename T>
static int MakeVersion(int i, const T &v)
{ return (i<<16)+sizeof(T); }

int CEditor::Save(const char *pFilename)
{
	return m_Map.Save(Kernel()->RequestInterface<IStorage>(), pFilename);
}

int CEditorMap::Save(class IStorage *pStorage, const char *pFileName)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "saving to '%s'...", pFileName);
	m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor", aBuf);
	CDataFileWriter df;
	if(!df.Open(pStorage, pFileName))
	{
		str_format(aBuf, sizeof(aBuf), "failed to open file '%s'...", pFileName);
		m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor", aBuf);
		return 0;
	}

	// save version
	{
		CMapItemVersion Item;
		Item.m_Version = CMapItemVersion::CURRENT_VERSION;
		df.AddItem(MAPITEMTYPE_VERSION, 0, sizeof(Item), &Item);
	}

	// save map info
	{
		CMapItemInfo Item;
		Item.m_Version = CMapItemInfo::CURRENT_VERSION;

		if(m_MapInfo.m_aAuthor[0])
			Item.m_Author = df.AddData(str_length(m_MapInfo.m_aAuthor)+1, m_MapInfo.m_aAuthor);
		else
			Item.m_Author = -1;
		if(m_MapInfo.m_aVersion[0])
			Item.m_MapVersion = df.AddData(str_length(m_MapInfo.m_aVersion)+1, m_MapInfo.m_aVersion);
		else
			Item.m_MapVersion = -1;
		if(m_MapInfo.m_aCredits[0])
			Item.m_Credits = df.AddData(str_length(m_MapInfo.m_aCredits)+1, m_MapInfo.m_aCredits);
		else
			Item.m_Credits = -1;
		if(m_MapInfo.m_aLicense[0])
			Item.m_License = df.AddData(str_length(m_MapInfo.m_aLicense)+1, m_MapInfo.m_aLicense);
		else
			Item.m_License = -1;

		df.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Item), &Item);
	}

	// save images
	for(int i = 0; i < m_lImages.size(); i++)
	{
		CEditorImage *pImg = m_lImages[i];

		// analyse the image for when saving (should be done when we load the image)
		// TODO!
		pImg->AnalyseTileFlags();

		CMapItemImage Item;
		Item.m_Version = CMapItemImage::CURRENT_VERSION;

		Item.m_Width = pImg->m_Width;
		Item.m_Height = pImg->m_Height;
		Item.m_External = pImg->m_External;
		Item.m_ImageName = df.AddData(str_length(pImg->m_aName)+1, pImg->m_aName);
		if(pImg->m_External)
			Item.m_ImageData = -1;
		else
		{
			int PixelSize = pImg->m_Format == CImageInfo::FORMAT_RGB ? 3 : 4;
			Item.m_ImageData = df.AddData(Item.m_Width*Item.m_Height*PixelSize, pImg->m_pData);
		}
		Item.m_Format = pImg->m_Format;
		df.AddItem(MAPITEMTYPE_IMAGE, i, sizeof(Item), &Item);
	}

	// save layers
	int LayerCount = 0, GroupCount = 0;
	for(int g = 0; g < m_lGroups.size(); g++)
	{
		CLayerGroup *pGroup = m_lGroups[g];
		if(!pGroup->m_SaveToMap)
			continue;

		CMapItemGroup GItem;
		GItem.m_Version = CMapItemGroup::CURRENT_VERSION;

		GItem.m_ParallaxX = pGroup->m_ParallaxX;
		GItem.m_ParallaxY = pGroup->m_ParallaxY;
		GItem.m_OffsetX = pGroup->m_OffsetX;
		GItem.m_OffsetY = pGroup->m_OffsetY;
		GItem.m_UseClipping = pGroup->m_UseClipping;
		GItem.m_ClipX = pGroup->m_ClipX;
		GItem.m_ClipY = pGroup->m_ClipY;
		GItem.m_ClipW = pGroup->m_ClipW;
		GItem.m_ClipH = pGroup->m_ClipH;
		GItem.m_StartLayer = LayerCount;
		GItem.m_NumLayers = 0;

		// save group name
		StrToInts(GItem.m_aName, sizeof(GItem.m_aName)/sizeof(int), pGroup->m_aName);

		for(int l = 0; l < pGroup->m_lLayers.size(); l++)
		{
			if(!pGroup->m_lLayers[l]->m_SaveToMap)
				continue;

			if(pGroup->m_lLayers[l]->m_Type == LAYERTYPE_TILES)
			{
				m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving tiles layer");
				CLayerTiles *pLayer = (CLayerTiles *)pGroup->m_lLayers[l];
				pLayer->PrepareForSave();

				CMapItemLayerTilemap Item;
				Item.m_Version = CMapItemLayerTilemap::CURRENT_VERSION;

				Item.m_Layer.m_Version = 0; // unused
				Item.m_Layer.m_Flags = pLayer->m_Flags;
				Item.m_Layer.m_Type = pLayer->m_Type;

				Item.m_Color = pLayer->m_Color;
				Item.m_ColorEnv = pLayer->m_ColorEnv;
				Item.m_ColorEnvOffset = pLayer->m_ColorEnvOffset;

				Item.m_Width = pLayer->m_Width;
				Item.m_Height = pLayer->m_Height;
				Item.m_Flags = pLayer->m_Game ? TILESLAYERFLAG_GAME : 0;
				Item.m_Image = pLayer->m_Image;
				Item.m_Data = df.AddData(pLayer->m_SaveTilesSize, pLayer->m_pSaveTiles);

				// save layer name
				StrToInts(Item.m_aName, sizeof(Item.m_aName)/sizeof(int), pLayer->m_aName);

				if(pLayer->m_Tele)
					Item.m_Flags = 2;
				else if(pLayer->m_Speedup)
					Item.m_Flags = 4;
				else if(pLayer->m_Front)
					Item.m_Flags = 8;
				else if(pLayer->m_Switch)
					Item.m_Flags = 16;
				else
					Item.m_Flags = pLayer->m_Game;
				Item.m_Image = pLayer->m_Image;
				if(pLayer->m_Tele)
				{
					CTile *Tiles = new CTile[pLayer->m_Width*pLayer->m_Height];
					mem_zero(Tiles, pLayer->m_Width*pLayer->m_Height*sizeof(CTile));
					Item.m_Data = df.AddData(pLayer->m_Width*pLayer->m_Height*sizeof(CTile), Tiles);
					Item.m_Tele = df.AddData(pLayer->m_Width*pLayer->m_Height*sizeof(CTeleTile), ((CLayerTele *)pLayer)->m_pTeleTile);
					delete[] Tiles;
				}
				else if(pLayer->m_Speedup)
				{
					CTile *Tiles = new CTile[pLayer->m_Width*pLayer->m_Height];
					mem_zero(Tiles, pLayer->m_Width*pLayer->m_Height*sizeof(CTile));
					Item.m_Data = df.AddData(pLayer->m_Width*pLayer->m_Height*sizeof(CTile), Tiles);
					Item.m_Speedup = df.AddData(pLayer->m_Width*pLayer->m_Height*sizeof(CSpeedupTile), ((CLayerSpeedup *)pLayer)->m_pSpeedupTile);
					delete[] Tiles;
				}
				else if(pLayer->m_Front)
				{
					CTile *Tiles = new CTile[pLayer->m_Width*pLayer->m_Height];
					mem_zero(Tiles, pLayer->m_Width*pLayer->m_Height*sizeof(CTile));
					Item.m_Data = df.AddData(pLayer->m_Width*pLayer->m_Height*sizeof(CTile), Tiles);
					Item.m_Front = df.AddData(pLayer->m_Width*pLayer->m_Height*sizeof(CTile), pLayer->m_pTiles);
					delete[] Tiles;
				}
				else if(pLayer->m_Switch)
				{
					CTile *Tiles = new CTile[pLayer->m_Width*pLayer->m_Height];
					mem_zero(Tiles, pLayer->m_Width*pLayer->m_Height*sizeof(CTile));
					Item.m_Data = df.AddData(pLayer->m_Width*pLayer->m_Height*sizeof(CTile), Tiles);
					Item.m_Switch = df.AddData(pLayer->m_Width*pLayer->m_Height*sizeof(CSwitchTile), ((CLayerSwitch *)pLayer)->m_pSwitchTile);
					delete[] Tiles;
				}
				else
					Item.m_Data = df.AddData(pLayer->m_Width*pLayer->m_Height*sizeof(CTile), pLayer->m_pTiles);
				df.AddItem(MAPITEMTYPE_LAYER, LayerCount, sizeof(Item), &Item);

				GItem.m_NumLayers++;
				LayerCount++;
			}
			else if(pGroup->m_lLayers[l]->m_Type == LAYERTYPE_QUADS)
			{
				m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving quads layer");
				CLayerQuads *pLayer = (CLayerQuads *)pGroup->m_lLayers[l];
				if(pLayer->m_lQuads.size())
				{
					CMapItemLayerQuads Item;
					Item.m_Version = CMapItemLayerQuads::CURRENT_VERSION;
					Item.m_Layer.m_Version = 0; // unused
					Item.m_Layer.m_Flags = pLayer->m_Flags;
					Item.m_Layer.m_Type = pLayer->m_Type;
					Item.m_Image = pLayer->m_Image;

					// add the data
					Item.m_NumQuads = pLayer->m_lQuads.size();
					Item.m_Data = df.AddDataSwapped(pLayer->m_lQuads.size()*sizeof(CQuad), pLayer->m_lQuads.base_ptr());

					// save layer name
					StrToInts(Item.m_aName, sizeof(Item.m_aName)/sizeof(int), pLayer->m_aName);

					df.AddItem(MAPITEMTYPE_LAYER, LayerCount, sizeof(Item), &Item);

					GItem.m_NumLayers++;
					LayerCount++;
				}
			}
		}

		df.AddItem(MAPITEMTYPE_GROUP, GroupCount++, sizeof(GItem), &GItem);
	}

	// check for bezier curve envelopes, otherwise use older, smaller envelope points
	int Version = CMapItemEnvelope_v2::CURRENT_VERSION;
	int Size = sizeof(CEnvPoint_v1);	
	for(int e = 0; e < m_lEnvelopes.size(); e++)
	{
		for(int p = 0; p < m_lEnvelopes[e]->m_lPoints.size(); p++)
		{
			if(m_lEnvelopes[e]->m_lPoints[p].m_Curvetype == CURVETYPE_BEZIER)
			{
				Version = CMapItemEnvelope::CURRENT_VERSION;
				Size = sizeof(CEnvPoint);
				break;
			}
		}
	}

	// save envelopes
	int PointCount = 0;
	for(int e = 0; e < m_lEnvelopes.size(); e++)
	{
		CMapItemEnvelope Item;
		Item.m_Version = Version;
		Item.m_Channels = m_lEnvelopes[e]->m_Channels;
		Item.m_StartPoint = PointCount;
		Item.m_NumPoints = m_lEnvelopes[e]->m_lPoints.size();
		Item.m_Synchronized = m_lEnvelopes[e]->m_Synchronized;
		StrToInts(Item.m_aName, sizeof(Item.m_aName)/sizeof(int), m_lEnvelopes[e]->m_aName);

		df.AddItem(MAPITEMTYPE_ENVELOPE, e, sizeof(Item), &Item);
		PointCount += Item.m_NumPoints;
	}

	// save points
	int TotalSize = Size * PointCount;
	unsigned char *pPoints = (unsigned char *)mem_alloc(TotalSize, 1);
	int Offset = 0;
	for(int e = 0; e < m_lEnvelopes.size(); e++)
	{
		for(int p = 0; p < m_lEnvelopes[e]->m_lPoints.size(); p++)
		{
			mem_copy(pPoints + Offset, &(m_lEnvelopes[e]->m_lPoints[p]), Size);
			Offset += Size;
		}
	}

	df.AddItem(MAPITEMTYPE_ENVPOINTS, 0, TotalSize, pPoints);
	mem_free(pPoints);

	// finish the data file
	df.Finish();
	m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "editor", "saving done");

	// send rcon.. if we can
	if(m_pEditor->Client()->RconAuthed())
	{
		CServerInfo CurrentServerInfo;
		m_pEditor->Client()->GetServerInfo(&CurrentServerInfo);
		char aMapName[128];
		m_pEditor->ExtractName(pFileName, aMapName, sizeof(aMapName));
		if(!str_comp(aMapName, CurrentServerInfo.m_aMap))
			m_pEditor->Client()->Rcon("reload");
	}

	return 1;
}

int CEditor::Load(const char *pFileName, int StorageType)
{
	Reset();
	return m_Map.Load(Kernel()->RequestInterface<IStorage>(), pFileName, StorageType);
}

void CEditor::LoadCurrentMap()
{
	CallbackOpenMap(m_pClient->GetCurrentMapPath(), IStorage::TYPE_ALL, this);
}

int CEditorMap::Load(class IStorage *pStorage, const char *pFileName, int StorageType)
{
	CDataFileReader DataFile;
	if(!DataFile.Open(pStorage, pFileName, StorageType))
		return 0;

	Clean();

	// check version
	CMapItemVersion *pItem = (CMapItemVersion *)DataFile.FindItem(MAPITEMTYPE_VERSION, 0);
	if(!pItem)
	{
		return 0;
	}
	else if(pItem->m_Version == CMapItemVersion::CURRENT_VERSION)
	{
		// load map info
		{
			CMapItemInfo *pItem = (CMapItemInfo *)DataFile.FindItem(MAPITEMTYPE_INFO, 0);
			if(pItem && pItem->m_Version == 1)
			{
				if(pItem->m_Author > -1)
					str_copy(m_MapInfo.m_aAuthor, (char *)DataFile.GetData(pItem->m_Author), sizeof(m_MapInfo.m_aAuthor));
				if(pItem->m_MapVersion > -1)
					str_copy(m_MapInfo.m_aVersion, (char *)DataFile.GetData(pItem->m_MapVersion), sizeof(m_MapInfo.m_aVersion));
				if(pItem->m_Credits > -1)
					str_copy(m_MapInfo.m_aCredits, (char *)DataFile.GetData(pItem->m_Credits), sizeof(m_MapInfo.m_aCredits));
				if(pItem->m_License > -1)
					str_copy(m_MapInfo.m_aLicense, (char *)DataFile.GetData(pItem->m_License), sizeof(m_MapInfo.m_aLicense));
			}
		}

		// load images
		{
			int Start, Num;
			DataFile.GetType( MAPITEMTYPE_IMAGE, &Start, &Num);
			for(int i = 0; i < Num; i++)
			{
				CMapItemImage *pItem = (CMapItemImage *)DataFile.GetItem(Start+i, 0, 0);
				char *pName = (char *)DataFile.GetData(pItem->m_ImageName);

				// copy base info
				CEditorImage *pImg = new CEditorImage(m_pEditor);
				pImg->m_External = pItem->m_External;

				if(pItem->m_External || (pItem->m_Version > 1 && pItem->m_Format != CImageInfo::FORMAT_RGB && pItem->m_Format != CImageInfo::FORMAT_RGBA))
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf),"mapres/%s.png", pName);

					// load external
					CEditorImage ImgInfo(m_pEditor);
					if(m_pEditor->Graphics()->LoadPNG(&ImgInfo, aBuf, IStorage::TYPE_ALL))
					{
						*pImg = ImgInfo;
						pImg->m_Texture = m_pEditor->Graphics()->LoadTextureRaw(ImgInfo.m_Width, ImgInfo.m_Height, ImgInfo.m_Format, ImgInfo.m_pData, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_MULTI_DIMENSION);
						ImgInfo.m_pData = 0;
						pImg->m_External = 1;
					}
				}
				else
				{
					pImg->m_Width = pItem->m_Width;
					pImg->m_Height = pItem->m_Height;
					pImg->m_Format = pItem->m_Version == 1 ? CImageInfo::FORMAT_RGBA : pItem->m_Format;
					int PixelSize = pImg->m_Format == CImageInfo::FORMAT_RGB ? 3 : 4;

					// copy image data
					void *pData = DataFile.GetData(pItem->m_ImageData);
					pImg->m_pData = mem_alloc(pImg->m_Width*pImg->m_Height*PixelSize, 1);
					mem_copy(pImg->m_pData, pData, pImg->m_Width*pImg->m_Height*PixelSize);
					pImg->m_Texture = m_pEditor->Graphics()->LoadTextureRaw(pImg->m_Width, pImg->m_Height, pImg->m_Format, pImg->m_pData, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_MULTI_DIMENSION);
				}

				// copy image name
				if(pName)
					str_copy(pImg->m_aName, pName, 128);

				// load auto mapper file
				pImg->LoadAutoMapper();

				m_lImages.add(pImg);

				// unload image
				DataFile.UnloadData(pItem->m_ImageData);
				DataFile.UnloadData(pItem->m_ImageName);
			}
		}

		// load groups
		{
			int LayersStart, LayersNum;
			DataFile.GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);

			int Start, Num;
			DataFile.GetType(MAPITEMTYPE_GROUP, &Start, &Num);
			for(int g = 0; g < Num; g++)
			{
				CMapItemGroup *pGItem = (CMapItemGroup *)DataFile.GetItem(Start+g, 0, 0);

				if(pGItem->m_Version < 1 || pGItem->m_Version > CMapItemGroup::CURRENT_VERSION)
					continue;

				CLayerGroup *pGroup = NewGroup();
				pGroup->m_ParallaxX = pGItem->m_ParallaxX;
				pGroup->m_ParallaxY = pGItem->m_ParallaxY;
				pGroup->m_OffsetX = pGItem->m_OffsetX;
				pGroup->m_OffsetY = pGItem->m_OffsetY;

				if(pGItem->m_Version >= 2)
				{
					pGroup->m_UseClipping = pGItem->m_UseClipping;
					pGroup->m_ClipX = pGItem->m_ClipX;
					pGroup->m_ClipY = pGItem->m_ClipY;
					pGroup->m_ClipW = pGItem->m_ClipW;
					pGroup->m_ClipH = pGItem->m_ClipH;
				}

				// load group name
				if(pGItem->m_Version >= 3)
					IntsToStr(pGItem->m_aName, sizeof(pGroup->m_aName)/sizeof(int), pGroup->m_aName);

				for(int l = 0; l < pGItem->m_NumLayers; l++)
				{
					CLayer *pLayer = 0;
					CMapItemLayer *pLayerItem = (CMapItemLayer *)DataFile.GetItem(LayersStart+pGItem->m_StartLayer+l, 0, 0);
					if(!pLayerItem)
						continue;

					if(pLayerItem->m_Type == LAYERTYPE_TILES)
					{
						CMapItemLayerTilemap *pTilemapItem = (CMapItemLayerTilemap *)pLayerItem;
						CLayerTiles *pTiles = 0;

						if(pTilemapItem->m_Flags&TILESLAYERFLAG_GAME)
						{
							pTiles = new CLayerGame(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeGameLayer(pTiles);
							MakeGameGroup(pGroup);
						}
						else if(pTilemapItem->m_Flags&TILESLAYERFLAG_TELE)
						{
							if(pTilemapItem->m_Version <= 2)
								pTilemapItem->m_Tele = *((int*)(pTilemapItem) + 15);

							pTiles = new CLayerTele(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeTeleLayer(pTiles);
						}
						else if(pTilemapItem->m_Flags&TILESLAYERFLAG_SPEEDUP)
						{
							if(pTilemapItem->m_Version <= 2)
								pTilemapItem->m_Speedup = *((int*)(pTilemapItem) + 16);

							pTiles = new CLayerSpeedup(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeSpeedupLayer(pTiles);
						}
						else if(pTilemapItem->m_Flags&TILESLAYERFLAG_FRONT)
						{
							if(pTilemapItem->m_Version <= 2)
								pTilemapItem->m_Front = *((int*)(pTilemapItem) + 17);

							pTiles = new CLayerFront(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeFrontLayer(pTiles);
						}
						else if(pTilemapItem->m_Flags&TILESLAYERFLAG_SWITCH)
						{
							if(pTilemapItem->m_Version <= 2)
								pTilemapItem->m_Switch = *((int*)(pTilemapItem) + 18);

							pTiles = new CLayerSwitch(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeSwitchLayer(pTiles);
						}
						else if(pTilemapItem->m_Flags&TILESLAYERFLAG_TUNE)
						{
							if(pTilemapItem->m_Version <= 2)
								pTilemapItem->m_Tune = *((int*)(pTilemapItem) + 19);

							pTiles = new CLayerTune(pTilemapItem->m_Width, pTilemapItem->m_Height);
							MakeTuneLayer(pTiles);
						}
						else
						{
							pTiles = new CLayerTiles(pTilemapItem->m_Width, pTilemapItem->m_Height);
							pTiles->m_pEditor = m_pEditor;
							pTiles->m_Color = pTilemapItem->m_Color;
							pTiles->m_ColorEnv = pTilemapItem->m_ColorEnv;
							pTiles->m_ColorEnvOffset = pTilemapItem->m_ColorEnvOffset;
						}

						pLayer = pTiles;

						pGroup->AddLayer(pTiles);
						void *pData = DataFile.GetData(pTilemapItem->m_Data);
						unsigned int Size = DataFile.GetDataSize(pTilemapItem->m_Data);
						pTiles->m_Image = pTilemapItem->m_Image;
						pTiles->m_Game = pTilemapItem->m_Flags&TILESLAYERFLAG_GAME;

						// load layer name
						if(pTilemapItem->m_Version >= 3)
							IntsToStr(pTilemapItem->m_aName, sizeof(pTiles->m_aName)/sizeof(int), pTiles->m_aName);

						if(pTiles->m_Tele)
						{
							void *pTeleData = DataFile.GetData(pTilemapItem->m_Tele);
							unsigned int Size = DataFile.GetDataSize(pTilemapItem->m_Tele);
							if (Size >= pTiles->m_Width*pTiles->m_Height*sizeof(CTeleTile))
							{
								static const int s_aTilesRep[] = {
									TILE_TELEIN,
									TILE_TELEINEVIL,
									TILE_TELEOUT,
									TILE_TELECHECK,
									TILE_TELECHECKIN,
									TILE_TELECHECKINEVIL,
									TILE_TELECHECKOUT,
									TILE_TELEINWEAPON,
									TILE_TELEINHOOK
								};
								mem_copy(((CLayerTele *)pTiles)->m_pTeleTile, pTeleData, pTiles->m_Width*pTiles->m_Height*sizeof(CTeleTile));

								for(int i = 0; i < pTiles->m_Width*pTiles->m_Height; i++)
								{
									pTiles->m_pTiles[i].m_Index = 0;
									for (unsigned e = 0; e < sizeof(s_aTilesRep) / sizeof(s_aTilesRep[0]); e++)
									{
										if (((CLayerTele *)pTiles)->m_pTeleTile[i].m_Type == s_aTilesRep[e])
											pTiles->m_pTiles[i].m_Index = s_aTilesRep[e];
									}
								}
							}
							DataFile.UnloadData(pTilemapItem->m_Tele);
						}
						else if(pTiles->m_Speedup)
						{
							void *pSpeedupData = DataFile.GetData(pTilemapItem->m_Speedup);
							unsigned int Size = DataFile.GetDataSize(pTilemapItem->m_Speedup);

							if (Size >= pTiles->m_Width*pTiles->m_Height*sizeof(CSpeedupTile))
							{
								mem_copy(((CLayerSpeedup*)pTiles)->m_pSpeedupTile, pSpeedupData, pTiles->m_Width*pTiles->m_Height*sizeof(CSpeedupTile));

								for(int i = 0; i < pTiles->m_Width*pTiles->m_Height; i++)
								{
									if(((CLayerSpeedup*)pTiles)->m_pSpeedupTile[i].m_Force > 0)
										pTiles->m_pTiles[i].m_Index = TILE_BOOST;
									else
										pTiles->m_pTiles[i].m_Index = 0;
								}
							}

							DataFile.UnloadData(pTilemapItem->m_Speedup);
						}
						else if(pTiles->m_Front)
						{
							void *pFrontData = DataFile.GetData(pTilemapItem->m_Front);
							unsigned int Size = DataFile.GetDataSize(pTilemapItem->m_Front);
							if (Size >= pTiles->m_Width*pTiles->m_Height*sizeof(CTile))
								mem_copy(((CLayerFront*)pTiles)->m_pTiles, pFrontData, pTiles->m_Width*pTiles->m_Height*sizeof(CTile));

							DataFile.UnloadData(pTilemapItem->m_Front);
						}
						else if(pTiles->m_Switch)
						{
							void *pSwitchData = DataFile.GetData(pTilemapItem->m_Switch);
							unsigned int Size = DataFile.GetDataSize(pTilemapItem->m_Switch);
							if (Size >= pTiles->m_Width*pTiles->m_Height*sizeof(CSwitchTile))
							{
								const int s_aTilesComp[] = {
									TILE_SWITCHTIMEDOPEN,
									TILE_SWITCHTIMEDCLOSE,
									TILE_SWITCHOPEN,
									TILE_SWITCHCLOSE,
									TILE_FREEZE,
									TILE_DFREEZE,
									TILE_DUNFREEZE,
									TILE_HIT_START,
									TILE_HIT_END,
									TILE_JUMP,
									TILE_PENALTY,
									TILE_BONUS,
									TILE_ALLOW_TELE_GUN,
									TILE_ALLOW_BLUE_TELE_GUN
								};
								CSwitchTile *pLayerSwitchTiles = ((CLayerSwitch *)pTiles)->m_pSwitchTile;
								mem_copy(((CLayerSwitch *)pTiles)->m_pSwitchTile, pSwitchData, pTiles->m_Width*pTiles->m_Height*sizeof(CSwitchTile));

								for(int i = 0; i < pTiles->m_Width*pTiles->m_Height; i++)
								{
									if(((pLayerSwitchTiles[i].m_Type > (ENTITY_CRAZY_SHOTGUN + ENTITY_OFFSET) && ((CLayerSwitch*)pTiles)->m_pSwitchTile[i].m_Type < (ENTITY_DRAGGER_WEAK + ENTITY_OFFSET)) || ((CLayerSwitch*)pTiles)->m_pSwitchTile[i].m_Type == (ENTITY_LASER_O_FAST + 1 + ENTITY_OFFSET)))
										continue;
									else if(pLayerSwitchTiles[i].m_Type >= (ENTITY_ARMOR_1 + ENTITY_OFFSET) && pLayerSwitchTiles[i].m_Type <= (ENTITY_DOOR + ENTITY_OFFSET))
									{
										pTiles->m_pTiles[i].m_Index = pLayerSwitchTiles[i].m_Type;
										pTiles->m_pTiles[i].m_Flags = pLayerSwitchTiles[i].m_Flags;
										continue;
									}

									for (unsigned e = 0; e < sizeof(s_aTilesComp) / sizeof(s_aTilesComp[0]); e++)
									{
										if(pLayerSwitchTiles[i].m_Type == s_aTilesComp[e])
										{
											pTiles->m_pTiles[i].m_Index = s_aTilesComp[e];
											pTiles->m_pTiles[i].m_Flags = pLayerSwitchTiles[i].m_Flags;
										}
									}
								}
							}
							DataFile.UnloadData(pTilemapItem->m_Switch);
						}
						else if(pTiles->m_Tune)
						{
							void *pTuneData = DataFile.GetData(pTilemapItem->m_Tune);
							unsigned int Size = DataFile.GetDataSize(pTilemapItem->m_Tune);
							if (Size >= pTiles->m_Width*pTiles->m_Height*sizeof(CTuneTile))
							{
								CTuneTile *pLayerTuneTiles = ((CLayerTune *)pTiles)->m_pTuneTile;
								mem_copy(((CLayerTune *)pTiles)->m_pTuneTile, pTuneData, pTiles->m_Width*pTiles->m_Height*sizeof(CTuneTile));

								for(int i = 0; i < pTiles->m_Width*pTiles->m_Height; i++)
								{
									if(pLayerTuneTiles[i].m_Type == TILE_TUNE1)
										pTiles->m_pTiles[i].m_Index = TILE_TUNE1;
									else
										pTiles->m_pTiles[i].m_Index = 0;
								}
							}
							DataFile.UnloadData(pTilemapItem->m_Tune);
						}
						else // regular tile layer or game layer
						{
							if (Size >= pTiles->m_Width*pTiles->m_Height*sizeof(CTile))
							{
								mem_copy(pTiles->m_pTiles, pData, pTiles->m_Width*pTiles->m_Height*sizeof(CTile));

								if(pTiles->m_Game && pTilemapItem->m_Version == MakeVersion(1, *pTilemapItem))
								{
									for(int i = 0; i < pTiles->m_Width*pTiles->m_Height; i++)
									{
										if(pTiles->m_pTiles[i].m_Index)
											pTiles->m_pTiles[i].m_Index += ENTITY_OFFSET;
									}
								}
							}
						}

						DataFile.UnloadData(pTilemapItem->m_Data);

						// Remove unused tiles on game and front layers
						/*if(pTiles->m_Game)
						{
							for(int i = 0; i < pTiles->m_Width*pTiles->m_Height; i++)
							{
								if(!IsValidGameTile(pTiles->m_pTiles[i].m_Index))
								{
									if(pTiles->m_pTiles[i].m_Index) {
										char aBuf[256];
										str_format(aBuf, sizeof(aBuf), "game layer, tile %d", pTiles->m_pTiles[i].m_Index);
										m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor", aBuf);
										Changed = true;
									}
									pTiles->m_pTiles[i].m_Index = 0;
									pTiles->m_pTiles[i].m_Flags = 0;
								}
							}
						}
						else if(pTiles->m_Front)
						{
							for(int i = 0; i < pTiles->m_Width*pTiles->m_Height; i++)
							{
								if(!IsValidFrontTile(pTiles->m_pTiles[i].m_Index))
								{
									if(pTiles->m_pTiles[i].m_Index) {
										char aBuf[256];
										str_format(aBuf, sizeof(aBuf), "front layer, tile %d", pTiles->m_pTiles[i].m_Index);
										m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor", aBuf);
										Changed = true;
									}
									pTiles->m_pTiles[i].m_Index = 0;
									pTiles->m_pTiles[i].m_Flags = 0;
								}
							}
						}
						else if(pTiles->m_Tele)
						{
							for(int i = 0; i < pTiles->m_Width*pTiles->m_Height; i++)
							{
								if(!IsValidTeleTile(pTiles->m_pTiles[i].m_Index))
								{
									if(pTiles->m_pTiles[i].m_Index) {
										char aBuf[256];
										str_format(aBuf, sizeof(aBuf), "tele layer, tile %d", pTiles->m_pTiles[i].m_Index);
										m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor", aBuf);
										Changed = true;
									}
									pTiles->m_pTiles[i].m_Index = 0;
									pTiles->m_pTiles[i].m_Flags = 0;
								}
							}
						}
						else if(pTiles->m_Speedup)
						{
							for(int i = 0; i < pTiles->m_Width*pTiles->m_Height; i++)
							{
								if(!IsValidSpeedupTile(pTiles->m_pTiles[i].m_Index))
								{
									if(pTiles->m_pTiles[i].m_Index) {
										char aBuf[256];
										str_format(aBuf, sizeof(aBuf), "speedup layer, tile %d", pTiles->m_pTiles[i].m_Index);
										m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor", aBuf);
										Changed = true;
									}
									pTiles->m_pTiles[i].m_Index = 0;
									pTiles->m_pTiles[i].m_Flags = 0;
								}
							}
						}
						else if(pTiles->m_Switch)
						{
							for(int i = 0; i < pTiles->m_Width*pTiles->m_Height; i++)
							{
								if(!IsValidSwitchTile(pTiles->m_pTiles[i].m_Index))
								{
									if(pTiles->m_pTiles[i].m_Index) {
										char aBuf[256];
										str_format(aBuf, sizeof(aBuf), "switch layer, tile %d", pTiles->m_pTiles[i].m_Index);
										m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "editor", aBuf);
										Changed = true;
									}
									pTiles->m_pTiles[i].m_Index = 0;
									pTiles->m_pTiles[i].m_Flags = 0;
								}
							}
						}*/

						// Convert race stoppers to ddrace stoppers
						/*if(pTiles->m_Game)
						{
							for(int i = 0; i < pTiles->m_Width*pTiles->m_Height; i++)
							{
								if(pTiles->m_pTiles[i].m_Index == 29)
								{
									pTiles->m_pTiles[i].m_Index = 60;
									pTiles->m_pTiles[i].m_Flags = TILEFLAG_HFLIP|TILEFLAG_VFLIP|TILEFLAG_ROTATE;
								}
								else if(pTiles->m_pTiles[i].m_Index == 30)
								{
									pTiles->m_pTiles[i].m_Index = 60;
									pTiles->m_pTiles[i].m_Flags = TILEFLAG_ROTATE;
								}
								else if(pTiles->m_pTiles[i].m_Index == 31)
								{
									pTiles->m_pTiles[i].m_Index = 60;
									pTiles->m_pTiles[i].m_Flags = TILEFLAG_HFLIP|TILEFLAG_VFLIP;
								}
								else if(pTiles->m_pTiles[i].m_Index == 32)
								{
									pTiles->m_pTiles[i].m_Index = 60;
									pTiles->m_pTiles[i].m_Flags = 0;
								}
							}
						}*/

					}
					else if(pLayerItem->m_Type == LAYERTYPE_QUADS)
					{
						CMapItemLayerQuads *pQuadsItem = (CMapItemLayerQuads *)pLayerItem;
						CLayerQuads *pQuads = new CLayerQuads;
						pQuads->m_pEditor = m_pEditor;
						pLayer = pQuads;
						pQuads->m_Image = pQuadsItem->m_Image;
						if(pQuads->m_Image < -1 || pQuads->m_Image >= m_lImages.size())
							pQuads->m_Image = -1;

						// load layer name
						if(pQuadsItem->m_Version >= 2)
							IntsToStr(pQuadsItem->m_aName, sizeof(pQuads->m_aName)/sizeof(int), pQuads->m_aName);

						void *pData = DataFile.GetDataSwapped(pQuadsItem->m_Data);
						pGroup->AddLayer(pQuads);
						pQuads->m_lQuads.set_size(pQuadsItem->m_NumQuads);
						mem_copy(pQuads->m_lQuads.base_ptr(), pData, sizeof(CQuad)*pQuadsItem->m_NumQuads);
						DataFile.UnloadData(pQuadsItem->m_Data);
					}

					if(pLayer)
						pLayer->m_Flags = pLayerItem->m_Flags;
				}
			}
		}

		// load envelopes
		{
			CEnvPoint *pEnvPoints = 0;
			{
				int Start, Num;
				DataFile.GetType(MAPITEMTYPE_ENVPOINTS, &Start, &Num);
				if(Num)
					pEnvPoints = (CEnvPoint *)DataFile.GetItem(Start, 0, 0);
			}

			int Start, Num;
			DataFile.GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);
			for(int e = 0; e < Num; e++)
			{
				CMapItemEnvelope *pItem = (CMapItemEnvelope *)DataFile.GetItem(Start+e, 0, 0);
				CEnvelope *pEnv = new CEnvelope(pItem->m_Channels);
				pEnv->m_lPoints.set_size(pItem->m_NumPoints);
				for(int n = 0; n < pItem->m_NumPoints; n++)
				{
					if(pItem->m_Version >= 3)
					{
						pEnv->m_lPoints[n] = pEnvPoints[pItem->m_StartPoint + n];
					}
					else
					{
						// backwards compatibility
						CEnvPoint_v1 *pEnvPoint_v1 = &((CEnvPoint_v1 *)pEnvPoints)[pItem->m_StartPoint + n];
						mem_zero((void*)&pEnv->m_lPoints[n], sizeof(CEnvPoint));

						pEnv->m_lPoints[n].m_Time = pEnvPoint_v1->m_Time;
						pEnv->m_lPoints[n].m_Curvetype = pEnvPoint_v1->m_Curvetype;

						for(int c = 0; c < pItem->m_Channels; c++)
						{
							pEnv->m_lPoints[n].m_aValues[c] = pEnvPoint_v1->m_aValues[c];
						}
					}
				}

				if(pItem->m_aName[0] != -1)	// compatibility with old maps
					IntsToStr(pItem->m_aName, sizeof(pItem->m_aName)/sizeof(int), pEnv->m_aName);
				m_lEnvelopes.add(pEnv);
				if(pItem->m_Version >= 2)
					pEnv->m_Synchronized = pItem->m_Synchronized;
			}
		}
	}
	else
		return 0;

	return 1;
}

static int gs_ModifyAddAmount = 0;
static void ModifyAdd(int *pIndex)
{
	if(*pIndex >= 0)
		*pIndex += gs_ModifyAddAmount;
}

int CEditor::Append(const char *pFileName, int StorageType)
{
	CEditorMap NewMap;
	NewMap.m_pEditor = this;

	int Err;
	Err = NewMap.Load(Kernel()->RequestInterface<IStorage>(), pFileName, StorageType);
	if(!Err)
		return Err;

	// modify indecies
	gs_ModifyAddAmount = m_Map.m_lImages.size();
	NewMap.ModifyImageIndex(ModifyAdd);

	gs_ModifyAddAmount = m_Map.m_lEnvelopes.size();
	NewMap.ModifyEnvelopeIndex(ModifyAdd);

	// transfer images
	for(int i = 0; i < NewMap.m_lImages.size(); i++)
		m_Map.m_lImages.add(NewMap.m_lImages[i]);
	NewMap.m_lImages.clear();

	// transfer envelopes
	for(int i = 0; i < NewMap.m_lEnvelopes.size(); i++)
		m_Map.m_lEnvelopes.add(NewMap.m_lEnvelopes[i]);
	NewMap.m_lEnvelopes.clear();

	// transfer groups

	for(int i = 0; i < NewMap.m_lGroups.size(); i++)
	{
		if(NewMap.m_lGroups[i] == NewMap.m_pGameGroup)
			delete NewMap.m_lGroups[i];
		else
		{
			NewMap.m_lGroups[i]->m_pMap = &m_Map;
			m_Map.m_lGroups.add(NewMap.m_lGroups[i]);
		}
	}
	NewMap.m_lGroups.clear();

	// all done \o/
	return 0;
}
