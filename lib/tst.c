/*  $Id$
**
**  Ternary search trie implementation.
**
**  A ternary search trie stores key/value pairs where the key is a
**  nul-terminated string and the value is an arbitrary pointer.  It uses a
**  data structure designed for fast lookups, where each level of the trie
**  represents a character in the string being searched for.
**
**  This implementation is based on the implementation by Peter A. Friend
**  (version 1.3), but has been assimilated into INN and modified to use INN
**  formatting conventions.  If new versions are released, examine the
**  differences between that version and version 1.3 (which was checked into
**  INN as individual files in case it's no longer available) and then apply
**  the changes to this file.
**
**  Copyright (c) 2002, Peter A. Friend
**  All rights reserved.
**
**  Redistribution and use in source and binary forms, with or without
**  modification, are permitted provided that the following conditions are
**  met:
**
**  Redistributions of source code must retain the above copyright notice,
**  this list of conditions and the following disclaimer.
**
**  Redistributions in binary form must reproduce the above copyright notice,
**  this list of conditions and the following disclaimer in the documentation
**  and/or other materials provided with the distribution.
**
**  Neither the name of Peter A. Friend nor the names of his contributors may
**  be used to endorse or promote products derived from this software without
**  specific prior written permission.
**
**  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
**  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
**  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
**  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
**  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
**  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
**  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
**  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
**  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
**  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
**  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/tst.h"
#include "libinn.h"


/* A single node in the ternary search trie.  Stores a character, which is
   part of the string formed by walking the tree from its root down to the
   node, and left, right, and middle pointers to child nodes.  If value is
   non-zero (not a nul), middle is the pointer to follow if the desired
   string's character matches value.  left is used if it's less than value and
   right is used if it's greater than value.  If value is zero, this is a
   terminal node, and middle holds a pointer to the data associated with the
   string that ends at this point. */
struct node {
    unsigned char value;
    struct node *left;
    struct node *middle;
    struct node *right;
};

/* The search trie structure.  node_line_width is the number of nodes that are
   allocated at a time, and node_lines holds a linked list of groups of nodes.
   The free_list is a linked list (through the middle pointers) of available
   nodes to use, and head holds pointers to the first nodes for each possible
   first letter of the string.

   FIXME: Currently only supports ASCII strings. */
struct tst {
    int node_line_width;
    struct node_lines *node_lines;
    struct node *free_list;
    struct node *head[127];
};

/* A simple linked list structure used to hold all the groups of nodes. */
struct node_lines {
    struct node *node_line;
    struct node_lines *next;
};


/*
**  tst_init allocates memory for members of struct tst, and allocates the
**  first node_line_width nodes.  The value for width must be chosen very
**  carefully.  One node is required for every character in the tree.  If you
**  choose a value that is too small, your application will spend too much
**  time calling malloc and your node space will be too spread out.  Too large
**  a value is just a waste of space.
*/
struct tst *
tst_init(int width)
{
    struct tst *tst;
    struct node *current_node;
    int i;

    tst = xcalloc(1, sizeof(struct tst));
    tst->node_lines = xcalloc(1, sizeof(struct node_lines));
    tst->node_line_width = width;
    tst->node_lines->next = NULL;
    tst->node_lines->node_line = xcalloc(width, sizeof(struct node));

    current_node = tst->node_lines->node_line;
    tst->free_list = current_node;
    for (i = 1; i < width; i++) {
        current_node->middle = &(tst->node_lines->node_line[i]);
        current_node = current_node->middle;
    }
    current_node->middle = NULL;
    return tst;
}


/*
**  Allocate new nodes for the free list, called when the free list is empty.
*/
static void
tst_grow_node_free_list(struct tst *tst)
{
    struct node *current_node;
    struct node_lines *new_line;
    int i;

    new_line = xmalloc(sizeof(struct node_lines));
    new_line->node_line = xcalloc(tst->node_line_width, sizeof(struct node));
    new_line->next = tst->node_lines;
    tst->node_lines = new_line;

    current_node = tst->node_lines->node_line;
    tst->free_list = current_node;
    for (i = 1; i < tst->node_line_width; i++) {
        current_node->middle = &(tst->node_lines->node_line[i]);
        current_node = current_node->middle;
    }
    current_node->middle = NULL;
}


