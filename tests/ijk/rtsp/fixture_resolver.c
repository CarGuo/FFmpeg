/* Test-process-only resolver injection. Never changes OS DNS or hosts. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static unsigned calls;
typedef int (*resolver_fn)(const char *, const char *, const struct addrinfo *, struct addrinfo **);
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    resolver_fn real = (resolver_fn)dlsym(RTLD_NEXT, "getaddrinfo");
    char mapping[256] = {0}, *save, *ip;
    const char *path = getenv("FIXTURE_DNS_MAP");
    struct addrinfo *head = NULL, **tail = &head;
    unsigned call;
    if (!node || strcmp(node, "fixture.invalid") || !path)
        return real(node, service, hints, res);
    if (service && *service && getenv("FIXTURE_DNS_DELAY_MS"))
        usleep(1000 * atoi(getenv("FIXTURE_DNS_DELAY_MS")));
    pthread_mutex_lock(&mu);
    call = ++calls;
    FILE *f = fopen(path, "r");
    if (f) { fgets(mapping, sizeof(mapping), f); fclose(f); }
    pthread_mutex_unlock(&mu);
    if (!mapping[0]) return EAI_NONAME;
    fprintf(stderr, "fixture_resolver call=%u service=%s addresses=%s", call, service ? service : "", mapping);
    for (ip = strtok_r(mapping, ", \n", &save); ip; ip = strtok_r(NULL, ", \n", &save)) {
        struct addrinfo *part = NULL;
        struct addrinfo h = hints ? *hints : (struct addrinfo){0};
        h.ai_flags |= AI_NUMERICHOST;
        int ret = real(ip, service, &h, &part);
        if (ret) { if (head) freeaddrinfo(head); return ret; }
        *tail = part;
        while (*tail) tail = &(*tail)->ai_next;
    }
    *res = head;
    return head ? 0 : EAI_NONAME;
}
