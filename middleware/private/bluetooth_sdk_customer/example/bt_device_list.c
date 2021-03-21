#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "linkplay_bluetooth_interface.h"
#include "common.h"
#include "lp_list.h"
#include <sys/stat.h>
#include <sys/statfs.h>
#include <fcntl.h> /*open(),flag*/

#define BT_HISTORY_SAVE_PATH "/vendor/bsa/bt_history.json"

LP_LIST_HEAD(list_bt_device_history);

enum { PROFILE_TYPE_A2DP_SINK = 0, PROFILE_TYPE_A2DP_SOURCE };

typedef struct _BT_DEVICE_CLIENT {
    struct lp_list_head entry;
    BD_ADDR bd_addr; /* BD address peer device. */
    BD_NAME name;    /* Name of peer device. */
    int audio_path;
    char profile[32];
} BT_DEVICE_CLIENT;

void DeleteFileInStorage(char *file_path)
{
    remove(file_path);
    sync();
}

void MoveFileInStorage(char *old_path, char *new_path)
{
    rename(old_path, new_path);
    sync();
}

void RenameFileInStorage(char *oldname, char *newname)
{
    rename(oldname, newname);
    sync();
}
int GetFileSizeInStorage(const char *path)
{
    unsigned long filesize = -1;
    struct stat statbuff;
    if (stat(path, &statbuff) < 0) {
        return filesize;
    } else {
        filesize = statbuff.st_size;
    }
    return filesize;
}

char *GetValueCheckEmptry(char *input) { return (input == NULL) ? "" : input; }
struct lp_list_head *GetBtListHistoryHead(void) { return &list_bt_device_history; }

void bt_devicelist_add_entry_sort(BD_ADDR bd_addr, char *name, char *profile,
                                  struct lp_list_head *headlist)
{
    struct lp_list_head *pos = NULL, *tmp = NULL;
    BT_DEVICE_CLIENT *item;

    lp_list_for_each_safe(pos, tmp, headlist)
    {
        item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
        if (memcmp(bd_addr, item->bd_addr, BD_ADDR_LEN) == 0) {
            lp_list_del(pos);
            free(item);
        }
    }

    item = (BT_DEVICE_CLIENT *)malloc(sizeof(BT_DEVICE_CLIENT));
    memset(item, 0x0, sizeof(BT_DEVICE_CLIENT));
    memcpy(item->bd_addr, bd_addr, BD_ADDR_LEN);
    memcpy(item->name, name, BD_NAME_LEN);
    if (profile)
        strncpy(item->profile, profile, 32);
    lp_list_add(&item->entry, headlist);

    bt_save_history();
}
void bt_devicelist_add_entry(char *bd_addr, char *name, char *profile,
                             struct lp_list_head *headlist)
{
    struct lp_list_head *pos;
    int found = 0;
    BT_DEVICE_CLIENT *item;
    lp_list_for_each(pos, headlist)
    {
        item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
        if (memcmp(bd_addr, item->bd_addr, BD_ADDR_LEN) == 0) {
            memcpy(item->name, name, BD_NAME_LEN);
            if (profile)
                strncpy(item->profile, profile, 32);
            found = 1;
            break;
        }
    }
    if (!found) {
        BT_DEVICE_CLIENT *item = (BT_DEVICE_CLIENT *)malloc(sizeof(BT_DEVICE_CLIENT));
        memset(item, 0x0, sizeof(BT_DEVICE_CLIENT));
        memcpy(item->bd_addr, bd_addr, BD_ADDR_LEN);
        memcpy(item->name, name, BD_NAME_LEN);
        if (profile)
            strncpy(item->profile, profile, 32);
        lp_list_add_tail(&item->entry, headlist);
    }
}

void bt_device_list_clear(struct lp_list_head *headlist)
{
    struct lp_list_head *pos = NULL, *tmp = NULL;

    lp_list_for_each_safe(pos, tmp, headlist)
    {
        BT_DEVICE_CLIENT *item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
        lp_list_del(pos);
        free(item);
    }

    return;
}

