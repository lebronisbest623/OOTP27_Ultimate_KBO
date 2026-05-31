#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_IO_SQL_INDEPENDENT_ACQUISITION_SQL_STORE_INTERNAL_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_IO_SQL_INDEPENDENT_ACQUISITION_SQL_STORE_INTERNAL_H_

#include "independent_acquisition_sql_store.h"

#include <stddef.h>
#include <stdint.h>

typedef struct KboIndependentAcquisitionSqlExistsResult {
    int found;
} KboIndependentAcquisitionSqlExistsResult;

typedef struct KboIndependentAcquisitionSqlCountResult {
    int count;
} KboIndependentAcquisitionSqlCountResult;

typedef struct KboIndependentAcquisitionSqlDateResult {
    uint32_t date;
} KboIndependentAcquisitionSqlDateResult;

typedef struct KboIndependentAcquisitionSqlQueuedLoadContext {
    KboIndependentAcquisitionQueuedRequest* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlQueuedLoadContext;

typedef struct KboIndependentAcquisitionSqlRequestLoadContext {
    KboIndependentAcquisitionSqlRequestRow* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlRequestLoadContext;

typedef struct KboIndependentAcquisitionSqlDecisionKeyLoadContext {
    KboIndependentAcquisitionDecisionKey* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlDecisionKeyLoadContext;

typedef struct KboIndependentAcquisitionSqlDecisionLoadContext {
    KboIndependentAcquisitionSqlDecisionRow* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlDecisionLoadContext;

typedef struct KboIndependentAcquisitionSqlTransferSummaryLoadContext {
    KboIndependentAcquisitionTransferSummary* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlTransferSummaryLoadContext;

int kbo_independent_acquisition_sql_ensure_schema(const char* source);
int kbo_independent_acquisition_sql_exists_cb(void* user_data, int ncols, char** vals, char** names);
int kbo_independent_acquisition_sql_count_cb(void* user_data, int ncols, char** vals, char** names);
int kbo_independent_acquisition_sql_date_cb(void* user_data, int ncols, char** vals, char** names);
uint32_t kbo_independent_acquisition_sql_u32(char** vals, int index);
int32_t kbo_independent_acquisition_sql_i32(char** vals, int index);
int64_t kbo_independent_acquisition_sql_i64(char** vals, int index);
void kbo_independent_acquisition_sql_text(char** vals, int index, char* out, size_t out_size);
int kbo_independent_acquisition_sql_transfer_summary_cb(void* user_data, int ncols, char** vals, char** names);
int kbo_independent_acquisition_sql_append_text(char* out, size_t out_size, size_t* cursor, const char* fmt, ...);
int kbo_independent_acquisition_sql_escape(char* out, size_t out_size, const char* value);

#endif
