#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/* Open/close world handle */
typedef struct WiWorld WiWorld;
WiWorld* wi_open(const char* world_path);
void wi_close(WiWorld* world);

/* C struct API */
typedef struct {
    int32_t slot;
    char* name;
    int32_t count;
    int32_t damage;
    char* tag_json;
} WiItem;

typedef struct {
    WiItem* items;
    int32_t count;
} WiItemArray;

typedef struct {
    WiItemArray inventory;
    WiItemArray armor;
    WiItem* offhand;
} WiInventoryResult;

WiInventoryResult* wi_get_inventory(WiWorld* world, const char* player_key);
void wi_free_inventory(WiInventoryResult* result);
char** wi_list_player_keys(WiWorld* world, int32_t* out_count);
void wi_free_string_array(char** arr, int32_t count);

/* JSON string API (simpler, opens/closes DB each call) */
char* wi_get_inventory_json(const char* world_path, const char* player_key);
void wi_free_string(char* s);

/* Encoded inventory API (pre-serialized binary NBT for inventoryui) */
typedef struct {
    int32_t slot;
    char* type_id;
    int32_t count;
    int32_t damage;
    uint8_t* nbt_bytes;
    int32_t nbt_len;
} WiEncodedItem;

typedef struct {
    WiEncodedItem* items;
    int32_t count;
} WiEncodedInventory;

WiEncodedInventory* wi_get_encoded_inventory(const char* world_path, const char* player_key);
void wi_free_encoded_inventory(WiEncodedInventory* inv);

#ifdef __cplusplus
}
#endif
