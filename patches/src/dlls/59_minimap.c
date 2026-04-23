#include "patches.h"

#include "PR/gbi.h"
#include "game/gamebits.h"
#include "sys/map.h"
#include "sys/objects.h"
#include "sys/main.h"
#include "sys/camera.h"
#include "sys/rcp.h"
#include "macros.h"

#include "recomp/dlls/engine/59_minimap_recomp.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define MINIMAP_SCREEN_X 50
#define MINIMAP_SCREEN_Y 200
#define NO_MAP_ID -1

typedef struct {
    s16 minX; //Section's bounds in worldSpace
    s16 maxX; //Section's bounds in worldSpace
    s16 minZ; //Section's bounds in worldSpace
    s16 maxZ; //Section's bounds in worldSpace
    s16 minY; //Section's bounds in worldSpace
    s16 maxY; //Section's bounds in worldSpace
    u16 gamebitSection; //For toggling a specific section of a map (e.g. deciding when to switch between the exterior/interior maps of LightFoot Village's ring fort)
    s8 screenOffsetX; //For repositioning map tiles' framing on screen
    s8 screenOffsetY; //For repositioning map tiles' framing on screen
    u16 texTableID; //Index into TEXTABLE.bin, used to find the texture's index in TEX0.bin
} MinimapSection;

typedef struct {
    MinimapSection* tiles;
    u16 gamebitLevel; //Overall gamebitID for having obtained the level's map (not checked)
    u8 mapID;
    u8 tileCount;
} MinimapLevel;

/*0xBF8*/ extern MinimapLevel sMinimapLevels[30];

/*0x0*/ extern s32 sLoadedTexTableID;           //Current map tile's index in TEXTABLE.bin (maps a texture in TEX0.bin)
/*0x4*/ extern u8 sMinimapVisible;
/*0x8*/ extern s16 sOpacity;
/*0xC*/ extern Texture* sMapTile;            //current map texture
/*0x10*/ extern Texture* sMarkerSidekick;    //green dot texture
/*0x14*/ extern Texture* sMarkerPlayer;      //blue diamond texture
/*0x18*/ extern s8 sOffsetX;
/*0x1C*/ extern s8 sOffsetY;
/*0x20*/ extern u8 sGridX;
/*0x24*/ extern u8 sGridZ;
/*0x28*/ extern s16 sLevelMaxX;
/*0x2C*/ extern s16 sLevelMaxZ;
/*0x30*/ extern s16 sLevelMinX;
/*0x34*/ extern s16 sLevelMinZ;

