#include <stdlib.h>
#include <string.h>

#include "collision.h"
#include "linked_list.h"
#include "lunarengine.h"

typedef struct {
    char* name;
    LE_EntityProperty value;
} _LE_EntityProperty;

typedef DEFINE_LIST(EntityTextureCallback) _LE_TexCallbackList;
typedef DEFINE_LIST(EntityUpdateCallback) _LE_UpdateCallbackList;
typedef DEFINE_LIST(EntityCollisionCallback) _LE_CollisionCallbackList;
typedef DEFINE_LIST(_LE_EntityProperty) _LE_EntityPropList;

typedef struct {
    _LE_TexCallbackList* textureCallbacks;
    _LE_UpdateCallbackList* updateCallbacks;
    _LE_CollisionCallbackList* collisionCallbacks;
    _LE_EntityPropList* properties;
    float width, height;
    int defaultDrawPriority;
    LE_EntityFlags flags;
} _LE_EntityBuilder;

typedef struct {
    float posX, posY;
    float velX, velY;
    float width, height;
    int drawPriority;
    LE_EntityFlags flags;
    float prevPosX, prevPosY;
    float lastDrawnX, lastDrawnY;
    bool deleted;
    LE_Entity* platform;
    _LE_EntityPropList* properties;
    _LE_TexCallbackList* textureCallbacks;
    _LE_UpdateCallbackList* updateCallbacks;
    _LE_CollisionCallbackList* collisionCallbacks;
    LE_EntityList* parent;
    LE_Tilemap* tilemap;
} _LE_Entity;

typedef DEFINE_LIST(_LE_Entity) _LE_EntityList;

void _LE_AddPropertyToList(_LE_EntityPropList* list, LE_EntityProperty property, const char* name) {
    _LE_EntityPropList* curr = list;
    while (curr->next) {
        curr = curr->next;
        if (strcmp(curr->value->name, name) == 0) {
            curr->value->value = property;
            return;
        }
    }
    _LE_EntityProperty* p = malloc(sizeof(_LE_EntityProperty));
    p->name = strdup(name);
    p->value = property;
    LE_LL_Add(list, p);
}

LE_EntityBuilder* LE_CreateEntityBuilder() {
    _LE_EntityBuilder* builder = malloc(sizeof(_LE_EntityBuilder));
    memset(builder, 0, sizeof(_LE_EntityBuilder));
    builder->textureCallbacks = LE_LL_Create();
    builder->updateCallbacks = LE_LL_Create();
    builder->collisionCallbacks = LE_LL_Create();
    builder->properties = LE_LL_Create();
    return (LE_EntityBuilder*)builder;
}

void LE_EntityBuilderAddTextureCallback(LE_EntityBuilder* builder, EntityTextureCallback callback) {
    LE_LL_Add(((_LE_EntityBuilder*)builder)->textureCallbacks, callback);
}

void LE_EntityBuilderAddUpdateCallback(LE_EntityBuilder* builder, EntityUpdateCallback callback) {
    LE_LL_Add(((_LE_EntityBuilder*)builder)->updateCallbacks, callback);
}

void LE_EntityBuilderAddCollisionCallback(LE_EntityBuilder* builder, EntityCollisionCallback callback) {
    LE_LL_Add(((_LE_EntityBuilder*)builder)->collisionCallbacks, callback);
}

void LE_EntityBuilderSetHitboxSize(LE_EntityBuilder* builder, float width, float height) {
    _LE_EntityBuilder* eb = (_LE_EntityBuilder*)builder;
    eb->width = width;
    eb->height = height;
}

void LE_EntityBuilderSetFlags(LE_EntityBuilder* builder, LE_EntityFlags flags) {
    ((_LE_EntityBuilder*)builder)->flags = flags;
}

void LE_EntityBuilderSetDrawPriority(LE_EntityBuilder* builder, int drawPriority) {
    ((_LE_EntityBuilder*)builder)->defaultDrawPriority = drawPriority;
}

void LE_EntityBuilderAppendFlags(LE_EntityBuilder* builder, LE_EntityFlags flags) {
    ((_LE_EntityBuilder*)builder)->flags |= flags;
}

void LE_EntityBuilderClearFlags(LE_EntityBuilder* builder, LE_EntityFlags flags) {
    ((_LE_EntityBuilder*)builder)->flags &= ~flags;
}

void LE_EntityBuilderSetProperty(LE_EntityBuilder* builder, LE_EntityProperty property, const char* name) {
    _LE_AddPropertyToList(((_LE_EntityBuilder*)builder)->properties, property, name);
}

