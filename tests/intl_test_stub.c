#include <string.h>

#include <acfutils/intl.h>

#include "intl_test_stub.h"

static const char *override_msgid;
static const char *override_msgstr;

void
intl_test_translation_override(const char *msgid, const char *msgstr)
{
    override_msgid = msgid;
    override_msgstr = msgstr;
}

void
intl_test_translation_reset(void)
{
    override_msgid = NULL;
    override_msgstr = NULL;
}

const char *
acfutils_xlate(const char *msgid)
{
    if (override_msgid != NULL && strcmp(msgid, override_msgid) == 0)
        return (override_msgstr);
    return (msgid);
}
