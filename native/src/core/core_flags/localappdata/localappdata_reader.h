#ifndef KBO_CORE_FLAGS_LOCALAPPDATA_READER_H
#define KBO_CORE_FLAGS_LOCALAPPDATA_READER_H

#include <stddef.h>

int kbo_read_localappdata_json_flag_value(const char* key, int* out_value);
int kbo_read_localappdata_json_int_value(const char* key, int* out_value);
int kbo_read_localappdata_setting_flag_value(const char* key, int* out_value);
int kbo_read_localappdata_setting_int_value(const char* key, int* out_value);
int kbo_read_localappdata_named_json_flag_value(const char* file_name, const char* key, int* out_value);
int kbo_read_localappdata_named_json_int_value(const char* file_name, const char* key, int* out_value);
int kbo_read_localappdata_named_json_string_value(const char* file_name, const char* key, char* out, size_t out_size);
int kbo_write_localappdata_named_json_int_value(const char* file_name, const char* key, int value);
int kbo_write_localappdata_json_int_value(const char* key, int value);
int kbo_write_localappdata_setting_int_value(const char* key, int value);

#endif
