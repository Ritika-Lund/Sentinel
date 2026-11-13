#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unlike assert(), this is not removed by -DNDEBUG */
#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while (0)

/* Returns how many times `needle` appears in the file (0 if unreadable). */
static int count_in_file(const char *path, const char *needle)
{
    FILE *f = fopen(path, "r");
    char line[512];
    int n = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f))
        if (strstr(line, needle)) n++;
    fclose(f);
    return n;
}

#endif
