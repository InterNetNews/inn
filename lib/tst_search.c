/*  $Id$
**
**  Copyright (c) 2002, Peter A. Friend 
**  All rights reserved. 
**
**  Redistribution and use in source and binary forms, with or without
**  modification, are permitted provided that the following conditions
**  are met:
**
**  Redistributions of source code must retain the above copyright
**  notice, this list of conditions and the following disclaimer.
**
**  Redistributions in binary form must reproduce the above copyright
**  notice, this list of conditions and the following disclaimer in
**  the documentation and/or other materials provided with the
**  distribution.
**
**  Neither the name of Peter A. Friend nor the names of his
**  contributors may be used to endorse or promote products derived
**  from this software without specific prior written permission.
**
**  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
**  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
**  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
**  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
**  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
**  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
**  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
**  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
**  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
**  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
**  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
**  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
**  SUCH DAMAGE.
*/

#include "tst.h"
#include <stdio.h>
#include <stdlib.h>

void *tst_search(unsigned char *key, struct tst *tst)
{
   struct node *current_node;
   int key_index;

   
   if(key[0] == 0)
      return NULL;
   
   if(tst->head[(int)key[0]] == NULL)
      return NULL;
   
   current_node = tst->head[(int)key[0]];
   key_index = 1;
   
   while (current_node != NULL)
   {
      if(key[key_index] == current_node->value)
      {
         if(current_node->value == 0)
            return current_node->middle;
         else
         {
            current_node = current_node->middle;
            key_index++;
            continue;
         }
      }
      else if( ((current_node->value == 0) && (key[key_index] < 64)) ||
         ((current_node->value != 0) && (key[key_index] <
         current_node->value)) )
      {
         current_node = current_node->left;
         continue;
      }
      else
      {
         current_node = current_node->right;
         continue;
      }
   }
   return NULL;
}

