#include "list.h"

#include "lib/special_mem/fixed_size_allocator.h"
#include "lib/special_mem/flex_array.h"
#include "lib/string.h"

const slnode_t _slnode_DEL = { 0 };
const dlnode_t _dlnode_DEL = { 0 };

static fixed_size_allocator_t _slnode_fsa = FSA_DEFAULT(sizeof(slnode_t));
static fixed_size_allocator_t _dlnode_fsa = FSA_DEFAULT(sizeof(dlnode_t));

static fixed_size_allocator_t _list_fsa = FSA_DEFAULT(sizeof(list_t));

static inline int _sll_data_get_node(ll_data_t *data, ssize_t idx, slnode_t **out) {
    if (data->length == 0) {
        return -ENOENT;
    }

    if (idx == (data->length - 1) || idx == -1L) {
        if (out) *out = data->tail;
        return 0;
    }

    if (idx < 0) {
        idx += data->length;
    }

    if (idx < 0 || idx >= data->length) {
        // Don't need to check against data->length again here
        return -ENOENT;
    }

    slnode_t *node = data->head;
    while (idx--) {
        node = node->next;
    }
    if (out) *out = node;
    return 0;
}

int _sll_get(list_t *list, ssize_t idx, void **out) {
    ll_data_t *data = (ll_data_t *)&list->data;
    mutex_lock(&data->lock);

    slnode_t *node = NULL;
    int res        = _sll_data_get_node(data, idx, &node);

    if (!res) {
        if (out) *out = node->data;
    }

    mutex_unlock(&data->lock);
    return res;
}

int _sll_set(list_t *list, ssize_t idx, void *new, void **out_old) {
    ll_data_t *data = (ll_data_t *)&list->data;
    mutex_lock(&data->lock);

    slnode_t *node = NULL;
    int res        = _sll_data_get_node(data, idx, &node);

    if (!res) {
        if (out_old) *out_old = node->data;
        node->data = new;
    }

    mutex_unlock(&data->lock);
    return res;
}

int _sll_remove(list_t *list, ssize_t idx, void **out) {
    ll_data_t *data = (ll_data_t *)&list->data;
    mutex_lock(&data->lock);

    if (data->length == 0) {
        mutex_unlock(&data->lock);
        return -ENOENT;
    }

    int res = 0;

    if (idx == 0 || idx == (0 - data->length)) {
        // Removing head
        slnode_t *old_head = data->head;
        data->head         = old_head->next;
        if (!old_head->next) {
            data->tail = NULL;
        }
        if (out) *out = old_head->data;
        slnode_free(old_head);
        data->length--;
        goto cleanup;
    }

    slnode_t *parent = NULL;
    res              = _sll_data_get_node(data, idx - 1, &parent);

    if (!res) {
        if (!parent->next) {
            res = -ENOENT;
        } else {
            slnode_t *node = parent->next;
            parent->next   = node->next;
            if (!parent->next) {
                data->tail = parent;
            }
            if (out) *out = node->data;
            slnode_free(node);
            data->length--;
        }
    }

cleanup:
    mutex_unlock(&data->lock);
    return res;
}

int _sll_insert(list_t *list, ssize_t idx, void *new) {
    ll_data_t *data = (ll_data_t *)&list->data;
    mutex_lock(&data->lock);

    int res = 0;

    if (idx == 0 || idx == -data->length) {
        slnode_t *new_head = slnode_alloc(new);
        if (!new_head) {
            res = -ENOMEM;
            goto cleanup;
        }
        new_head->next = data->head;
        data->head     = new_head;
        data->length++;
        if (!data->tail) {
            data->tail = new_head;
        }
        goto cleanup;
    }

    slnode_t *parent = NULL;
    res              = _sll_data_get_node(data, idx - 1, &parent);

    if (!res) {
        slnode_t *new_node = slnode_alloc(new);
        if (!new_node) {
            res = -ENOMEM;
            goto cleanup;
        }
        new_node->next = parent->next;
        parent->next   = new_node;
        data->length++;
        if (!new_node->next) {
            data->tail = new_node;
        }
    }

cleanup:
    mutex_unlock(&data->lock);
    return res;
}