/*
**  tst_insert inserts the string key into the tree.  Behavior when a
**  duplicate key is inserted is controlled by option.  If key is already in
**  the tree then TST_DUPLICATE_KEY is returned, and the data pointer for the
**  existing key is placed in exist_ptr.  If option is set to TST_REPLACE then
**  the existing data pointer for the existing key is replaced by data.  Note
**  that the old data pointer will still be placed in exist_ptr.
**
**  If a duplicate key is encountered and option is not set to TST_REPLACE
**  then TST_DUPLICATE_KEY is returned.  If key is zero length then
**  TST_NULL_KEY is returned.  A successful insert or replace returns TST_OK.
**
**  The data argument must NEVER be NULL.  If it is, then calls to tst_search
**  will fail for a key that exists because the data value was set to NULL,
**  which is what tst_search returns when the key doesn't exist.  If you just
**  want a simple existence tree, use the tst pointer as the data pointer.
*/
int
tst_insert(unsigned char *key, void *data, struct tst *tst, int option,
           void **exist_ptr)
{
    struct node *current_node;
    struct node *new_node_tree_begin = NULL;
    int key_index;
    int perform_loop = 1;

    if (key == NULL)
        return TST_NULL_KEY;

    if (key[0] == 0)
        return TST_NULL_KEY;

    if (tst->head[(int) key[0]] == NULL) {
        if (tst->free_list == NULL)
            tst_grow_node_free_list(tst);
        tst->head[(int) key[0]] = tst->free_list;
        tst->free_list = tst->free_list->middle;
        current_node = tst->head[(int) key[0]];
        current_node->value = key[1];
        if (key[1] == 0) {
            current_node->middle = data;
            return TST_OK;
        } else
            perform_loop = 0;
    }

    current_node = tst->head[(int) key[0]];
    key_index = 1;
    while (perform_loop == 1) {
        if (key[key_index] == current_node->value) {
            if (key[key_index] == 0) {
                if (option == TST_REPLACE) {
                    if (exist_ptr != NULL)
                        *exist_ptr = current_node->middle;
                    current_node->middle = data;
                    return TST_OK;
                } else {
                    if (exist_ptr != NULL)
                        *exist_ptr = current_node->middle;
                    return TST_DUPLICATE_KEY;
                }
            } else {
                if (current_node->middle == NULL) {
                    if (tst->free_list == NULL)
                        tst_grow_node_free_list(tst);
                    current_node->middle = tst->free_list;
                    tst->free_list = tst->free_list->middle;
                    new_node_tree_begin = current_node;
                    current_node = current_node->middle;
                    current_node->value = key[key_index];
                    break;
                } else {
                    current_node = current_node->middle;
                    key_index++;
                    continue;
                }
            }
        }

        if (((current_node->value == 0) && (key[key_index] < 64))
            || ((current_node->value != 0)
                && (key[key_index] < current_node->value))) {
            if (current_node->left == NULL)  {
                if (tst->free_list == NULL)
                    tst_grow_node_free_list(tst);
                current_node->left = tst->free_list;
                tst->free_list = tst->free_list->middle;
                new_node_tree_begin = current_node;
                current_node = current_node->left;
                current_node->value = key[key_index];
                if (key[key_index] == 0) {
                    current_node->middle = data;
                    return TST_OK;
                } else
                    break;
            } else {
                current_node = current_node->left;
                continue;
            }
        } else {
            if (current_node->right == NULL) {
                if (tst->free_list == NULL)
                    tst_grow_node_free_list(tst);
                current_node->right = tst->free_list;
                tst->free_list = tst->free_list->middle;
                new_node_tree_begin = current_node;
                current_node = current_node->right;
                current_node->value = key[key_index];
                break;
            } else {
                current_node = current_node->right;
                continue;
            }
        }
    }

    do {
        key_index++;
        if (tst->free_list == NULL)
            tst_grow_node_free_list(tst);
        current_node->middle = tst->free_list;
        tst->free_list = tst->free_list->middle;
        current_node = current_node->middle;
        current_node->value = key[key_index];
    } while (key[key_index] != 0);

    current_node->middle = data;
    return TST_OK;
}


