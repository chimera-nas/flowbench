#pragma once

#include <stdint.h>
#include <errno.h>

#define NS_PER_S (1000000000UL)

static uint64_t
ts_interval(
    const struct timespec *end,
    const struct timespec *start)
{
    return NS_PER_S * (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec);
}

static void
format_throughput(
    char *out,
    int outlen,
    uint64_t bytes,
    uint64_t nanoseconds)
{
    double seconds = (double)nanoseconds / NS_PER_S;
    double tp = 8.0 * (double)bytes / seconds;

    if (tp > 1000000000.0F) {
        snprintf(out, outlen, "%.02F Gbps", tp / 1000000000.0F);
    } else if (bytes / seconds > 1000000UL) {
        snprintf(out, outlen, "%.02F Mbps", tp / 1000000.0F);
    } else if (bytes / seconds > 1000UL) {
        snprintf(out, outlen, "%.02F Kbps", tp / 1000.0F);
    } else {
        snprintf(out, outlen, "%.02F bps", tp);
    }
}


static int 
parse_size(uint64_t *result, const char *size_str) {

    char *end;

    errno = 0;

    uint64_t size = strtoul(size_str, &end, 10);
    if (errno != 0 || end == size_str) {
        return -1;
    }

    uint64_t multiplier = 1;

    if (*end != '\0') {
        if (strcasecmp(end, "kib") == 0 || strcasecmp(end, "k") == 0) {
            multiplier = 1024;
        } else if (strcasecmp(end, "kb") == 0) {
            multiplier = 1000;
        } else if (strcasecmp(end, "mib") == 0 || strcasecmp(end, "m") == 0) {
            multiplier = 1024 * 1024;
        } else if (strcasecmp(end, "mb") == 0) {
            multiplier = 1000 * 1000;
        } else if (strcasecmp(end, "gib") == 0 || strcasecmp(end, "g") == 0) {
            multiplier = 1024 * 1024 * 1024;
        } else if (strcasecmp(end, "gb") == 0) {
            multiplier = 1000 * 1000 * 1000;
        } else if (strcasecmp(end, "tib") == 0) {
            multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
        } else if (strcasecmp(end, "tb") == 0) {
            multiplier = 1000ULL * 1000ULL * 1000ULL * 1000ULL;
        } else {
            return 1;
        }
    }

    *result = size * multiplier;

    return 0;
}