void LE_DestroyEntityBuilder(LE_EntityBuilder* builder) {
    _LE_EntityBuilder* b = (_LE_EntityBuilder*)builder;
    LE_LL_Free(b->textureCallbacks);
    LE_LL_Free(b->updateCallbacks);
    LE_LL_Free(b->collisionCallbacks);
    LE_LL_DeepFree(b->properties, free);
    free(builder);
}

LE_EntityList* LE_CreateEntityList() {
    _LE_EntityList* el = LE_LL_Create();
    el->value = malloc(sizeof(_LE_Entity));
    el->value->parent = (LE_EntityList*)el;
    el->value->tilemap = NULL;
    return (LE_EntityList*)el;
}

LE_Entity* LE_CreateEntity(LE_EntityList* list, LE_EntityBuilder* builder, float x, float y) {
    _LE_Entity* entity = malloc(sizeof(_LE_Entity));
    _LE_EntityBuilder* b = (_LE_EntityBuilder*)builder;
    entity->posX = x;
    entity->posY = y;
    entity->velX = 0;
    entity->velY = 0;
    entity->width = b->width;
    entity->height = b->height;
    entity->flags = b->flags;
    entity->deleted = false;
    entity->platform = NULL;
    entity->drawPriority = b->defaultDrawPriority;
    entity->textureCallbacks = b->textureCallbacks;
    entity->updateCallbacks = b->updateCallbacks;
    entity->collisionCallbacks = b->collisionCallbacks;
    entity->properties = LE_LL_Create();
    entity->parent = LE_LL_Add(list, entity);
    _LE_EntityPropList* curr = b->properties;
    while (curr->next) {
        curr = curr->next;
        _LE_AddPropertyToList(entity->properties, curr->value->value, curr->value->name);
    }
    return (LE_Entity*)entity;
}

LE_Entity* LE_EntityGetPlatform(LE_Entity* entity) {
    return ((_LE_Entity*)entity)->platform;
}

void LE_EntitySetPlatform(LE_Entity* entity, LE_Entity* platform) {
    ((_LE_Entity*)entity)->platform = platform;
}

void LE_EntityAssignTilemap(LE_EntityList* list, LE_Tilemap* tilemap) {
    ((_LE_EntityList*)list)->value->tilemap = tilemap;
}

void LE_EntitySetProperty(LE_Entity* entity, LE_EntityProperty property, const char* name) {
    _LE_AddPropertyToList(((_LE_Entity*)entity)->properties, property, name);
}

void LE_EntityDelProperty(LE_Entity* entity, const char* name) {
    _LE_EntityPropList* prop = ((_LE_Entity*)entity)->properties;
    while (prop->next) {
        prop = prop->next;
        _LE_EntityProperty* value = prop->value;
        if (strcmp(value->name, name) == 0) {
            LE_LL_Remove(prop->frst, value);
            free(value->name);
            free(value);
            return;
        }
    }
}

bool LE_EntityGetProperty(LE_Entity* entity, LE_EntityProperty* property, const char* name) {
    _LE_EntityPropList* prop = ((_LE_Entity*)entity)->properties;
    while (prop->next) {
        prop = prop->next;
        if (strcmp(prop->value->name, name) == 0) {
            if (property) memcpy(property, &prop->value->value, sizeof(LE_EntityProperty));
            return true;
        }
    }
    return false;
}

int LE_EntityNumProperties(LE_Entity* entity) {
    return LE_LL_Size(((_LE_Entity*)entity)->properties);
}

LE_EntityProperty LE_EntityGetPropertyOrDefault(LE_Entity *entity, LE_EntityProperty def, const char *name) {
    LE_EntityGetProperty(entity, &def, name);
    return def;
}

const char* LE_EntityGetPropertyKey(LE_Entity* entity, int index) {
    return((_LE_EntityProperty*)LE_LL_Get(((_LE_Entity*)entity)->properties, index))->name;
}

void LE_EntityChangeLists(LE_Entity* entity, LE_EntityList* destlist) {
    LE_LL_Remove(((_LE_Entity*)entity)->parent, entity);
    LE_LL_Add(destlist, entity);
    ((_LE_Entity*)entity)->parent = destlist;
}

void LE_EntityCollision(LE_Entity* entity, LE_Entity* collider) {
    _LE_CollisionCallbackList* collision = ((_LE_Entity*)entity)->collisionCallbacks;
    if (!collision) return;
    while (collision->next) {
        collision = collision->next;
        ((EntityCollisionCallback)collision->value)(entity, collider);
    }
}

