#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "index.h"
#include "object.h"

#define INDEX_FILE ".pes/index"

int index_load(Index *index)
{
    FILE *fp = fopen(INDEX_FILE, "rb");

    if (!fp) {
        index->count = 0;
        return 0;
    }

    if (fread(&index->count, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        index->count = 0;
        return 0;
    }

    for (int i = 0; i < index->count; i++) {
        if (fread(&index->entries[i], sizeof(IndexEntry), 1, fp) != 1) {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

int index_save(const Index *index)
{
    FILE *fp = fopen(INDEX_FILE, "wb");
    if (!fp) return -1;

    fwrite(&index->count, sizeof(int), 1, fp);

    for (int i = 0; i < index->count; i++) {
        fwrite(&index->entries[i], sizeof(IndexEntry), 1, fp);
    }

    fclose(fp);
    return 0;
}

int index_add(Index *index, const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return -1;

    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    void *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -1;
    }

    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -1;
    }

    fclose(fp);

    ObjectID hash;
    if (object_write(OBJ_BLOB, data, size, &hash) != 0) {
        free(data);
        return -1;
    }

    free(data);

    IndexEntry *e = &index->entries[index->count++];

    e->mode = st.st_mode;
    e->mtime_sec = st.st_mtime;
    e->size = size;

    strncpy(e->path, path, sizeof(e->path) - 1);
    e->path[sizeof(e->path) - 1] = '\0';

    memcpy(&e->hash, &hash, sizeof(ObjectID));

    return 0;
}

// REQUIRED for pes.c
int index_status(const Index *index)
{
    printf("Staged changes:\n");

    for (int i = 0; i < index->count; i++) {
        printf("  staged: %s\n", index->entries[i].path);
    }

    return 0;
}