char *bt_device_convert_to_string(struct lp_list_head *headlist, UINT8 *btaddr, int ct, int status)
{
    char tmp[32];
    char *utf8_name = NULL;
    int utf8_name_length = 0;
    json_object *json_root = NULL, *json_array = NULL;
    ;
    struct lp_list_head *pos;
    BT_DEVICE_CLIENT *item = NULL;
    char *strDeviceLists = NULL;
    int connect = 0;
    json_root = json_object_new_object();

    json_object_object_add(json_root, "num", json_object_new_int(lp_list_number(headlist)));
    json_object_object_add(json_root, "scan_status", json_object_new_int(status));
    if (lp_list_number(headlist) > 0) {
        json_array = json_object_new_array();
        lp_list_for_each(pos, headlist)
        {
            connect = 0;
            json_object *json = json_object_new_object();
            item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
            printf("item->name %s \r\n", item->name);
            //   convert_to_utf8(item->name, strlen(item->name), NULL, &utf8_name_length);
            //   utf8_name = (char *)malloc(utf8_name_length + 1);
            //   convert_to_utf8(item->name, strlen(item->name), utf8_name, &utf8_name_length);
            //   utf8_name[utf8_name_length] = 0;
            json_object_object_add(json, "name", json_object_new_string(item->name));
            //   free(utf8_name);
            bt_address_to_string(tmp, &item->bd_addr);
            if (ct && btaddr && memcmp(btaddr, item->bd_addr, BD_ADDR_LEN) == 0) {
                connect = 1;
            }
            json_object_object_add(json, "ad", json_object_new_string(tmp));
            json_object_object_add(json, "ct", json_object_new_int(connect));
            json_object_object_add(json, "role", json_object_new_string(item->profile));
            json_object_array_add(json_array, json);
        }
        json_object_object_add(json_root, "list", json_array);
    }

    strDeviceLists = strdup(json_object_to_json_string(json_root));
    printf("strDeviceLists %s \r\n", strDeviceLists);

    json_object_put(json_root);

    return strDeviceLists;
}

int bt_save_history(void)
{
    char *strtmp = bt_device_convert_to_string(&list_bt_device_history, NULL, 0, 0);
    if (strtmp) {
        DeleteFileInStorage(BT_HISTORY_SAVE_PATH);
        FILE *file = fopen(BT_HISTORY_SAVE_PATH, "w+");
        if (file) {
            fputs(strtmp, file);
            fflush(file);
            fclose(file);
            sync();
        }
        free(strtmp);
    }
}
int bt_load_history(void)
{
    int file_length = 0;
    char *buffer = 0;
    printf("Load bt history \r\n");

    file_length = GetFileSizeInStorage(BT_HISTORY_SAVE_PATH);

    if (file_length > 0) {
        FILE *file = fopen(BT_HISTORY_SAVE_PATH, "r");
        if (!file)
            return -1;
        buffer = (char *)malloc(file_length + 1);
        if (buffer == NULL) {
            fclose(file);
            return -1;
        }
        fread(buffer, 1, file_length, file);
        buffer[file_length] = 0;
        fclose(file);
    }

    if (buffer != NULL) {
        int i = 0;
        int file_length = 0;
        int ret = -1;
        int nums = 0;
        json_object *new_obj = 0, *list_obj = 0, *row_obj = 0;
        new_obj = json_tokener_parse(buffer);
        if (is_error(new_obj)) {
            printf("json_tokener_parse failed!! \r\n");
        }
        list_obj = json_object_object_get(new_obj, "list");
        if (list_obj) {
            nums = json_object_array_length(list_obj);
            printf("nums = %d \r\n", nums);
            bt_device_list_clear(&list_bt_device_history);
            for (i = 0; i < nums; i++) {
                char *name = 0, *addr = 0, *role;
                BT_DEVICE_CLIENT *item = (BT_DEVICE_CLIENT *)malloc(sizeof(BT_DEVICE_CLIENT));
                memset(item, 0x0, sizeof(BT_DEVICE_CLIENT));
                row_obj = json_object_array_get_idx(list_obj, i);
                name = JSON_GET_STRING_FROM_OBJECT(row_obj, "name");
                if (name)
                    memcpy(item->name, name, BD_NAME_LEN);
                addr = JSON_GET_STRING_FROM_OBJECT(row_obj, "ad");
                if (addr)
                    string_to_bt_address(addr, &item->bd_addr);
                role = JSON_GET_STRING_FROM_OBJECT(row_obj, "role");
                if (role)
                    strncpy(item->profile, role, 32);
                item->audio_path = JSON_GET_INT_FROM_OBJECT(row_obj, "audio path");
                lp_list_add_tail(&item->entry, &list_bt_device_history);
            }
        }
        json_object_put(new_obj);

        free(buffer);
    }

    return 0;
}