void LE_UpdateEntities(LE_EntityList* entities, float delta_time) {
    _LE_EntityList* e = (_LE_EntityList*)entities;
    _LE_EntityList* curr = e;
    while (curr->next) {
        curr = curr->next;
        LE_UpdateEntity((LE_Entity*)curr->value, delta_time);
    }
    curr = e->next;
    while (curr) {
        _LE_EntityList* next = curr->next;
        if (curr->value->deleted) LE_DestroyEntity((LE_Entity*)curr->value);
        curr = next;
    }
}

void LE_UpdateEntity(LE_Entity* entity, float delta_time) {
    _LE_Entity* e = (_LE_Entity*)entity;
    _LE_UpdateCallbackList* update = e->updateCallbacks;
    if (e->deleted) return;
    while (update->next) {
        update = update->next;
        ((EntityUpdateCallback)update->value)(entity);
    }
    e->prevPosX = e->posX;
    e->prevPosY = e->posY;
    e->posY += e->velY * delta_time;
    LE_RunCollisionY(entity);
    e->posX += e->velX * delta_time;
    LE_RunCollisionX(entity);
}

void LE_EntityGetPrevPosition(LE_Entity* entity, float* x, float* y) {
    if (x) *x = ((_LE_Entity*)entity)->prevPosX;
    if (y) *y = ((_LE_Entity*)entity)->prevPosY;
}

void LE_DrawEntity(LE_Entity* entity, float x, float y, float scaleW, float scaleH, LE_DrawList* dl) {
    _LE_Entity* e = (_LE_Entity*)entity;
    _LE_TexCallbackList* tex = e->textureCallbacks;
    void* texture = NULL;
    float width, height;
    int srcX, srcY, srcW, srcH;
    int absw, absh;
    if (e->deleted) return;
    while (tex->next) {
        tex = tex->next;
        texture = ((EntityTextureCallback)tex->value)(entity, &width, &height, &srcX, &srcY, &srcW, &srcH);
        if (texture) break;
    }
    if (!texture) return;
    absw = width  * (width  < 0 ? -1 : 1);
    absh = height * (height < 0 ? -1 : 1);
    e->lastDrawnX = x - (absw * scaleW) / 2;
    e->lastDrawnY = y - (absh * scaleH);
    LE_DrawListAppend(dl, texture, e->lastDrawnX, e->lastDrawnY, width * scaleW, height * scaleH, srcX, srcY, srcW, srcH);
}

void LE_EntityLastDrawnPos(LE_Entity* entity, float* x, float* y) {
    _LE_Entity* e = (_LE_Entity*)entity;
    if (x) *x = e->lastDrawnX;
    if (y) *y = e->lastDrawnY;
}

void LE_DeleteEntity(LE_Entity* entity) {
    ((_LE_Entity*)entity)->deleted = true;
}

void LE_DestroyEntity(LE_Entity* entity) {
    _LE_Entity* e = (_LE_Entity*)entity;
    if ((void*)e->parent != (void*)((_LE_EntityList*)e->parent)->frst) {
        LE_LL_DeepFree(e->properties, free);
        LE_LL_Remove(((_LE_EntityList*)e->parent)->frst, entity);
    }
    free(entity);
}

void LE_DestroyEntityInner(LE_Entity* entity) {
    _LE_Entity* e = (_LE_Entity*)entity;
    if ((void*)e->parent != (void*)((_LE_EntityList*)e->parent)->frst) {
        LE_LL_DeepFree(e->properties, free);
    }
    free(entity);
}

void LE_DestroyEntityList(LE_EntityList* list) {
    LE_LL_DeepFree(list, (void(*)(void*))LE_DestroyEntityInner);
}

int LE_NumEntities(LE_EntityList* list) {
    return LE_LL_Size(list);
}

LE_EntityList* LE_EntityGetList(LE_Entity* entity) {
    return (LE_EntityList*)((_LE_EntityList*)((_LE_Entity*)entity)->parent)->frst;
}

LE_Tilemap* LE_EntityGetTilemap(LE_EntityList* list) {
    return ((_LE_EntityList*)list)->value->tilemap;
}

LE_EntityListIter* LE_EntityListGetIter(LE_EntityList* list) {
    return LE_EntityListNext((LE_EntityListIter*)list);
}

LE_EntityListIter* LE_EntityListNext(LE_EntityListIter* iter) {
    return (LE_EntityListIter*)((_LE_EntityList*)iter)->next;
}

LE_EntityListIter* LE_EntityListPrev(LE_EntityListIter* iter) {
    _LE_EntityList* ll = (_LE_EntityList*)iter;
    if (!ll->prev->prev) return NULL;
    return (LE_EntityListIter*)ll->prev;
}

LE_Entity* LE_EntityListGet(LE_EntityListIter* iter) {
    return (LE_Entity*)((_LE_EntityList*)iter)->value;
}
