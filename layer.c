#include "linked_list.h"
#include "lunarengine.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    float scrollOffsetX, scrollSpeedX;
    float scrollOffsetY, scrollSpeedY;
    float scaleW, scaleH;
    float prevScrollOffsetX, prevScrollSpeedX;
    float prevScrollOffsetY, prevScrollSpeedY;
    float prevScaleW, prevScaleH;
    LE_LayerType type;
    void* ptr;
    LE_LayerList* parent;
    struct {
        float camPosX;
        float camPosY;
        float prevCamPosX;
        float prevCamPosY;
    } cameraData;
} _LE_Layer;

typedef struct {
    void* params;
    CustomLayer callback;
} _LE_CustomLayer;

typedef DEFINE_LIST(_LE_Layer) _LE_LayerList;

LE_LayerList* LE_CreateLayerList() {
    struct LinkedList__LE_Layer* list = LE_LL_Create();
    list->value = malloc(sizeof(_LE_Layer));
    list->value->cameraData.camPosX = 0;
    list->value->cameraData.camPosY = 0;
    return (LE_LayerList*)list;
}

LE_Layer* LE_MakeLayer(LE_LayerList* layers, void* data, LE_LayerType type) {
    _LE_Layer* l = malloc(sizeof(_LE_Layer));
    l->scrollOffsetX = l->scrollOffsetY = 0;
    l->scrollSpeedX = l->scrollSpeedY = 1;
    l->scaleW = l->scaleH = 1;
    l->type = type;
    l->ptr = data;
    l->parent = (LE_LayerList*)LE_LL_Add(layers, l);
    return (LE_Layer*)l;
}

LE_Layer* LE_AddTilemapLayer(LE_LayerList* layers, LE_Tilemap* tilemap) {
    return LE_MakeLayer(layers, tilemap, LE_LayerType_Tilemap);
}

LE_Layer* LE_AddEntityLayer(LE_LayerList* layers, LE_EntityList* entities) {
    return LE_MakeLayer(layers, entities, LE_LayerType_Entity);
}

LE_Layer* LE_AddCustomLayer(LE_LayerList* layers, CustomLayer callback, void* params) {
    _LE_CustomLayer* layer = malloc(sizeof(_LE_CustomLayer));
    layer->callback = callback;
    layer->params = params;
    return LE_MakeLayer(layers, layer, LE_LayerType_Custom);
}

LE_Layer* LE_LayerGetByIndex(LE_LayerList* layers, int index) {
    if (index < 0) return NULL;
    _LE_LayerList* ll = (_LE_LayerList*)layers;
    for (int i = 0; i < index; i++) {
        ll = ll->next;
        if (!ll) return NULL;
    }
    return (LE_Layer*)ll->next->value;
}

void LE_MoveLayer(LE_Layer* layer, int index) {
    _LE_Layer* l = (_LE_Layer*)layer;
    _LE_LayerList* ll = (_LE_LayerList*)l->parent;
    if (ll->prev) ll->prev->next = ll->next;
    if (ll->next) ll->next->prev = ll->prev;
    _LE_LayerList* prev = ll->frst;
    for (int i = 0; i < index; i++) {
        if (!prev->next) break;
    }
    _LE_LayerList* next = prev->next;
    prev->next = ll;
    if (next) next->prev = ll;
    ll->prev = prev;
    ll->next = next;
}

int LE_IndexOfLayer(LE_Layer* layer) {
    _LE_Layer* l = (_LE_Layer*)layer;
    _LE_LayerList* ll = (_LE_LayerList*)l->parent;
    int counter = 0;
    while (ll->prev->prev) {
        counter++;
        ll = ll->prev;
    }
    return counter;
}

void LE_ScrollCamera(LE_LayerList* layers, float camX, float camY) {
    _LE_LayerList* ll = (_LE_LayerList*)layers;
    ll->value->cameraData.camPosX = camX;
    ll->value->cameraData.camPosY = camY;
}

void LE_GetCameraPos(LE_LayerList* layers, float* camX, float* camY) {
    _LE_LayerList* ll = (_LE_LayerList*)layers;
    if (camX) *camX = ll->value->cameraData.camPosX;
    if (camY) *camY = ll->value->cameraData.camPosY;
}

LE_LayerType LE_LayerGetType(LE_Layer* layer) {
    return ((_LE_Layer*)layer)->type;
}

void* LE_LayerGetDataPointer(LE_Layer* layer) {
    return ((_LE_Layer*)layer)->ptr;
}

void LE_UpdateLayerList(LE_LayerList* layers) {
    _LE_LayerList* ll = (_LE_LayerList*)layers;
    ll->value->cameraData.prevCamPosX = ll->value->cameraData.camPosX;
    ll->value->cameraData.prevCamPosY = ll->value->cameraData.camPosY;
    while (ll->next) {
        ll = ll->next;
        ll->value->prevScaleW = ll->value->scaleW;
        ll->value->prevScaleH = ll->value->scaleH;
        ll->value->prevScrollSpeedX = ll->value->scrollSpeedX;
        ll->value->prevScrollSpeedY = ll->value->scrollSpeedY;
        ll->value->prevScrollOffsetX = ll->value->scrollOffsetX;
        ll->value->prevScrollOffsetY = ll->value->scrollOffsetY;
    }
}

void LE_Draw(LE_LayerList* layers, int screenW, int screenH, float interpolation, LE_DrawList* dl) {
    for (int i = LE_LL_Size(layers) - 1; i >= 0; i--) {
        LE_DrawSingleLayer((LE_Layer*)LE_LL_Get(layers, i), screenW, screenH, interpolation, dl);
    }
}

