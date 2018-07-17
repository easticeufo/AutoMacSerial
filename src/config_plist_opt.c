#include "base_fun.h"

#define TAG_DICT "<dict>"
#define TAG_DICT_END "</dict>"
#define TAG_KEY "<key>"
#define TAG_KEY_END "</key>"
#define TAG_STRING "<string>"
#define TAG_STRING_END "</string>"
#define TAG_INT "<integer>"
#define TAG_INT_END "</integer>"
#define TAG_TRUE "<true/>"
#define TAG_FALSE "<false/>"

#define KEY_VALUE_TYPE_UNKNOWN 0
#define KEY_VALUE_TYPE_INT 1
#define KEY_VALUE_TYPE_BOOL 2
#define KEY_VALUE_TYPE_STRING 3

typedef struct KEY_VALUE_NODE
{
	const INT8 *p_key;
    INT32 key_len;
    INT32 value_type;
    INT32 int_value;
    BOOL bool_value;
	INT8 *p_string_value;
    INT32 string_value_len;
	BOOL need_free;
    struct KEY_VALUE_NODE *p_next;
}KEY_VALUE_NODE;

static INT8 *p_config_plist_read_buffer = NULL;
static KEY_VALUE_NODE *p_key_value_root = NULL;

static KEY_VALUE_NODE *parse_config_plist(const INT8 *p_buffer)
{
    const INT8 *ptr = NULL;
    const INT8 *ptr_next = NULL;
    const INT8 *ptr_end = NULL;
    KEY_VALUE_NODE *p_root = NULL;
    KEY_VALUE_NODE *p_node = NULL;
    KEY_VALUE_NODE *p_prev = NULL;

    ptr = strstr(p_buffer, "<key>SMBIOS</key>");
    if (NULL == ptr)
    {
        DEBUG_PRINT(DEBUG_WARN, "config.plist format error");
        return NULL;
    }

    ptr = strstr(ptr, TAG_DICT);
    if (NULL == ptr)
    {
        DEBUG_PRINT(DEBUG_WARN, "config.plist format error");
        return NULL;
    }

    while (ptr != NULL && (ptr = strstr(ptr, TAG_KEY)) != NULL && (ptr_end = strstr(ptr, TAG_KEY_END)) != NULL)
    {
        p_node = (KEY_VALUE_NODE *)malloc(sizeof(KEY_VALUE_NODE));
        memset(p_node, 0, sizeof(KEY_VALUE_NODE));
        p_node->p_key = ptr + strlen(TAG_KEY);
        p_node->key_len = ptr_end - p_node->p_key;

        ptr_end = ptr_end + strlen(TAG_KEY_END);
        ptr_next = strstr(ptr_end, TAG_KEY); // get next <key> tag position
        if (NULL == ptr_next)
        {
            ptr_next = strstr(ptr_end, TAG_DICT_END);
        }
        if ((ptr = strstr(ptr_end, TAG_STRING)) != NULL && ptr < ptr_next)
        {
            p_node->value_type = KEY_VALUE_TYPE_STRING;
            p_node->p_string_value = (INT8 *)ptr + strlen(TAG_STRING);
            p_node->string_value_len = strstr(ptr, TAG_STRING_END) - p_node->p_string_value;
        }
        else if ((ptr = strstr(ptr_end, TAG_INT)) != NULL && ptr < ptr_next)
        {
            p_node->value_type = KEY_VALUE_TYPE_INT;
            p_node->int_value = atoi(ptr + strlen(TAG_INT));
        }
        else if ((ptr = strstr(ptr_end, TAG_TRUE)) != NULL && ptr < ptr_next)
        {
            p_node->value_type = KEY_VALUE_TYPE_BOOL;
            p_node->bool_value = TRUE;
        }
        else if ((ptr = strstr(ptr_end, TAG_FALSE)) != NULL && ptr < ptr_next)
        {
            p_node->value_type = KEY_VALUE_TYPE_BOOL;
            p_node->bool_value = FALSE;
        }
        else
        {
            p_node->value_type = KEY_VALUE_TYPE_UNKNOWN;
        }

        if (NULL == p_root)
        {
            p_root = p_prev = p_node;
        }
        else
        {
            p_prev->p_next = p_node;
            p_prev = p_node;
        }
    }

    return p_root;
}

