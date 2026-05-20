#include "season_phase.h"

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../build_verify/build_verify.h"
#include "../../dates/core_text_date.h"
#include "../../dates/constants/kbo_date_constants.h"

static int kbo_season_phase_date_valid(uint32_t yyyymmdd)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    return kbo_date_serial(year, month, day) != 0u;
}

static int kbo_season_phase_year_plausible(uint32_t year)
{
    return year >= KBO_SEASON_YEAR_MIN && year <= KBO_SIM_YEAR_MAX;
}

static int kbo_season_phase_known(uint8_t phase)
{
    return phase <= KBO_SEASON_PHASE_POSTSEASON;
}

static int kbo_season_phase_date_on_or_after(uint32_t date, uint32_t anchor)
{
    uint32_t date_serial = kbo_date_serial(
        date / 10000u,
        (date / 100u) % 100u,
        date % 100u);
    uint32_t anchor_serial = kbo_date_serial(
        anchor / 10000u,
        (anchor / 100u) % 100u,
        anchor % 100u);
    return date_serial != 0u && anchor_serial != 0u && date_serial >= anchor_serial;
}

const char* kbo_season_phase_label(uint8_t phase)
{
    switch (phase) {
    case KBO_SEASON_PHASE_OFFSEASON_RESET: return "offseason_or_reset";
    case KBO_SEASON_PHASE_OFFSEASON_STARTED: return "offseason_started";
    case KBO_SEASON_PHASE_PRESEASON: return "preseason_or_spring";
    case KBO_SEASON_PHASE_REGULAR_SEASON: return "regular_season";
    case KBO_SEASON_PHASE_POSTSEASON: return "postseason_or_transition";
    default: return "unknown";
    }
}

int kbo_season_phase_is_offseason(uint8_t phase)
{
    return phase == KBO_SEASON_PHASE_OFFSEASON_RESET
        || phase == KBO_SEASON_PHASE_OFFSEASON_STARTED;
}

int kbo_season_phase_can_enter_offseason(uint8_t phase)
{
    return phase == KBO_SEASON_PHASE_REGULAR_SEASON
        || phase == KBO_SEASON_PHASE_POSTSEASON;
}

int kbo_season_phase_is_preseason_or_regular(uint8_t phase)
{
    return phase == KBO_SEASON_PHASE_PRESEASON
        || phase == KBO_SEASON_PHASE_REGULAR_SEASON;
}

int kbo_season_phase_site_is_offseason_transition(uint32_t site_rva)
{
    return kbo_current_build_rva_matches(site_rva, OOTP27_SEASON_PHASE_WRITE_1_RVA)
        || kbo_current_build_rva_matches(site_rva, OOTP27_SEASON_PHASE_WRITE_0_RVA);
}

int kbo_season_phase_info_has_today_offseason_transition_capture(
    const KboSeasonPhaseInfo* info,
    uint32_t today_yyyymmdd)
{
    return info != NULL
        && info->capture_valid
        && info->capture_date == today_yyyymmdd
        && kbo_season_phase_is_offseason(info->captured_phase)
        && kbo_season_phase_site_is_offseason_transition(info->capture_site_rva);
}

uint8_t kbo_season_phase_effective_from_values(
    uint32_t today_yyyymmdd,
    uint32_t league_year,
    uint8_t raw_phase,
    uint32_t opening_day_yyyymmdd,
    uint32_t known_offseason_start_yyyymmdd,
    int* out_corrected)
{
    if (out_corrected != NULL) {
        *out_corrected = 0;
    }

    int raw_known = kbo_season_phase_known(raw_phase);
    if (!kbo_season_phase_date_valid(today_yyyymmdd)) {
        return raw_known ? raw_phase : KBO_SEASON_PHASE_UNKNOWN;
    }

    if (known_offseason_start_yyyymmdd != 0u
            && kbo_season_phase_date_on_or_after(today_yyyymmdd, known_offseason_start_yyyymmdd)) {
        if (out_corrected != NULL) {
            *out_corrected = raw_phase != KBO_SEASON_PHASE_OFFSEASON_RESET
                && raw_phase != KBO_SEASON_PHASE_OFFSEASON_STARTED;
        }
        return KBO_SEASON_PHASE_OFFSEASON_RESET;
    }

    uint32_t date_year = today_yyyymmdd / 10000u;
    uint32_t month_day = today_yyyymmdd % 10000u;
    int league_year_valid = kbo_season_phase_year_plausible(league_year);
    if (league_year_valid && league_year > date_year && month_day >= 1001u) {
        if (out_corrected != NULL) {
            *out_corrected = raw_phase != KBO_SEASON_PHASE_OFFSEASON_RESET
                && raw_phase != KBO_SEASON_PHASE_OFFSEASON_STARTED;
        }
        return KBO_SEASON_PHASE_OFFSEASON_RESET;
    }

    if (raw_phase == KBO_SEASON_PHASE_REGULAR_SEASON
            || raw_phase == KBO_SEASON_PHASE_POSTSEASON) {
        return raw_phase;
    }

    int opening_day_valid = kbo_season_phase_date_valid(opening_day_yyyymmdd);
    if (opening_day_valid) {
        uint32_t opening_year = opening_day_yyyymmdd / 10000u;
        uint32_t opening_serial = kbo_date_serial(
            opening_day_yyyymmdd / 10000u,
            (opening_day_yyyymmdd / 100u) % 100u,
            opening_day_yyyymmdd % 100u);
        uint32_t today_serial = kbo_date_serial(
            today_yyyymmdd / 10000u,
            (today_yyyymmdd / 100u) % 100u,
            today_yyyymmdd % 100u);

        if (today_serial != 0u && opening_serial != 0u && date_year == opening_year) {
            if (today_serial < opening_serial) {
                uint8_t effective = month_day >= 301u
                    ? KBO_SEASON_PHASE_PRESEASON
                    : (raw_known ? raw_phase : KBO_SEASON_PHASE_OFFSEASON_RESET);
                if (out_corrected != NULL) {
                    *out_corrected = !raw_known || effective != raw_phase;
                }
                return effective;
            }

            uint8_t effective =
                month_day < 1001u
                    ? KBO_SEASON_PHASE_REGULAR_SEASON
                    : (month_day < 1201u
                        ? KBO_SEASON_PHASE_POSTSEASON
                        : KBO_SEASON_PHASE_OFFSEASON_RESET);
            if (out_corrected != NULL) {
                *out_corrected = !raw_known || effective != raw_phase;
            }
            return effective;
        }
    }

    if (raw_phase == KBO_SEASON_PHASE_PRESEASON) {
        return raw_phase;
    }

    uint8_t fallback = KBO_SEASON_PHASE_OFFSEASON_RESET;
    if (month_day >= 301u && month_day <= 415u) {
        fallback = KBO_SEASON_PHASE_PRESEASON;
    } else if (month_day >= 416u && month_day < 1001u) {
        fallback = KBO_SEASON_PHASE_REGULAR_SEASON;
    } else if (month_day >= 1001u && month_day < 1201u) {
        fallback = KBO_SEASON_PHASE_POSTSEASON;
    } else if (raw_known) {
        fallback = raw_phase;
    }

    if (out_corrected != NULL) {
        *out_corrected = !raw_known || fallback != raw_phase;
    }
    return fallback;
}