static int sort_entities(const void* left, const void* right) {
    return (*(LE_Entity**)left)->drawPriority - (*(LE_Entity**)right)->drawPriority;
}

void LE_DrawSingleLayer(LE_Layer* layer, int screenW, int screenH, float interpolation, LE_DrawList* dl) {
    extern void LE_EntityGetPrevPosition(LE_Entity* entity, float* x, float* y);
    _LE_Layer* l = (_LE_Layer*)layer;
    _LE_LayerList* ll = ((_LE_LayerList*)l->parent)->frst;

    int tileW = 1, tileH = 1;
    if (l->type == LE_LayerType_Tilemap) {
        LE_Tilemap* tilemap = l->ptr;
        LE_Tileset* tileset = LE_TilemapGetTileset(tilemap);
        if (!tileset) return;
        LE_TilesetGetTileSize(tileset, &tileW, &tileH);
    }
    if (l->type == LE_LayerType_Entity) {
        LE_Tilemap* tilemap = LE_EntityGetTilemap(l->ptr);
        if (tilemap) {
            LE_Tileset* tileset = LE_TilemapGetTileset(tilemap);
            if (tileset) {
                LE_TilesetGetTileSize(tileset, &tileW, &tileH);
            }
        }
    }

    float camPosX = (ll->value->cameraData.camPosX - ll->value->cameraData.prevCamPosX) * interpolation + ll->value->cameraData.prevCamPosX;
    float camPosY = (ll->value->cameraData.camPosY - ll->value->cameraData.prevCamPosY) * interpolation + ll->value->cameraData.prevCamPosY;
    float scaleW = (l->prevScaleW - l->scaleW) * interpolation + l->prevScaleW;
    float scaleH = (l->prevScaleH - l->scaleH) * interpolation + l->prevScaleH;
    float scrollSpeedX = (l->prevScrollSpeedX - l->scrollSpeedX) * interpolation + l->prevScrollSpeedX;
    float scrollSpeedY = (l->prevScrollSpeedY - l->scrollSpeedY) * interpolation + l->prevScrollSpeedY;
    float scrollOffsetX = (l->prevScrollOffsetX - l->scrollOffsetX) * interpolation + l->prevScrollOffsetX;
    float scrollOffsetY = (l->prevScrollOffsetY - l->scrollOffsetY) * interpolation + l->prevScrollOffsetY;

    float offsetX = (camPosX * scrollSpeedX - screenW / 2.f) / tileW / scaleW + scrollOffsetX;
    float offsetY = (camPosY * scrollSpeedY - screenH / 2.f) / tileH / scaleH + scrollOffsetY;
    float tlx = offsetX - 1;
    float tly = offsetY - 1;
    float brx = offsetX + screenW / scaleW / tileW + 1;
    float bry = offsetY + screenH / scaleH / tileH + 1;
    switch (l->type) {
        case LE_LayerType_Tilemap: {
            LE_DrawPartialTilemap(l->ptr, -offsetX, -offsetY, tlx, tly, brx, bry, scaleW, scaleH, dl);
        } break;
        case LE_LayerType_Entity: {
            int index = 0;
            int num_ents = LE_NumEntities(l->ptr);
            LE_Entity* entities[num_ents];
            LE_EntityListIter* iter = LE_EntityListGetIter(l->ptr);
            while (iter) {
                entities[index++] = LE_EntityListGet(iter);
                iter = LE_EntityListNext(iter);
            }
            qsort(entities, num_ents, sizeof(*entities), sort_entities);
            for (int i = 0; i < num_ents; i++) {
                LE_Entity* entity = entities[i];
                float prevX, prevY;
                LE_EntityGetPrevPosition(entity, &prevX, &prevY);
                float x = (entity->posX - prevX) * interpolation + prevX;
                float y = (entity->posY - prevY) * interpolation + prevY;
                LE_DrawEntity(entity, (x - offsetX) * tileW * scaleW, (y - offsetY) * tileH * scaleH, scaleW, scaleH, dl);
            }
        } break;
        case LE_LayerType_Custom: {
            _LE_CustomLayer* custom = (_LE_CustomLayer*)l->ptr;
            custom->callback(dl, custom->params, offsetX, offsetY, scaleW, scaleH);
        } break;
    }
}

void LE_DisposeLayer(LE_Layer* layer) {
    _LE_Layer* l = (_LE_Layer*)layer;
    if (l->type == LE_LayerType_Custom) free(l->ptr);
    free(layer);
}

void LE_DestroyLayer(LE_Layer* layer) {
    _LE_LayerList* ll = ((_LE_LayerList*)((_LE_Layer*)layer)->parent)->frst;
    LE_LL_Remove(ll, layer);
    LE_DisposeLayer(layer);
}

void LE_DestroyLayerList(LE_LayerList* layers) {
    LE_LL_DeepFree(layers, (void(*)(void*))LE_DisposeLayer);
}

int LE_NumLayers(LE_LayerList* layers) {
    return LE_LL_Size(layers);
}

LE_LayerListIter* LE_LayerListGetIter(LE_LayerList* list) {
    return LE_LayerListNext((LE_LayerListIter*)list);
}

LE_LayerListIter* LE_LayerListNext(LE_LayerListIter* iter) {
    return (LE_LayerListIter*)((_LE_LayerList*)iter)->next;
}

LE_LayerListIter* LE_LayerListPrev(LE_LayerListIter* iter) {
    _LE_LayerList* ll = (_LE_LayerList*)iter;
    if (!ll->prev->prev) return NULL;
    return (LE_LayerListIter*)ll->prev;
}

LE_Layer* LE_LayerListGet(LE_LayerListIter* iter) {
    return (LE_Layer*)((_LE_LayerList*)iter)->value;
}