BOOL plist_init(const INT8 *p_config_plist_path)
{
    INT32 fd = -1;
    INT32 file_len = -1;

    if (NULL == p_config_plist_path)
    {
        return FALSE;
    }

    if ((fd = open(p_config_plist_path, O_RDONLY)) == ERROR)
    {
        DEBUG_PRINT(DEBUG_ERROR, "open failed:%s\n", strerror(errno));
        return FALSE;
    }

    if ((file_len = lseek(fd, 0, SEEK_END)) == ERROR)
    {
        DEBUG_PRINT(DEBUG_ERROR, "lseek failed:%s\n", strerror(errno));
        SAFE_CLOSE(fd);
        return FALSE;
    }

    if (lseek(fd, 0, SEEK_SET) == ERROR)
    {
        DEBUG_PRINT(DEBUG_ERROR, "lseek failed:%s\n", strerror(errno));
        SAFE_CLOSE(fd);
        return FALSE;
    }
    DEBUG_PRINT(DEBUG_NONE, "config.plist file length=%d", file_len);

    p_config_plist_read_buffer = (INT8 *)malloc(file_len + 4);
    if (NULL == p_config_plist_read_buffer)
    {
        DEBUG_PRINT(DEBUG_ERROR, "malloc failed\n");
        SAFE_CLOSE(fd);
        return FALSE;
    }
    memset(p_config_plist_read_buffer, 0, file_len + 4);

    if (readn(fd, p_config_plist_read_buffer, file_len) != file_len)
    {
        DEBUG_PRINT(DEBUG_ERROR, "read failed:%s\n", strerror(errno));
        SAFE_FREE(p_config_plist_read_buffer);
        SAFE_CLOSE(fd);
        return FALSE;
    }

    DEBUG_PRINT(DEBUG_NOTICE, "config.plist:\n%s\n", p_config_plist_read_buffer);

    p_key_value_root = parse_config_plist(p_config_plist_read_buffer);
    if (NULL == p_key_value_root)
    {
        DEBUG_PRINT(DEBUG_ERROR, "parse_config_plist failed\n");
        SAFE_FREE(p_config_plist_read_buffer);
        SAFE_CLOSE(fd);
        return FALSE;
    }

    SAFE_CLOSE(fd);
    return TRUE;
}

void plist_destroy(void)
{
    KEY_VALUE_NODE *p_node = p_key_value_root;

    while (p_node != NULL)
    {
        if (p_node->need_free)
        {
            SAFE_FREE(p_node->p_string_value);
        }
        p_node->need_free = FALSE;

        p_node = p_node->p_next;
    }

    SAFE_FREE(p_key_value_root);
    SAFE_FREE(p_config_plist_read_buffer);

    return;
}

void plist_print(void)
{
    KEY_VALUE_NODE *p_node = p_key_value_root;
    INT32 i = 0;

    printf("parsed config.plist:\n");
    while (p_node != NULL)
    {
        for (i = 0; i < p_node->key_len; i++)
        {
            printf("%c", *(p_node->p_key + i));
        }
        printf("=");
        switch (p_node->value_type)
        {
            case KEY_VALUE_TYPE_STRING:
            if (p_node->p_string_value != NULL)
            {
                for (i = 0; i < p_node->string_value_len; i++)
                {
                    printf("%c", *(p_node->p_string_value + i));
                }
            }
            break;

            case KEY_VALUE_TYPE_INT:
            printf("%d", p_node->int_value);
            break;

            case KEY_VALUE_TYPE_BOOL:
            if (p_node->bool_value)
            {
                printf("true");
            }
            else
            {
                printf("false");
            }
            break;

            default:
            break;
        }
        printf("\n");

        p_node = p_node->p_next;
    }

    return;
}

void plist_set_string_value(const INT8 *p_key, const INT8 *p_string_value)
{
    KEY_VALUE_NODE *p_node = p_key_value_root;
    INT32 key_len = strlen(p_key);
    INT32 string_value_len = strlen(p_string_value);

    while (p_node != NULL)
    {
        if (key_len == p_node->key_len && memcmp(p_key, p_node->p_key, key_len) == 0)
        {
            if (p_node->need_free)
            {
                SAFE_FREE(p_node->p_string_value);
            }
            p_node->value_type = KEY_VALUE_TYPE_STRING;
            p_node->p_string_value = (INT8 *)malloc(string_value_len);
            memcpy(p_node->p_string_value, p_string_value, string_value_len);
            p_node->string_value_len = string_value_len;
            p_node->need_free = TRUE;

            break;
        }

        p_node = p_node->p_next;
    }

    return;
}

void plist_set_int_value(const INT8 *p_key, INT32 int_value)
{
    KEY_VALUE_NODE *p_node = p_key_value_root;
    INT32 key_len = strlen(p_key);

    while (p_node != NULL)
    {
        if (key_len == p_node->key_len && memcmp(p_key, p_node->p_key, key_len) == 0)
        {
            if (p_node->need_free)
            {
                SAFE_FREE(p_node->p_string_value);
            }
            p_node->need_free = FALSE;
            p_node->value_type = KEY_VALUE_TYPE_INT;
            p_node->int_value = int_value;

            break;
        }

        p_node = p_node->p_next;
    }

    return;
}

void plist_set_bool_value(const INT8 *p_key, BOOL bool_value)
{
    KEY_VALUE_NODE *p_node = p_key_value_root;
    INT32 key_len = strlen(p_key);

    while (p_node != NULL)
    {
        if (key_len == p_node->key_len && memcmp(p_key, p_node->p_key, key_len) == 0)
        {
            if (p_node->need_free)
            {
                SAFE_FREE(p_node->p_string_value);
            }
            p_node->need_free = FALSE;
            p_node->value_type = KEY_VALUE_TYPE_BOOL;
            p_node->bool_value = bool_value;

            break;
        }

        p_node = p_node->p_next;
    }

    return;
}
