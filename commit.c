#include "commit.h"
#include "tree.h"
#include "object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===== REQUIRED =====

// parse commit
int commit_parse(const void *data, size_t len, Commit *commit_out)
{
    (void)len;
    const char *p = (const char *)data;
    char hex[65];

    if (sscanf(p, "tree %64s\n", hex) != 1) return -1;
    if (hex_to_hash(hex, &commit_out->tree) != 0) return -1;

    p = strchr(p, '\n') + 1;

    if (strncmp(p, "parent ", 7) == 0) {
        if (sscanf(p, "parent %64s\n", hex) != 1) return -1;
        if (hex_to_hash(hex, &commit_out->parent) != 0) return -1;
        commit_out->has_parent = 1;
        p = strchr(p, '\n') + 1;
    } else {
        commit_out->has_parent = 0;
    }

    char author[256];
    unsigned long long ts;

    if (sscanf(p, "author %255[^\n]\n", author) != 1) return -1;

    char *last = strrchr(author, ' ');
    if (!last) return -1;

    ts = strtoull(last + 1, NULL, 10);
    *last = '\0';

    strncpy(commit_out->author, author, sizeof(commit_out->author));
    commit_out->timestamp = ts;

    p = strchr(p, '\n') + 1;
    p = strchr(p, '\n') + 1;
    p = strchr(p, '\n') + 1;

    strncpy(commit_out->message, p, sizeof(commit_out->message) - 1);
    commit_out->message[sizeof(commit_out->message) - 1] = '\0';

    return 0;
}

// serialize commit
int commit_serialize(const Commit *c, void **data_out, size_t *len_out)
{
    char tree_hex[65];
    hash_to_hex(&c->tree, tree_hex);

    char parent_hex[65];

    char buf[4096];
    int n = 0;

    n += snprintf(buf + n, sizeof(buf) - n, "tree %s\n", tree_hex);

    if (c->has_parent) {
        hash_to_hex(&c->parent, parent_hex);
        n += snprintf(buf + n, sizeof(buf) - n, "parent %s\n", parent_hex);
    }

    n += snprintf(buf + n, sizeof(buf) - n,
                  "author %s %llu\n"
                  "committer %s %llu\n\n%s",
                  c->author, c->timestamp,
                  c->author, c->timestamp,
                  c->message);

    if (n < 0 || n >= (int)sizeof(buf)) return -1;

    *data_out = malloc(n);
    memcpy(*data_out, buf, n);
    *len_out = n;

    return 0;
}

// walk commits
int commit_walk(commit_walk_fn callback, void *ctx)
{
    ObjectID id;

    if (head_read(&id) != 0) {
        printf("No commits yet.\n");
        return 0;
    }

    while (1) {
        ObjectType type;
        void *data;
        size_t len;

        if (object_read(&id, &type, &data, &len) != 0) return -1;

        Commit c;

        if (commit_parse(data, len, &c) != 0) {
            free(data);
            return -1;
        }

        free(data);

        callback(&id, &c, ctx);

        if (!c.has_parent) break;

        id = c.parent;
    }

    return 0;
}

// ===== YOUR FUNCTION =====

static void get_author(char *buf, size_t size)
{
    snprintf(buf, size, "Mahesh <mahesh@example.com>");
}

int commit_create(const char *message, ObjectID *commit_id_out)
{
    ObjectID tree_id;

    if (tree_from_index(&tree_id) != 0) return -1;

    ObjectID parent;
    int has_parent = (head_read(&parent) == 0);

    Commit c;
    memset(&c, 0, sizeof(c));

    c.tree = tree_id;
    c.has_parent = has_parent;

    if (has_parent) c.parent = parent;

    get_author(c.author, sizeof(c.author));
    c.timestamp = time(NULL);

    strncpy(c.message, message, sizeof(c.message) - 1);
    c.message[sizeof(c.message) - 1] = '\0';

    void *data;
    size_t len;

    if (commit_serialize(&c, &data, &len) != 0) return -1;

    if (object_write(OBJ_COMMIT, data, len, commit_id_out) != 0) {
        free(data);
        return -1;
    }

    if (head_update(commit_id_out) != 0) {
        free(data);
        return -1;
    }

    free(data);
    return 0;
}
