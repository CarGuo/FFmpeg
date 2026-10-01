/* Verify the real !HAVE_GETADDRINFO implementation's error distinction. */
#include <limits.h>
#include <stdio.h>
#include "config.h"
#include "libavformat/network.h"
#include "libavutil/mem.h"

#if HAVE_GETADDRINFO
#error This regression must be built with HAVE_GETADDRINFO=0
#endif

static int check(const char *node, int allocation_failure, int expected)
{
    struct addrinfo hints = { 0 }, *ai = NULL;
    int ret;
    hints.ai_flags = AI_NUMERICHOST;
    if (allocation_failure)
        av_max_alloc(1);
    ret = ff_getaddrinfo(node, NULL, &hints, &ai);
    av_max_alloc(INT_MAX);
    if (ret != expected || (ret && ai) || (!ret && !ai)) {
        fprintf(stderr, "FAIL %s allocation_failure=%d: %d expected %d\n",
                node, allocation_failure, ret, expected);
        if (ai)
            ff_freeaddrinfo(ai);
        return 1;
    }
    if (ai)
        ff_freeaddrinfo(ai);
    printf("PASS %s allocation_failure=%d result=%d\n", node, allocation_failure, ret);
    return 0;
}

int main(void)
{
    int failures = 0;
    failures += check("localhost", 0, EAI_NONAME);
    failures += check("other.invalid", 0, EAI_NONAME);
    failures += check("127.0.0.1", 0, 0);
    failures += check("localhost", 1, EAI_FAIL);
    failures += check("127.0.0.1", 1, EAI_FAIL);
    failures += check("127.0.0.1", 0, 0);
    return !!failures;
}
