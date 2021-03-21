#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <json.h>

#include "alexa_debug.h"
#include "alexa_products_macro.h"
#include "lpcrypto.h"

void free_alexa_device_info(AlexaDeviceInfo *p)
{
    if (!p)
        return;

    if (p->projectName) {
        free(p->projectName);
        p->projectName = NULL;
    }

    if (p->deviceName) {
        free(p->deviceName);
        p->deviceName = NULL;
    }

    if (p->clientId) {
        free(p->clientId);
        p->clientId = NULL;
    }

    if (p->clientSecret) {
        free(p->clientSecret);
        p->clientSecret = NULL;
    }

    if (p->serverUrl) {
        free(p->serverUrl);
        p->serverUrl = NULL;
    }

    if (p->DTID) {
        free(p->DTID);
        p->DTID = NULL;
    }
}

static int parse_products(char *buf, char *device_name, AlexaDeviceInfo *result)
{
    int i = 0;
    char *p = NULL;
    json_object *root_obj = NULL;
    json_object *products_obj = NULL;
    json_object *product_data = NULL;
    json_object *tmp = NULL;
    int obj_num = 0;
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "(%p, %s, %p)\n", buf, device_name, result);
    if (!result)
        return -1;

    root_obj = json_tokener_parse(buf);
    if (!root_obj) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_tokener_parse root failed\n");
        return -1;
    }
    products_obj = json_object_object_get(root_obj, JSON_KEY_PRODUCTS);
    if (!products_obj || (obj_num = json_object_array_length(products_obj)) < 1 ||
        !json_object_array_get_idx(products_obj, 0)) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_object_object_get [%s] failed\n", JSON_KEY_PRODUCTS);
        goto exit;
    }

    for (i = 0; i < obj_num; ++i) {
        free_alexa_device_info(result);
        tmp = json_object_array_get_idx(products_obj, i);
        if (!tmp)
            continue;

        // local project name
        product_data = json_object_object_get(tmp, JSON_KEY_PROJECT_NAME);
        if (!product_data)
            continue;
        p = (char *)json_object_get_string(product_data);
        if (!p || strcmp(p, device_name))
            continue;
        result->projectName = strdup(p);

        // alexa device name
        product_data = json_object_object_get(tmp, JSON_KEY_DEVICE_NAME);
        if (!product_data)
            continue;
        p = (char *)json_object_get_string(product_data);
        if (!p)
            continue;
        result->deviceName = strdup(p);

        // alexa clientId
        product_data = json_object_object_get(tmp, JSON_KEY_CLIENT_ID);
        if (!product_data)
            continue;

        p = (char *)json_object_get_string(product_data);
        if (!p)
            continue;
        result->clientId = strdup(p);

        // alexa secretId
        product_data = json_object_object_get(tmp, JSON_KEY_CLIENT_SECRET);
        if (!product_data)
            continue;

        p = (char *)json_object_get_string(product_data);
        if (!p)
            continue;
        result->clientSecret = strdup(p);

        // server url
        product_data = json_object_object_get(tmp, JSON_KEY_SERVER_URL);
        if (!product_data)
            continue;

        p = (char *)json_object_get_string(product_data);
        if (!p)
            continue;
        result->serverUrl = strdup(p);
        // alexa DTID
        product_data = json_object_object_get(tmp, JSON_KEY_DTID);
        if (product_data) {
            p = (char *)json_object_get_string(product_data);
            if (p)
                result->DTID = strdup(p);
        }
        break;
    }

exit:
    json_object_put(root_obj);
    if (!result->deviceName || !result->clientId || !result->clientSecret || !result->serverUrl) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "read cfg failed\n");
        free_alexa_device_info(result);
        return -1;
    }
    return 0;
}

int get_alexa_device_info(char *path, char *device_name, AlexaDeviceInfo *result)
{
    struct stat fs_stat;
    unsigned char *buf_dec = NULL;
    int ret = -1;
    lp_aes_cbc_ctx_t aes_cbc_param;
    unsigned char aes_key[16] = {0x80, 0x6a, 0x17, 0x95, 0xf7, 0x30, 0xd3, 0xf9,
                                 0x13, 0x2b, 0x2d, 0x92, 0xe8, 0x37, 0xf1, 0x7e};
    unsigned char aes_iv[16] = {0x88, 0x2d, 0xc7, 0x4e, 0xdb, 0x9a, 0x22, 0x85,
                                0xd3, 0x42, 0x7b, 0x73, 0x60, 0xb8, 0x8b, 0x01};
    char *tmp_file = "/tmp/aes_dec_alexa_products.json";
    FILE *file = NULL;
    unsigned char buff_src[2048] = {0};
    int read_len = 0;

    memset(&aes_cbc_param, 0, sizeof(lp_aes_cbc_ctx_t));
    memcpy(aes_cbc_param.key, aes_key, 16);
    memcpy(aes_cbc_param.iv, aes_iv, 16);
    aes_cbc_param.key_bits = 16 * 8;
    aes_cbc_param.padding_mode = LP_CIPHER_PADDING_MODE_PKCS7;
    aes_cbc_param.is_base64_format = 0;
    ret = lp_aes_cbc_decrypt_file(path, tmp_file, &aes_cbc_param);
    if (ret != 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "file %s decrypt error\n", path);
        return -1;
    }
    memset((void *)&fs_stat, 0, sizeof(fs_stat));
    if (stat(tmp_file, &fs_stat) || fs_stat.st_size <= 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "file %s state error\n", path);
        return -1;
    }
    file = fopen(tmp_file, "rb");
    if (!file) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "open file %s failed\n", tmp_file);
        return -1;
    }
    buf_dec = (unsigned char *)malloc(fs_stat.st_size + 1);
    if (!buf_dec) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "malloc error\n");
        fclose(file);
        return -1;
    }
    unsigned char *p_write = buf_dec;
    while (1) {
        read_len = fread(buff_src, 1, 2048, file);
        if (read_len <= 0)
            break;
        strncpy((char *)p_write, (char *)buff_src, read_len);
        p_write += read_len;
    }
    ret = parse_products((char *)buf_dec, device_name, result);
    free(buf_dec);
    fclose(file);
    remove(tmp_file);
    sync();
    return ret;
}

#ifdef X86
int main(int argc, char **argv)
{
    AlexaDeviceInfo result;
    int ret = -1;
    if (argc != 3) {
        fprintf(stderr, "Usage: %s path device_name\n", argv[0]);
        return -1;
    }

    memset((void *)&result, 0, sizeof(result));
    ret = get_alexa_device_info(argv[1], argv[2], &result);
    if (!ret) {
        printf("project:%s\n", result.projectName);
        printf("device_name:%s\n", result.deviceName);
        printf("client_id:%s\n", result.clientId);
        printf("client_secret:%s\n", result.clientSecret);
        printf("server_url:%s\n", result.serverUrl);
        printf("DTID :%s\n", result.DTID);
        free_alexa_device_info(&result);
    } else {
        fprintf(stderr, "parse product %s from %s failed\n", argv[2], argv[1]);
        return -1;
    }
    return 0;
}
#endif