int bt_check_history_by_addr(UINT8 *btaddr)
{
    struct lp_list_head *pos;
    BT_DEVICE_CLIENT *item;
    BD_ADDR bd_addr;
    string_to_bt_address(btaddr, &bd_addr);

    if (lp_list_number(&list_bt_device_history) <= 0) {
        return 0;
    }

    lp_list_for_each(pos, &list_bt_device_history)
    {
        item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);

        if (memcmp(item->bd_addr, bd_addr, BD_ADDR_LEN) == 0) {
            return 1;
        }
    }

    return 0;
}
char *bt_get_name_by_addr(UINT8 *btaddr)
{
    struct lp_list_head *pos;
    BT_DEVICE_CLIENT *item;
    BD_ADDR bd_addr;
    string_to_bt_address(btaddr, &bd_addr);

    if (lp_list_number(&list_bt_device_history) <= 0) {
        return NULL;
    }

    lp_list_for_each(pos, &list_bt_device_history)
    {
        item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);

        if (memcmp(item->bd_addr, bd_addr, BD_ADDR_LEN) == 0) {
            return (char *)item->name;
        }
    }

    return NULL;
}
char *bt_get_profile_by_addr(BD_ADDR bd_addr, struct lp_list_head *headlist)
{
    struct lp_list_head *pos;
    BT_DEVICE_CLIENT *item;

    if (lp_list_number(headlist) <= 0) {
        return NULL;
    }

    lp_list_for_each(pos, headlist)
    {
        item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
        if (memcmp(item->bd_addr, bd_addr, BD_ADDR_LEN) == 0) {
            return (char *)item->profile;
        }
    }

    return NULL;
}

int bt_delete_history_by_addr(UINT8 *btaddr)
{
    BT_DEVICE_CLIENT *item;
    int ret = -1;
    struct lp_list_head *pos = NULL, *tmp = NULL;

    if (lp_list_number(&list_bt_device_history) <= 0) {
        return ret;
    }

    printf("bt_delete_history_by_addr ++\r\n");
    lp_list_for_each_safe(pos, tmp, &list_bt_device_history)
    {
        item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);

        if (memcmp(item->bd_addr, btaddr, BD_ADDR_LEN) == 0) {
            printf("delete btaddr \r\n");
            lp_list_del(pos);
            free(item);
            ret = 0;
            break;
        }
    }
    printf("bt_delete_history_by_addr --\r\n");

    if (!ret)
        bt_save_history();

    return ret;
}
int bt_check_history_full(int max_number, char *profile, char *inaddr, char *addr)
{
    struct lp_list_head *pos = NULL;
    BT_DEVICE_CLIENT *item;
    int count = 0, ret = 0;

    if (bt_check_history_by_addr(inaddr) == 1) {
        return 0;
    }

    lp_list_for_each(pos, &list_bt_device_history)
    {
        item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
        if (strncmp(profile, item->profile, 32) == 0) {
            ++count;
            if (count >= max_number) {
                bt_address_to_string(addr, item->bd_addr);
                ret = 1;
                break;
            }
        }
    }

    return ret;
}

int bt_get_last_connection_addr(BD_ADDR bd_addr, char *profile)
{
    struct lp_list_head *pos = NULL;
    BT_DEVICE_CLIENT *item;
    int count = 0, ret = 0;

    lp_list_for_each(pos, &list_bt_device_history)
    {
        item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
        if (strncmp(profile, item->profile, 32) == 0) {
            memcpy(bd_addr, item->bd_addr, BD_ADDR_LEN);
            ret = 1;
            break;
        }
    }

    return ret;
}
int bt_get_next_connection_addr(BD_ADDR bd_addr, BD_ADDR bd_next_addr, char *profile)
{
    struct lp_list_head *pos = NULL;
    BT_DEVICE_CLIENT *item;
    int ret = 0;

    lp_list_for_each(pos, &list_bt_device_history)
    {
        item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
        if (strncmp(profile, item->profile, 32) == 0 &&
            memcmp(item->bd_addr, bd_addr, BD_ADDR_LEN) == 0) {
            for (pos = pos->next;; pos = pos->next) {
                if (pos != &list_bt_device_history) {
                    item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
                    if (strncmp(profile, item->profile, 32) == 0) {
                        memcpy(bd_addr, item->bd_addr, BD_ADDR_LEN);
                        ret = 1;
                        break;
                    }
                }
            }
        }
    }

    return ret;
}
