//
// Created by dingjing on 23-12-8.
//

#include "regex.h"
#include "log.h"

void regex_free(GWMRegex *regex)
{
    g_return_if_fail(regex);

    FREE(regex->pattern);
    FREE(regex->regex);
    FREE(regex);
}

GWMRegex *regex_new(const char *pattern)
{
    int errorCode;
    PCRE2_SIZE offset;

    GWMRegex* re = (GWMRegex*) g_malloc0(sizeof(GWMRegex));
    re->pattern = g_strdup(pattern);
    uint32_t options = PCRE2_UTF;
    /* We use PCRE_UCP so that \B, \b, \D, \d, \S, \s, \W, \w and some POSIX
     * character classes play nicely with Unicode */
    options |= PCRE2_UCP;
    if (!(re->regex = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, options, &errorCode, &offset, NULL))) {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(errorCode, buffer, sizeof(buffer));
        WARNING("PCRE regular expression compilation failed at %lu: %s", offset, buffer);
        regex_free(re);
        return NULL;
    }
    return re;
}

bool regex_matches(GWMRegex *regex, const char *input)
{
    pcre2_match_data *match_data;
    int rc;

    match_data = pcre2_match_data_create_from_pattern(regex->regex, NULL);

    rc = pcre2_match(regex->regex, (PCRE2_SPTR)input, strlen(input), 0, 0, match_data, NULL);
    pcre2_match_data_free(match_data);
    if (rc > 0) {
        INFO("Regular expression \"%s\" matches \"%s\"", regex->pattern, input);
        return true;
    }

    if (rc == PCRE2_ERROR_NOMATCH) {
        INFO("Regular expression \"%s\" does not match \"%s\"", regex->pattern, input);
        return false;
    }

    ERROR("PCRE error %d while trying to use regular expression \"%s\" on input \"%s\", see pcreapi(3)", rc, regex->pattern, input);

    return false;
}
