/*  $Id$
**
**  Ternary search trie implementation.
**
**  This implementation is based on the implementation by Peter A. Friend
**  (version 1.3), but has been assimilated into INN and modified to use INN
**  formatting conventions.
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
**  Neither the name of Peter A. Friend nor the names of its contributors may
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

#ifndef INN_TST_H
#define INN_TST_H 1

#include <inn/defines.h>

BEGIN_DECLS

/* Constants used for return values and options. */
enum tst_constants {
    TST_OK,
    TST_ERROR,
    TST_NULL_KEY,
    TST_DUPLICATE_KEY,
    TST_REPLACE
};

/* Opaque data type returned by and used by ternary search trie functions. */
struct tst;

/* Allocate a new ternary search trie.  width is the number of nodes allocated
   at a time and should be chosen carefully.  One node is required for every
   character in the tree.  If you choose a value that is too small, your
   application will spend too much time calling malloc and your node space
   will be too spread out.  Too large a value is just a waste of space. */
struct tst *tst_init(int width);

/* Insert a value into the tree.  If the key already exists in the tree,
   option determiens the behavior.  If set to TST_REPLACE, the data for that
   key is replaced with the new data value and the old value is returned in
   exist_ptr.  Otherwise, TST_DUPLICATE_KEY is returned.  If key is zero
   length, TST_NULL_KEY is returned.  On success, TST_OK is returned.

   The data argument must never be NULL, as NULL is returned by tst_search to
   indicate that a key was not found.  For a simple existence tree, use the
   struct tst pointer as the data. */
int tst_insert(unsigned char *key, void *data, struct tst *, int option,
               void **exist_ptr);

/* Search for a key and return the associated data, or NULL if not found. */
void *tst_search(unsigned char *key, struct tst *);

/* Delete the given key out of the trie, returning the data that it pointed
   to.  If the key was not found, returns NULL. */
void *tst_delete(unsigned char *key, struct tst *);

/* Free the given ternary search trie and all resources it uses. */
void tst_cleanup(struct tst *);

#endif /* !INN_TST_H */
