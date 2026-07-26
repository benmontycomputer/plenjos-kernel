#include "range_tree.h"

/**
 * This function returns a node to insert into the parent of the given node if the given node splits.
 * 
 * The given node must be write-locked; the write-lock will be released by this function.
 */
range_node_t *_range_node_insert(range_node_t *node, range_item_t *item) {
    bool is_leaf = node->largest_gap & 0x1UL;
    for (int i = 0; i < RANGE_TREE_NODE_SEGMENTS; i++) {
        ptr_t slot_end  = node->splits[i + 1];

        if (item->start >= slot_end) {
            continue;
        }

        ptr_t slot_start  = node->splits[i];

        if (item->start >= slot_start) {
            if (item->end > slot_end) {
                // Spills over into the next range
                rw_lock_write_unlock(&node->lock);
                return (range_node_t *)-1UL; // Out of range
            }

            // In range

            if (is_leaf) {
                if (node->children[i]) {
                    rw_lock_write_unlock(&node->lock);
                    return (range_node_t *)-2UL; // Slot occupied
                }
                if (item->start == slot_start) {
                    // Don't need a padding block before
                    if (item->end == slot_end) {
                        // Perfectly lines up
                        node->children[i] = item;
                    } else {
                        // Need padding after
                    }
                } else {
                    if (item->end == slot_end) {
                        // Need padding before
                    } else {
                        // Need padding before and after
                    }
                }
            } else {
                range_node_t *slot = node->children[i];

                range_node_t *res = _range_node_insert(slot, item);
                if (res == -1UL || res == -2UL) {
                    return res;
                } else if (res) {
                    // Need to insert res into this node
                } else {}
            }
        }
    }
}

void range_tree_insert(range_node_t *root, range_item_t *item, void *data) {
    
}