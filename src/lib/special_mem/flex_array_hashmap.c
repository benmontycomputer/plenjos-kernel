#include "flex_array_hashmap.h"

#include "lib/string.h"
#include "lib/structures/list.h"

static fixed_size_allocator_t _fah_fsa = FSA_DEFAULT(sizeof(fixed_size_allocator_t));

static int _fah_key_compare_func(void *a, void *b, const void *data) {
    key_func_t key_func = (key_func_t)data;
    return strncmp(key_func(a), key_func(b), (size_t)-1);
}

// TODO: get a better hash func
static uint64_t string_hash64_func_default(const void *data) {
    uint64_t res    = 0;
    const char *cur = (const char *)data;
    while (*cur) {
        res += *cur++;
    }
    return res;
}

// TODO: thread safe
int fah_insert(flex_array_hashmap_t *fah, void *data, void **out_old) {
    const char *key     = fah->key_func(data);
    uint64_t bucket_idx = fah->key_hash_func(key) % fah->fa_size;
    list_t **slot       = (list_t **)ptr_flex_array_get_slot(fah->fa, bucket_idx);

    if (!*slot) {
        *slot = singly_linked_list_new();
    }

    if (!*slot) {
        kout(KERNEL_WARN, "fah_insert(): singly_linked_list_new() failed (OOM?)\n");
        return -ENOMEM;
    }

    void *out_old_tmp = NULL;

    int res = list_append_or_replace(*slot, data, _fah_key_compare_func, (const void *)fah->key_func, &out_old_tmp);
    if (res != 0) {
        kout(KERNEL_WARN, "fah_insert(): list_append_or_replace with error %d%s\n", res,
             (res == -ENOMEM) ? "(OOM?)" : "");
    } else {
        if (out_old) *out_old = out_old_tmp;
        if (!out_old_tmp) {
            // New item added
            fah->item_count++;
            list_append(fah->keys_list, (void *)key);
        }
    }
    // TODO: load factor; rebalance
    return res;
}

void *fah_get(flex_array_hashmap_t *fah, const char *key) {
    uint64_t bucket_idx = fah->key_hash_func((void *)key) % fah->fa_size;
    list_t **slot       = (list_t **)ptr_flex_array_get_slot(fah->fa, bucket_idx);

    if (!*slot) {
        return NULL;
    }

    void *found = NULL;
    int res     = list_get_by_key(*slot, key, fah->key_func, &found);

    if (!res) {
        return found;
    }
    return NULL;
}

int fah_each(flex_array_hashmap_t *fah, size_t start, ssize_t len, void(each_func)(void *item, void *func_args),
             void *func_args) {
    ll_data_t *keys_data = (ll_data_t *)fah->keys_list->data;
    slnode_t *node       = keys_data->head;

    while (node && start) {
        node = node->next;
        start--;
    }

    if (start) {
        return -ENOENT;
    }

    while (node && len) {
        each_func(node->data, func_args);

        node = node->next;
        len--;
    }

    if (len) {
        return -ENOENT;
    }

    return 0;
}

#define FAH_INITIAL_SIZE 32
flex_array_hashmap_t *fah_new(key_func_t key_func, hash64_func_t key_hash_func) {
    flex_array_hashmap_t *res = fsa_alloc(&_fah_fsa);

    if (!res) {
        kout(KERNEL_WARN, "fah_new(): fsa_alloc() failed (OOM?)\n");
        return NULL;
    }

    res->fa            = ptr_flex_array_alloc(FAH_INITIAL_SIZE);
    res->fa_size       = FAH_INITIAL_SIZE;
    res->key_hash_func = key_hash_func ? key_hash_func : string_hash64_func_default;
    res->item_count    = 0;
    res->key_func      = key_func;
    res->keys_list     = singly_linked_list_new();

    return res;
}