RECOMP_PATCH s32 minimap_print(Gfx **gdl, s32 arg1) {
    Object* player;
    Object* sidekick;
    s32 loadTextureID;
    s16 playerX;
    s16 playerZ;
    s16 playerY;
    u8 index;
    u8 index2;
    u16 texTableID;
    s16 tileCount;
    MinimapSection* mapTiles;
    u8 mapID;
    u8 tileIndex;
    s8 mapFound;
    MinimapLevel* mapLevel;
    MinimapSection* tile;

    loadTextureID = 0;
    mapFound = FALSE;
    index = 0;
    index2 = 0;
    mapID = 0;
    player = get_player();
    sidekick = get_sidekick();
    if (player != NULL) {
        //Get mapID from player's coords, or from the mobile map Object they're parented to
        if (player->parent) {
            mapID = player->parent->mapID;
        } else {
            mapID = map_get_map_id_from_xz_ws(player->srt.transl.x, player->srt.transl.z);
        }

        //Iterate over the map definitions until the one with the relevant mapID is found
        while (mapFound == FALSE && (u32)index < ARRAYCOUNT(sMinimapLevels)){
            if (mapID == sMinimapLevels[index].mapID) { 
                mapFound = TRUE;
            } else {
                index++;
            }
        }
        
        if (mapFound) {
            playerX = player->globalPosition.x;
            playerZ = player->globalPosition.z;
            playerY = player->globalPosition.y;
            
            //Iterate over the map's tiles, until finding a tile whose bounding volume contains the player coords
            mapLevel = &sMinimapLevels[index];
            mapTiles = mapLevel->tiles;
            for (mapFound = FALSE, index = 0; index2 < mapLevel->tileCount; index2++){
                if ((playerX >= mapTiles[index2].minX) && (playerX < mapTiles[index2].maxX) && 
                    (playerZ >= mapTiles[index2].minZ) && (playerZ < mapTiles[index2].maxZ) && 
                    (playerY >= mapTiles[index2].minY) && (playerY < mapTiles[index2].maxY) && 
                    main_get_bits(mapTiles[index2].gamebitSection)) { //Make sure the tile's gamebit is set

                    tileIndex = 0;
                    mapFound = TRUE;
                    loadTextureID = 0;
                    //Store the tile's texTableID, for loading a TEX0 minimap texture
                    if (mapTiles[index2].texTableID) {
                        loadTextureID = mapTiles[index2].texTableID;
                    }
                    
                    //@bug?: calculates the bounds every frame
                    if (sLoadedTexTableID == mapTiles[index2].texTableID) {
                        tileCount = mapLevel->tileCount;
                        sLevelMaxX = -0x8000;
                        sLevelMaxZ = -0x8000;
                        sLevelMinX = 0x7FFF;
                        sLevelMinZ = 0x7FFF;

                        //Getting minX/maxX, minZ/maxZ for whole map
                        while (tileIndex < tileCount){
                            if (loadTextureID == mapTiles[tileIndex].texTableID) {
                                sLevelMinX = (mapTiles[tileIndex].minX < sLevelMinX) ? mapTiles[tileIndex].minX : sLevelMinX;
                                sLevelMaxX = (mapTiles[tileIndex].maxX > sLevelMaxX) ? mapTiles[tileIndex].maxX : sLevelMaxX;
                                sLevelMinZ = (mapTiles[tileIndex].minZ < sLevelMinZ) ? mapTiles[tileIndex].minZ : sLevelMinZ;
                                sLevelMaxZ = (mapTiles[tileIndex].maxZ > sLevelMaxZ) ? mapTiles[tileIndex].maxZ : sLevelMaxZ;
                            }
                            tileIndex++;
                        }

                        //Getting gridX and gridZ
                        sGridX = ((sLevelMaxX - sLevelMinX) * 8) / BLOCKS_GRID_UNIT;
                        if (sGridX > 24) {
                            sGridX = 24;
                        }
                        sGridZ = ((sLevelMaxZ - sLevelMinZ) * 8) / BLOCKS_GRID_UNIT;
                        if (sGridZ > 24) {
                            sGridZ = (sGridZ * 2) - 24;
                        }

                        //Store screen offset for the tile
                        sOffsetX = mapTiles[index2].screenOffsetX;
                        sOffsetY = mapTiles[index2].screenOffsetY;
                    }
                    break;                
                }
            }
            
        }
        
        //Set/get minimap gamebits
        main_set_bits(BIT_Toggle_Minimap, mapFound);
        if (sMinimapVisible == FALSE || main_get_bits(BIT_Hide_Minimap)) {
            loadTextureID = 0;
        }
        
        //Hide during cutscenes
        if (camera_get_letterbox()) {
            loadTextureID = 0;
            sOpacity = 0;
        }

        //Fade minimap in/out
        if (loadTextureID == sLoadedTexTableID) {
            // @recomp: Make framerate independent
            sOpacity += (32/2) * gUpdateRate;
            if (sOpacity > 0xFF) {
                sOpacity = 0xFF;
            }
        } else {
            // @recomp: Make framerate independent
            sOpacity -= (32/2) * gUpdateRate;
            if (sOpacity < 0) {
                sOpacity = 0;
                if (sMapTile && (loadTextureID || sOpacity == 0)) {
                    tex_free(sMapTile);
                    sMapTile = NULL;
                    sLoadedTexTableID = 0;
                }
                if (loadTextureID) {
                    sMapTile = tex_load_deferred(loadTextureID);
                    sLoadedTexTableID = loadTextureID;
                }
            }
        }

        if (sMapTile && sOpacity) {
            // @recomp: Fullscreen scissor
            gEXPushScissor((*gdl)++);
            gEXSetScissorAlign((*gdl)++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, -SCREEN_WIDTH, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            gDPSetScissor((*gdl)++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            // @recomp: Align minimap to left
            gEXSetViewportAlign((*gdl)++, G_EX_ORIGIN_LEFT, 0, 0);
            gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, 0, 0, 0, 0);

            //Draw minimap tile
            rcp_screen_full_write(gdl, sMapTile, 
                    (MINIMAP_SCREEN_X + sOffsetX - sGridX),
                    (MINIMAP_SCREEN_Y + sOffsetY - sGridZ),
                    0, 0, sOpacity, SCREEN_WRITE_TRANSLUCENT);

            //Draw player marker
            rcp_screen_full_write(gdl, sMarkerPlayer, 
                    (MINIMAP_SCREEN_X - sGridX - (player->globalPosition.x - sLevelMaxX) * 0.025f) - 4.0f,
                    (MINIMAP_SCREEN_Y - sGridZ - (player->globalPosition.z - sLevelMaxZ) * 0.025f) - 4.0f,
                    0, 0, sOpacity, SCREEN_WRITE_TRANSLUCENT);

            //Draw sidekick marker (if the sidekick's somewhere inside the current map's extended bounds)
            if (sidekick != NULL) {
                if (
                    (sLevelMinX - BLOCKS_GRID_UNIT_HALF < sidekick->globalPosition.x) && (sidekick->globalPosition.x < sLevelMaxX + BLOCKS_GRID_UNIT_HALF) &&
                    (sLevelMinZ - BLOCKS_GRID_UNIT_HALF < sidekick->globalPosition.z) && (sidekick->globalPosition.z < sLevelMaxZ + BLOCKS_GRID_UNIT_HALF)
                ) {
                    rcp_screen_full_write(gdl, sMarkerSidekick, 
                        (MINIMAP_SCREEN_X - sGridX - (sidekick->globalPosition.x - sLevelMaxX) * 0.025f) - 4.0f,
                        (MINIMAP_SCREEN_Y - sGridZ - (sidekick->globalPosition.z - sLevelMaxZ) * 0.025f) - 4.0f,
                        0, 0, sOpacity, SCREEN_WRITE_TRANSLUCENT);
                }
            }

            // @recomp: Reset alignment
            gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
            gEXSetViewportAlign((*gdl)++, G_EX_ORIGIN_NONE, 0, 0);
            // @recomp: Reset scissor align
            gEXSetScissorAlign((*gdl)++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            gEXPopScissor((*gdl)++);
        }
    }
    return 0;
}
