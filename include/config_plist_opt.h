#ifndef _CONFIG_PLIST_H_
#define _CONFIG_PLIST_H_

#include "common.h"

extern BOOL plist_init(const INT8 *p_config_plist_path);
extern void plist_print(void);
extern void plist_set_string_value(const INT8 *p_key, const INT8 *p_string_value);
extern void plist_set_int_value(const INT8 *p_key, INT32 int_value);
extern void plist_set_bool_value(const INT8 *p_key, BOOL bool_value);
extern void plist_destroy(void);
extern BOOL plist_save(const INT8 *p_config_plist_path);

#endif
