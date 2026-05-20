#ifndef KBOFIX_SRC_CORE_CORE_SAVE_PATHS_H_
#define KBOFIX_SRC_CORE_CORE_SAVE_PATHS_H_

#include <stddef.h>

int kbo_get_current_save_path(char* out, size_t out_size);
int kbo_get_global_data_dir(char* out, size_t out_size);
int kbo_get_global_data_subdir(const char* dir_name, char* out, size_t out_size);
int kbo_get_global_data_file(const char* file_name, char* out, size_t out_size);
int kbo_get_save_scoped_data_dir(char* out, size_t out_size);
int kbo_get_save_scoped_data_file(const char* file_name, char* out, size_t out_size);
int kbo_get_ootp_save_file_path(const char* save_path, const char* file_name, char* out, size_t out_size);
int kbo_get_ootp_save_description_file_path(const char* save_path, char* out, size_t out_size);
int kbo_get_ootp_save_started_file_path(const char* save_path, char* out, size_t out_size);
int kbo_get_ootp_save_completed_file_path(const char* save_path, char* out, size_t out_size);

#endif
