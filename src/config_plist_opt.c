#include "base_fun.h"

#define XML_HEAD "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" \
    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
#define XML_CLOVER_BOOT "\t<key>Boot</key>\n" \
	"\t<dict>\n" \
	"\t\t<key>DefaultVolume</key>\n" \
	"\t\t<string>LastBootedVolume</string>\n" \
	"\t\t<key>Fast</key>\n" \
	"\t\t<true/>\n" \
	"\t</dict>\n"

#define TAG_PLIST "<plist version=\"1.0\">"
#define TAG_PLIST_END "</plist>"
#define TAG_DICT "<dict>"
#define TAG_DICT_END "</dict>"
#define TAG_KEY "<key>"
#define TAG_KEY_END "</key>"
#define TAG_DATA "<data>"
#define TAG_DATA_END "</data>"
#define TAG_STRING "<string>"
#define TAG_STRING_END "</string>"
#define TAG_INT "<integer>"
#define TAG_INT_END "</integer>"
#define TAG_TRUE "<true/>"
#define TAG_FALSE "<false/>"
#define ELE_SMBIOS "SMBIOS"
#define RT_VARIABLES "RtVariables"

#define KEY_VALUE_TYPE_UNKNOWN 0
#define KEY_VALUE_TYPE_INT 1
#define KEY_VALUE_TYPE_BOOL 2
#define KEY_VALUE_TYPE_STRING 3
#define KEY_VALUE_TYPE_DATA 4

typedef struct KEY_VALUE_NODE
{
	const INT8 *p_key;
    INT32 key_len;
    INT32 value_type;
    INT32 int_value;
    BOOL bool_value;
	INT8 *p_string_value;
    INT32 string_value_len;
    INT8 *p_data_value;
    INT32 data_value_len;
	BOOL need_free;
    struct KEY_VALUE_NODE *p_next;
}KEY_VALUE_NODE;

static INT8 *p_config_plist_read_buffer = NULL;
static KEY_VALUE_NODE *p_key_value_root = NULL;

static void free_node_valuemem(KEY_VALUE_NODE *p_node)
{
    if (p_node->need_free)
    {
        if (p_node->value_type == KEY_VALUE_TYPE_STRING)
        {
            SAFE_FREE(p_node->p_string_value);
        }
        else if (p_node->value_type == KEY_VALUE_TYPE_DATA)
        {
            SAFE_FREE(p_node->p_data_value);
        }
        else
        {
            DEBUG_PRINT(DEBUG_ERROR, "value_type error: p_node->value_type=%d\n", p_node->value_type);
        }
    }
    p_node->need_free = FALSE;

    return;
}

static KEY_VALUE_NODE *parse_keyvalue_nodelist(const INT8 *p_start_pos, const INT8 *p_stop_pos)
{
    const INT8 *ptr = p_start_pos;
    const INT8 *ptr_next = NULL;
    const INT8 *ptr_end = NULL;
    KEY_VALUE_NODE *p_nodelist = NULL;
    KEY_VALUE_NODE *p_node = NULL;
    KEY_VALUE_NODE *p_prev = NULL;

    while (ptr != NULL && (ptr = strstr(ptr, TAG_KEY)) != NULL && (ptr_end = strstr(ptr, TAG_KEY_END)) != NULL)
    {
        if (ptr > p_stop_pos || ptr_end > p_stop_pos)
        {
            break;
        }

        p_node = (KEY_VALUE_NODE *)malloc(sizeof(KEY_VALUE_NODE));
        memset(p_node, 0, sizeof(KEY_VALUE_NODE));
        p_node->p_key = ptr + strlen(TAG_KEY);
        p_node->key_len = ptr_end - p_node->p_key;

        ptr_end = ptr_end + strlen(TAG_KEY_END);
        ptr_next = strstr(ptr_end, TAG_KEY); // get next <key> tag position
        if (NULL == ptr_next || ptr_next > p_stop_pos)
        {
            ptr_next = p_stop_pos;
        }

        if ((ptr = strstr(ptr_end, TAG_DATA)) != NULL && ptr < ptr_next)
        {
            p_node->value_type = KEY_VALUE_TYPE_DATA;
            p_node->p_data_value = (INT8 *)ptr + strlen(TAG_DATA);
            p_node->data_value_len = strstr(ptr, TAG_DATA_END) - p_node->p_data_value;
        }
        else if ((ptr = strstr(ptr_end, TAG_STRING)) != NULL && ptr < ptr_next)
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

        if (NULL == p_nodelist)
        {
            p_nodelist = p_prev = p_node;
        }
        else
        {
            p_prev->p_next = p_node;
            p_prev = p_node;
        }

        ptr = ptr_next;
    }

    return p_nodelist;
}

