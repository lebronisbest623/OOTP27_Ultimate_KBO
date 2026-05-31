#include "independent_acquisition_seller_ai_internal.h"
#include "helpers/transfer/independent_acquisition_seller_transfer.h"

#include <string.h>

static KboIndependentAcquisitionTransferSummary*
kbo_independent_acquisition_transfer_summary_for_team(
    KboIndependentAcquisitionTransferSummary* summaries,
    int count,
    uint32_t team_id)
{
    if (summaries == NULL || count <= 0 || team_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        if (summaries[i].team_id == team_id) {
            return &summaries[i];
        }
    }
    return NULL;
}

int kbo_independent_acquisition_seller_transfer_count_for_ai(
    uint32_t season,
    uint32_t seller_team_id,
    KboIndependentAcquisitionTransferSummary* summaries,
    int summary_count)
{
    if (summary_count >= 0) {
        KboIndependentAcquisitionTransferSummary* summary =
            kbo_independent_acquisition_transfer_summary_for_team(
                summaries,
                summary_count,
                seller_team_id);
        return summary != NULL ? summary->transferred_count : 0;
    }
    return kbo_independent_acquisition_transferred_count(season, seller_team_id);
}

int kbo_independent_acquisition_buyer_transfer_count_for_ai(
    uint32_t season,
    uint32_t buyer_team_id,
    KboIndependentAcquisitionTransferSummary* summaries,
    int summary_count)
{
    if (summary_count >= 0) {
        KboIndependentAcquisitionTransferSummary* summary =
            kbo_independent_acquisition_transfer_summary_for_team(
                summaries,
                summary_count,
                buyer_team_id);
        return summary != NULL ? summary->transferred_count : 0;
    }
    return kbo_independent_acquisition_buyer_transferred_count(season, buyer_team_id);
}

uint32_t kbo_independent_acquisition_last_transfer_date_for_ai(
    uint32_t season,
    uint32_t seller_team_id,
    KboIndependentAcquisitionTransferSummary* summaries,
    int summary_count)
{
    if (summary_count >= 0) {
        KboIndependentAcquisitionTransferSummary* summary =
            kbo_independent_acquisition_transfer_summary_for_team(
                summaries,
                summary_count,
                seller_team_id);
        return summary != NULL ? summary->last_transfer_date : 0u;
    }
    return kbo_independent_acquisition_last_transfer_date(season, seller_team_id);
}

void kbo_independent_acquisition_record_transfer_summary_for_ai(
    KboIndependentAcquisitionTransferSummary* summaries,
    int* summary_count,
    int max_count,
    uint32_t team_id,
    uint32_t today,
    int update_last_transfer_date)
{
    if (summaries == NULL || summary_count == NULL || *summary_count < 0 || team_id == 0u) {
        return;
    }
    KboIndependentAcquisitionTransferSummary* summary =
        kbo_independent_acquisition_transfer_summary_for_team(
            summaries,
            *summary_count,
            team_id);
    if (summary != NULL) {
        summary->transferred_count++;
        if (update_last_transfer_date && today > summary->last_transfer_date) {
            summary->last_transfer_date = today;
        }
        return;
    }
    if (*summary_count >= max_count) {
        *summary_count = -1;
        return;
    }

    summary = &summaries[*summary_count];
    memset(summary, 0, sizeof(*summary));
    summary->team_id = team_id;
    summary->transferred_count = 1;
    summary->last_transfer_date = update_last_transfer_date ? today : 0u;
    (*summary_count)++;
}

