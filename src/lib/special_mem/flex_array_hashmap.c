#include "flex_array_hashmap.h"

#include "lib/string.h"
#include "lib/structures/list.h"

static fixed_size_allocator_t _fah_fsa = FSA_DEFAULT(sizeof(fixed_size_allocator_t));

int _fah_key_compare_func(void *a, void *b, void *data) {
    key_func_t key_func = (key_func_t)data;
    return strncmp(key_func(a), key_func(b), (size_t)-1);
}

// TODO: get a better hash func
uint64_t string_hash64_func_default(void *data) {
    uint64_t res = 0;
    char *cur    = (char *)data;
    while (*cur) {
        res += *cur++;
    }
    return res;
}

// TODO: thread safe
int fah_insert(flex_array_hashmap_t *fah, void *data, void **out_old) {
    uint64_t bucket_idx = fah->hash_func(fah->key_func(data)) % fah->fa_size;
    list_t **slot       = (list_t **)ptr_flex_array_get_slot(fah->fa, bucket_idx);

    if (!*slot) {
        *slot = singly_linked_list_new();
    }

    if (!*slot) {
        kout(KERNEL_WARN, "fah_insert(): singly_linked_list_new() failed (OOM?)\n");
        return -ENOMEM;
    }

    void *out_old_tmp = NULL;

    int res = list_append_or_replace(*slot, data, _fah_key_compare_func, fah->key_func, &out_old_tmp);
    if (res != 0) {
        kout(KERNEL_WARN, "fah_insert(): list_append_or_replace with error %d%s\n", res,
             (res == -ENOMEM) ? "(OOM?)" : "");
    } else {
        if (out_old) *out_old = out_old_tmp;
        if (!out_old_tmp) fah->item_count++; // New item added
    }
    // TODO: load factor; rebalance
    return res;
}

#define FAH_INITIAL_SIZE 32
flex_array_hashmap_t *fah_new(key_func_t key_func, hash64_func_t hash_func) {
    flex_array_hashmap_t *res = fsa_alloc(&_fah_fsa);

    if (!res) {
        kout(KERNEL_WARN, "fah_new(): fsa_alloc() failed (OOM?)\n");
        return NULL;
    }

    res->fa         = ptr_flex_array_alloc(FAH_INITIAL_SIZE);
    res->fa_size    = FAH_INITIAL_SIZE;
    res->hash_func  = hash_func ? hash_func : string_hash64_func_default;
    res->item_count = 0;
    res->key_func   = key_func;

    return res;
}