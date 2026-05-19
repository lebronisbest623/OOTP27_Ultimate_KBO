#include "../flags_api.h"

#include "../../localappdata/localappdata_reader.h"

#define KBO_CUSTOM_NEWS_LANGUAGE_KEY "custom_news_language"

static int kbo_clamp_custom_news_language_setting(int language)
{
    return language == KBO_CUSTOM_NEWS_LANGUAGE_EN
        ? KBO_CUSTOM_NEWS_LANGUAGE_EN
        : KBO_CUSTOM_NEWS_LANGUAGE_KO;
}

int kbo_get_custom_news_language_setting(void)
{
    int value = KBO_CUSTOM_NEWS_LANGUAGE_KO;
    if (!kbo_read_localappdata_setting_int_value(KBO_CUSTOM_NEWS_LANGUAGE_KEY, &value)) {
        value = KBO_CUSTOM_NEWS_LANGUAGE_KO;
    }
    return kbo_clamp_custom_news_language_setting(value);
}

int kbo_set_custom_news_language_setting(int language)
{
    return kbo_write_localappdata_setting_int_value(
        KBO_CUSTOM_NEWS_LANGUAGE_KEY,
        kbo_clamp_custom_news_language_setting(language));
}

const char* kbo_custom_news_language_dir(void)
{
    return kbo_get_custom_news_language_setting() == KBO_CUSTOM_NEWS_LANGUAGE_EN ? "en" : "ko";
}