int _sll_append(list_t *list, void *new) {
    ll_data_t *data = (ll_data_t *)&list->data;
    mutex_lock(&data->lock);

    slnode_t *new_node = slnode_alloc(new);
    if (!new_node) {
        mutex_unlock(&data->lock);
        return -ENOMEM;
    }

    if (!data->tail) {
        data->head = new_node;
    } else {
        data->tail->next = new_node;
    }

    data->tail = new_node;
    data->length++;

    mutex_unlock(&data->lock);
    return 0;
}

int _sll_append_or_replace(list_t *list, void *new, compare_func_t cmp, void *cmp_data, void **out_old) {
    ll_data_t *data = (ll_data_t *)&list->data;
    mutex_lock(&data->lock);
    int res = 0;

    slnode_t *cur = data->head;
    if (!cur) {
        data->head = slnode_alloc(new);
        if (!data->head) {
            res = -ENOMEM;
            goto cleanup;
        }
        data->tail = data->head;
        if (out_old) *out_old = NULL;
        data->length++;
    } else {
        while (cur) {
            if (cmp(cur->data, new, cmp_data) == 0) {
                if (out_old) *out_old = cur->data;
                cur->data = new;
                goto cleanup;
            }
            cur = cur->next;
        }
        slnode_t *new_node = slnode_alloc(new);
        if (!new_node) {
            res = -ENOMEM;
            goto cleanup;
        }
        data->tail->next = new_node;
        data->tail       = new_node;
        if (out_old) *out_old = NULL;
        data->length++;
    }

cleanup:
    mutex_unlock(&data->lock);
    return res;
}

int _sll_get_by_key(list_t *list, const char *key, key_func_t key_func, void **out) {
    ll_data_t *data = (ll_data_t *)&list->data;
    mutex_lock(&data->lock);

    int res = -ENOENT;

    slnode_t *node = data->head;

    while (node) {
        if (!strncmp(key_func(node->data), key, SIZE_MAX)) {
            if (out) {
                *out = node->data;
            }
            res = 0;
            break;
        }
        node = node->next;
    }

    mutex_unlock(&data->lock);
    return res;
}

ssize_t _ll_count(list_t *list) {
    ll_data_t *data = (ll_data_t *)&list->data;
    mutex_lock(&data->lock);
    ssize_t res = data->length;
    mutex_unlock(&data->lock);
    return res;
}

slnode_t *slnode_alloc(void *data) {
    slnode_t *node = (slnode_t *)fsa_alloc(&_slnode_fsa);
    if (!node) return NULL;
    node->data = data;
    return node;
}

dlnode_t *dlnode_alloc(void *data) {
    dlnode_t *node = (dlnode_t *)fsa_alloc(&_dlnode_fsa);
    if (!node) return NULL;
    node->data = data;
    return node;
}

void slnode_free(slnode_t *node) {
    fsa_free(&_slnode_fsa, (void *)node);
}

void dlnode_free(dlnode_t *node) {
    fsa_free(&_dlnode_fsa, (void *)node);
}

static const list_ops_block_t sll_ops = {
    .get               = _sll_get,
    .set               = _sll_set,
    .remove            = _sll_remove,
    .insert            = _sll_insert,
    .append            = _sll_append,
    .append_or_replace = _sll_append_or_replace,
    .get_by_key        = _sll_get_by_key,
    .count             = _ll_count,
};

list_t *singly_linked_list_new() {
    list_t *res = fsa_alloc(&_list_fsa);

    if (!res) {
        kout(KERNEL_WARN, "singly_linked_list_new(): fsa_alloc() failed (OOM?)\n");
        return NULL;
    }

    res->ops = &sll_ops;
    return res;
}