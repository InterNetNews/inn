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


int tst_grow_node_free_list(struct tst *tst);
int tst_insert(unsigned char *key, void *data, struct tst *tst, int option, void **exist_ptr)
{
   struct node *current_node;
   struct node *new_node_tree_begin = NULL;
   int key_index;
   int perform_loop = 1;

   
   if (key == NULL)
      return TST_NULL_KEY;
   
   if(key[0] == 0)
      return TST_NULL_KEY;
   
   if(tst->head[(int)key[0]] == NULL)
   {
      
      if(tst->free_list == NULL)
      {
         if(tst_grow_node_free_list(tst) != 1)
            return TST_ERROR;
      }
      tst->head[(int)key[0]] = tst->free_list;
      
      tst->free_list = tst->free_list->middle;
      current_node = tst->head[(int)key[0]];
      current_node->value = key[1];
      if(key[1] == 0)
      {
         current_node->middle = data;
         return TST_OK;
      }
      else
         perform_loop = 0;
   }
   
   current_node = tst->head[(int)key[0]];
   key_index = 1;
   while(perform_loop == 1)
   {
      if(key[key_index] == current_node->value)
      {
         
         if(key[key_index] == 0)
         {
            if (option == TST_REPLACE)
            {
               if (exist_ptr != NULL)
                  *exist_ptr = current_node->middle;
         
               current_node->middle = data;
               return TST_OK;
            }
            else
            {
               if (exist_ptr != NULL)
                  *exist_ptr = current_node->middle;
               return TST_DUPLICATE_KEY;
            }
         }
         else
         {
            if(current_node->middle == NULL)
            {
               
               if(tst->free_list == NULL)
               {
                  if(tst_grow_node_free_list(tst) != 1)
                     return TST_ERROR;
               }
               current_node->middle = tst->free_list;
               
               tst->free_list = tst->free_list->middle;
               new_node_tree_begin = current_node;
               current_node = current_node->middle;
               current_node->value = key[key_index];
               break;
            }
            else
            {
               current_node = current_node->middle;
               key_index++;
               continue;
            }
         }
      }
   
      if( ((current_node->value == 0) && (key[key_index] < 64)) ||
         ((current_node->value != 0) && (key[key_index] <
         current_node->value)) )
      {
         
         if (current_node->left == NULL)
         {
            
            if(tst->free_list == NULL)
            {
               if(tst_grow_node_free_list(tst) != 1)
                  return TST_ERROR;
            }
            current_node->left = tst->free_list;
            
            tst->free_list = tst->free_list->middle;
            new_node_tree_begin = current_node;
            current_node = current_node->left;
            current_node->value = key[key_index];
            if(key[key_index] == 0)
            {
               current_node->middle = data;
               return TST_OK;
            }
            else
               break;
         }
         else
         {
            current_node = current_node->left;
            continue;
         }
      }
      else
      {
         
         if (current_node->right == NULL)
         {
            
            if(tst->free_list == NULL)
            {
               if(tst_grow_node_free_list(tst) != 1)
                  return TST_ERROR;
            }
            current_node->right = tst->free_list;
            
            tst->free_list = tst->free_list->middle;
            new_node_tree_begin = current_node;
            current_node = current_node->right;
            current_node->value = key[key_index];
            break;
         }
         else
         {
            current_node = current_node->right;
            continue;
         }
      }
   }
   
   do
   {
      key_index++;
   
      if(tst->free_list == NULL)
      {
         if(tst_grow_node_free_list(tst) != 1)
         {
            current_node = new_node_tree_begin->middle;
   
            while (current_node->middle != NULL)
               current_node = current_node->middle;
   
            current_node->middle = tst->free_list;
            tst->free_list = new_node_tree_begin->middle;
            new_node_tree_begin->middle = NULL;
   
            return TST_ERROR;
         }
      }
   
      
      if(tst->free_list == NULL)
      {
         if(tst_grow_node_free_list(tst) != 1)
            return TST_ERROR;
      }
      current_node->middle = tst->free_list;
      
      tst->free_list = tst->free_list->middle;
      current_node = current_node->middle;
      current_node->value = key[key_index];
   } while(key[key_index] !=0);
   
   current_node->middle = data;
   return TST_OK;
}