static KEY_VALUE_NODE *parse_config_plist(const INT8 *p_buffer)
{
    const INT8 *ptr = NULL;
    const INT8 *ptr_end = NULL;
    KEY_VALUE_NODE *p_root = NULL;
    KEY_VALUE_NODE *p_node = NULL;

    ptr = strstr(p_buffer, RT_VARIABLES);
    if (NULL != ptr)
    {
        if ((ptr = strstr(ptr, TAG_DICT)) == NULL || (ptr_end = strstr(ptr, TAG_DICT_END)) == NULL)
        {
            DEBUG_PRINT(DEBUG_WARN, "config.plist format error");
            return NULL;
        }
        p_root = parse_keyvalue_nodelist(ptr + strlen(TAG_DICT), ptr_end);
    }

    ptr = strstr(p_buffer, ELE_SMBIOS);
    if (NULL == ptr)
    {
        DEBUG_PRINT(DEBUG_WARN, "config.plist format error");
        return NULL;
    }

    if ((ptr = strstr(ptr, TAG_DICT)) == NULL || (ptr_end = strstr(ptr, TAG_DICT_END)) == NULL)
    {
        DEBUG_PRINT(DEBUG_WARN, "config.plist format error");
        return NULL;
    }

    if (NULL == p_root)
    {
        p_root = parse_keyvalue_nodelist(ptr + strlen(TAG_DICT), ptr_end);
    }
    else
    {
        p_node = p_root;
        while (p_node->p_next != NULL)
        {
            p_node = p_node->p_next;
        }
        p_node->p_next = parse_keyvalue_nodelist(ptr + strlen(TAG_DICT), ptr_end);
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

    DEBUG_PRINT(DEBUG_INFO, "config.plist:\n%s\n", p_config_plist_read_buffer);

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
    KEY_VALUE_NODE *ptr;

    while (p_node != NULL)
    {
        free_node_valuemem(p_node);
        ptr = p_node;
        p_node = p_node->p_next;
        SAFE_FREE(ptr);
    }

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
            case KEY_VALUE_TYPE_DATA:
            printf("data:");
            if (p_node->p_data_value != NULL)
            {
                for (i = 0; i < p_node->data_value_len; i++)
                {
                    printf("%c", *(p_node->p_data_value + i));
                }
            }
            break;

            case KEY_VALUE_TYPE_STRING:
            printf("string:");
            if (p_node->p_string_value != NULL)
            {
                for (i = 0; i < p_node->string_value_len; i++)
                {
                    printf("%c", *(p_node->p_string_value + i));
                }
            }
            break;

            case KEY_VALUE_TYPE_INT:
            printf("integer:");
            printf("%d", p_node->int_value);
            break;

            case KEY_VALUE_TYPE_BOOL:
            printf("bool:");
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

void plist_set_data_value(const INT8 *p_key, const INT8 *p_data_value)
{
    KEY_VALUE_NODE *p_node = p_key_value_root;
    INT32 key_len = strlen(p_key);
    INT32 data_value_len = strlen(p_data_value);

    while (p_node != NULL)
    {
        if (key_len == p_node->key_len && memcmp(p_key, p_node->p_key, key_len) == 0)
        {
            free_node_valuemem(p_node);
            p_node->value_type = KEY_VALUE_TYPE_DATA;
            p_node->p_data_value = (INT8 *)malloc(data_value_len);
            memcpy(p_node->p_data_value, p_data_value, data_value_len);
            p_node->data_value_len = data_value_len;
            p_node->need_free = TRUE;

            break;
        }

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
            free_node_valuemem(p_node);
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
            free_node_valuemem(p_node);
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
            free_node_valuemem(p_node);
            p_node->value_type = KEY_VALUE_TYPE_BOOL;
            p_node->bool_value = bool_value;

            break;
        }

        p_node = p_node->p_next;
    }

    return;
}

BOOL plist_save(const INT8 *p_config_plist_path)
{
    KEY_VALUE_NODE *p_node = p_key_value_root;
    INT32 fd = -1;
    INT8 int_string[16] = {0};

    if (NULL == p_config_plist_path)
    {
        return FALSE;
    }

    if ((fd = open(p_config_plist_path, O_WRONLY | O_CREAT | O_TRUNC, 0777)) == ERROR)
    {
        DEBUG_PRINT(DEBUG_ERROR, "open failed:%s\n", strerror(errno));
        return FALSE;
    }

    writen(fd, XML_HEAD, strlen(XML_HEAD));
    writen(fd, TAG_PLIST"\n", strlen(TAG_PLIST"\n"));
    writen(fd, TAG_DICT"\n", strlen(TAG_DICT"\n"));
    writen(fd, XML_CLOVER_BOOT, strlen(XML_CLOVER_BOOT));

    if (p_node != NULL && 3 == p_node->key_len && memcmp(p_node->p_key, "ROM", p_node->key_len) == 0)
    {
        writen(fd, "\t"TAG_KEY RT_VARIABLES TAG_KEY_END"\n", strlen("\t"TAG_KEY RT_VARIABLES TAG_KEY_END"\n"));
        writen(fd, "\t"TAG_DICT"\n", strlen("\t"TAG_DICT"\n"));

        writen(fd, "\t\t"TAG_KEY, strlen("\t\t"TAG_KEY));
        writen(fd, p_node->p_key, p_node->key_len);
        writen(fd, TAG_KEY_END"\n", strlen(TAG_KEY_END"\n"));
        if (KEY_VALUE_TYPE_DATA == p_node->value_type)
        {
            writen(fd, "\t\t"TAG_DATA"\n", strlen("\t\t"TAG_DATA"\n"));
            writen(fd, "\t\t", strlen("\t\t"));
            writen(fd, p_node->p_data_value, p_node->data_value_len);
            writen(fd, "\n", strlen("\n"));
            writen(fd, "\t\t"TAG_DATA_END"\n", strlen("\t\t"TAG_DATA_END"\n"));
        }
        else if (KEY_VALUE_TYPE_STRING == p_node->value_type)
        {
            writen(fd, "\t\t"TAG_STRING"\n", strlen("\t\t"TAG_STRING"\n"));
            writen(fd, "\t\t", strlen("\t\t"));
            writen(fd, p_node->p_string_value, p_node->string_value_len);
            writen(fd, "\n", strlen("\n"));
            writen(fd, "\t\t"TAG_STRING_END"\n", strlen("\t\t"TAG_STRING_END"\n"));
        }

        writen(fd, "\t"TAG_DICT_END"\n", strlen("\t"TAG_DICT_END"\n"));
        
        p_node = p_node->p_next;
    }

    writen(fd, "\t"TAG_KEY ELE_SMBIOS TAG_KEY_END"\n", strlen("\t"TAG_KEY ELE_SMBIOS TAG_KEY_END"\n"));
    writen(fd, "\t"TAG_DICT"\n", strlen("\t"TAG_DICT"\n"));

    while (p_node != NULL)
    {
        writen(fd, "\t\t"TAG_KEY, strlen("\t\t"TAG_KEY));
        writen(fd, p_node->p_key, p_node->key_len);
        writen(fd, TAG_KEY_END"\n", strlen(TAG_KEY_END"\n"));
        switch (p_node->value_type)
        {
            case KEY_VALUE_TYPE_DATA:
            if (p_node->p_data_value != NULL)
            {
                writen(fd, "\t\t"TAG_DATA, strlen("\t\t"TAG_DATA));
                writen(fd, p_node->p_data_value, p_node->data_value_len);
                writen(fd, TAG_DATA_END"\n", strlen(TAG_DATA_END"\n"));
            }
            break;

            case KEY_VALUE_TYPE_STRING:
            if (p_node->p_string_value != NULL)
            {
                writen(fd, "\t\t"TAG_STRING, strlen("\t\t"TAG_STRING));
                writen(fd, p_node->p_string_value, p_node->string_value_len);
                writen(fd, TAG_STRING_END"\n", strlen(TAG_STRING_END"\n"));
            }
            break;

            case KEY_VALUE_TYPE_INT:
            writen(fd, "\t\t"TAG_INT, strlen("\t\t"TAG_INT));
            snprintf(int_string, sizeof(int_string), "%d", p_node->int_value);
            writen(fd, int_string, strlen(int_string));
            writen(fd, TAG_INT_END"\n", strlen(TAG_INT_END"\n"));
            break;

            case KEY_VALUE_TYPE_BOOL:
            if (p_node->bool_value)
            {
                writen(fd, "\t\t"TAG_TRUE"\n", strlen("\t\t"TAG_TRUE"\n"));
            }
            else
            {
                writen(fd, "\t\t"TAG_FALSE"\n", strlen("\t\t"TAG_FALSE"\n"));
            }
            break;

            default:
            break; 
        }

        p_node = p_node->p_next;
    }

    writen(fd, "\t"TAG_DICT_END"\n", strlen("\t"TAG_DICT_END"\n"));
    writen(fd, TAG_DICT_END"\n", strlen(TAG_DICT_END"\n"));
    writen(fd, TAG_PLIST_END"\n", strlen(TAG_PLIST_END"\n"));

    SAFE_CLOSE(fd);
    return TRUE;
}
