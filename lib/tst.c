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
   first letter of the string. */
struct tst {
    int node_line_width;
    struct node_lines *node_lines;
    struct node *free_list;
    struct node *head[256];
};

/* A simple linked list structure used to hold all the groups of nodes. */
struct node_lines {
    struct node *node_line;
    struct node_lines *next;
};


/*
**  Given a node and a character, decide whether a new node for that character
**  should be placed as the left child of that node.  (If false, it should be
**  placed as the right child.)
*/
#define LEFTP(n, c) (((n)->value == 0) ? ((c) < 64) : ((c) < (n)->value))


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

    tst = xcalloc(1, sizeof(struct tst));
    tst->node_lines = NULL;
    tst->node_line_width = width;
    tst_grow_node_free_list(tst);
    return tst;
}


/*
**  tst_insert inserts the string key into the tree.  Behavior when a
**  duplicate key is inserted is controlled by option.  If key is already in
**  the tree then TST_DUPLICATE_KEY is returned, and the data pointer for the
**  existing key is placed in exist_ptr.  If option is set to TST_REPLACE then
**  the existing data pointer for the existing key is replaced by data.  The
**  old data pointer will still be placed in exist_ptr.
**
**  If a duplicate key is encountered and option is not set to TST_REPLACE
**  then TST_DUPLICATE_KEY is returned.  If key is zero-length, then
**  TST_NULL_KEY is returned.  A successful insert or replace returns TST_OK.
**
**  The data argument may not be NULL; if it is, TST_NULL_DATA is returned.
**  If you just want a simple existence tree, use the tst pointer as the data
**  pointer.
*/
int
tst_insert(const unsigned char *key, void *data, struct tst *tst, int option,
           void **exist_ptr)
{
    struct node *current_node;
    struct node *new_node_tree_begin = NULL;
    int key_index;
    bool perform_loop = true;

    if (data == NULL)
        return TST_NULL_DATA;

    if (key == NULL || *key == '\0')
        return TST_NULL_KEY;

    if (tst->head[*key] == NULL) {
        if (tst->free_list == NULL)
            tst_grow_node_free_list(tst);
        tst->head[*key] = tst->free_list;
        tst->free_list = tst->free_list->middle;
        current_node = tst->head[*key];
        current_node->value = key[1];
        if (key[1] == '\0') {
            current_node->middle = data;
            return TST_OK;
        } else
            perform_loop = false;
    }

    current_node = tst->head[*key];
    key_index = 1;
    while (perform_loop) {
        if (key[key_index] == current_node->value) {
            if (key[key_index] == '\0') {
                if (exist_ptr != NULL)
                    *exist_ptr = current_node->middle;
                if (option == TST_REPLACE) {
                    current_node->middle = data;
                    return TST_OK;
                } else
                    return TST_DUPLICATE_KEY;
            }

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

        if (LEFTP(current_node, key[key_index])) {
            if (current_node->left == NULL)  {
                if (tst->free_list == NULL)
                    tst_grow_node_free_list(tst);
                current_node->left = tst->free_list;
                tst->free_list = tst->free_list->middle;
                new_node_tree_begin = current_node;
                current_node = current_node->left;
                current_node->value = key[key_index];
                if (key[key_index] == '\0') {
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
    } while (key[key_index] != '\0');

    current_node->middle = data;
    return TST_OK;
}


/*
**  tst_search finds the string key in the tree if it exists and returns the
**  data pointer associated with that key or NULL if it's not found.
*/
void *
tst_search(const unsigned char *key, struct tst *tst)
{
    struct node *current_node;
    int key_index;

    if (key == NULL || *key == '\0')
        return NULL;

    if (tst->head[*key] == NULL)
        return NULL;

    current_node = tst->head[*key];
    key_index = 1;
    while (current_node != NULL) {
        if (key[key_index] == current_node->value) {
            if (current_node->value == '\0')
                return current_node->middle;
            else {
                current_node = current_node->middle;
                key_index++;
                continue;
            }
        } else if (LEFTP(current_node, key[key_index]))
            current_node = current_node->left;
        else
            current_node = current_node->right;
    }
    return NULL;
}


/*
**  tst_delete deletes the string key from the tree if it exists and returns
**  the data pointer assocaited with that key, or NULL if it wasn't found.
*/
void *
tst_delete(const unsigned char *key, struct tst *tst)
{
    struct node *current_node;
    struct node *current_node_parent;
    struct node *last_branch;
    struct node *last_branch_parent;
    struct node *next_node;
    struct node *last_branch_replacement;
    struct node *last_branch_dangling_child;
    int key_index;

    if (key == NULL || *key == '\0')
        return NULL;

    if (tst->head[*key] == NULL)
        return NULL;

    last_branch = NULL;
    last_branch_parent = NULL;
    current_node = tst->head[*key];
    current_node_parent = NULL;
    key_index = 1;
    while (current_node != NULL) {
        if (key[key_index] == current_node->value) {
            if (current_node->left != NULL || current_node->right != NULL) {
                last_branch = current_node;
                last_branch_parent = current_node_parent;
            }
            if (key[key_index] == '\0')
                break;
            else {
                current_node_parent = current_node;
                current_node = current_node->middle;
                key_index++;
            }
        } else if (LEFTP(current_node, key[key_index])) {
            last_branch_parent = current_node;
            current_node_parent = current_node;
            current_node = current_node->left;
            last_branch = current_node;
        } else {
            last_branch_parent = current_node;
            current_node_parent = current_node;
            current_node = current_node->right;
            last_branch = current_node;
        }
    }
    if (current_node == NULL)
        return NULL;

    if (last_branch == NULL) {
        next_node = tst->head[*key];
        tst->head[*key] = NULL;
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
            tst->head[*key] = last_branch_replacement;
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