/*
**  tst_search finds the string key in the tree if it exists and returns the
**  data pointer associated with that key.  If key is not found, then NULL is
**  returned.  Since the success of the search is indicated by the return of a
**  valid data pointer, it is essential that the data argument provided to
**  tst_insert is NEVER NULL.  If you just want a simple existence tree, use
**  the tst pointer as the data pointer.
*/
void *
tst_search(unsigned char *key, struct tst *tst)
{
    struct node *current_node;
    int key_index;

    if (key[0] == 0)
        return NULL;

    if (tst->head[(int) key[0]] == NULL)
        return NULL;

    current_node = tst->head[(int) key[0]];
    key_index = 1;

    while (current_node != NULL) {
        if (key[key_index] == current_node->value) {
            if (current_node->value == 0)
                return current_node->middle;
            else {
                current_node = current_node->middle;
                key_index++;
                continue;
            }
        } else if (((current_node->value == 0) && (key[key_index] < 64))
                   || ((current_node->value != 0)
                       && (key[key_index] < current_node->value))) {
            current_node = current_node->left;
            continue;
        } else {
            current_node = current_node->right;
            continue;
        }
    }
    return NULL;
}


/*
**  tst_delete deletes the string key from the tree if it exists and returns
**  the data pointer assocaited with that key.  If key is not found, then NULL
**  is returned.
*/
void *
tst_delete(unsigned char *key, struct tst *tst)
{
    struct node *current_node;
    struct node *current_node_parent;
    struct node *last_branch;
    struct node *last_branch_parent;
    struct node *next_node;
    struct node *last_branch_replacement;
    struct node *last_branch_dangling_child;
    int key_index;

    if (key[0] == 0)
        return NULL;

    if (tst->head[(int) key[0]] == NULL)
        return NULL;

    last_branch = NULL;
    last_branch_parent = NULL;
    current_node = tst->head[(int) key[0]];
    current_node_parent = NULL;
    key_index = 1;
    while (current_node != NULL) {
        if (key[key_index] == current_node->value) {
            if (current_node->left != NULL || current_node->right != NULL) {
                last_branch = current_node;
                last_branch_parent = current_node_parent;
            }
            if (key[key_index] == 0)
                break;
            else {
                current_node_parent = current_node;
                current_node = current_node->middle;
                key_index++;
                continue;
            }
        } else if (((current_node->value == 0) && (key[key_index] < 64))
                   || ((current_node->value != 0)
                       && (key[key_index] < current_node->value))) {
            last_branch_parent = current_node;
            current_node_parent = current_node;
            current_node = current_node->left;
            last_branch = current_node;
            continue;
        } else {
            last_branch_parent = current_node;
            current_node_parent = current_node;
            current_node = current_node->right;
            last_branch = current_node;
            continue;
        }
    }
    if (current_node == NULL)
        return NULL;

    if (last_branch == NULL) {
        next_node = tst->head[(int) key[0]];
         tst->head[(int)key[0]] = NULL;
    } else if (last_branch->left == NULL && last_branch->right == NULL) {
        if (last_branch_parent->left == last_branch)
            last_branch_parent->left = NULL;
        else
            last_branch_parent->right = NULL;
        next_node = last_branch;
    } else {
        if (last_branch->left != NULL && last_branch->right != NULL) {
            last_branch_replacement = last_branch->right;
            last_branch_dangling_child = last_branch->left;
        } else if (last_branch->right != NULL) {
            last_branch_replacement = last_branch->right;
            last_branch_dangling_child = NULL;
        } else {
            last_branch_replacement = last_branch->left;
            last_branch_dangling_child = NULL;
        }

        if (last_branch_parent == NULL)
            tst->head[(int) key[0]] = last_branch_replacement;
        else {
            if (last_branch_parent->left == last_branch)
                last_branch_parent->left = last_branch_replacement;
            else if (last_branch_parent->right == last_branch)
                last_branch_parent->right = last_branch_replacement;
            else
                last_branch_parent->middle = last_branch_replacement;
        }

        if (last_branch_dangling_child != NULL) {
            current_node = last_branch_replacement;
            while (current_node->left != NULL)
                current_node = current_node->left;
            current_node->left = last_branch_dangling_child;
        }

        next_node = last_branch;
    }

    do {
        current_node = next_node;
        next_node = current_node->middle;

        current_node->left = NULL;
        current_node->right = NULL;
        current_node->middle = tst->free_list;
        tst->free_list = current_node;
    } while (current_node->value != 0);

    return next_node;
}


/*
**  tst_cleanup frees all memory allocated to nodes, internal structures,
**  as well as tst itself.
*/
void
tst_cleanup(struct tst *tst)
{
    struct node_lines *current_line;
    struct node_lines *next_line;

    next_line = tst->node_lines;
    do {
        current_line = next_line;
        next_line = current_line->next;
        free(current_line->node_line);
        free(current_line);
    } while (next_line != NULL);

    free(tst);
}
