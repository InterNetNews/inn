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

struct tst *tst_init(int width)
{
   struct tst *tst;
   struct node *current_node;
   int i;


if((tst = (struct tst *) calloc(1, sizeof(struct tst))) == NULL)
   return NULL;

if ((tst->node_lines = (struct node_lines *) calloc(1, sizeof(struct node_lines))) == NULL)
{
   free(tst);
   return NULL;
}

tst->node_line_width = width;
tst->node_lines->next = NULL;
if ((tst->node_lines->node_line = (struct node *) calloc(width, sizeof(struct node))) == NULL)
{
   free(tst->node_lines);
   free(tst);
   return NULL;
}

current_node = tst->node_lines->node_line;
tst->free_list = current_node;
for (i = 1; i < width; i++)
{
   current_node->middle = &(tst->node_lines->node_line[i]);
   current_node = current_node->middle;
}
current_node->middle = NULL;
return tst;
}


int tst_grow_node_free_list(struct tst *tst)
{
   struct node *current_node;
   struct node_lines *new_line;
   int i;

   
   if((new_line = (struct node_lines *) malloc(sizeof(struct node_lines))) == NULL)
      return TST_ERROR;
   
   if((new_line->node_line = (struct node *)
   calloc(tst->node_line_width, sizeof(struct node))) == NULL)
   {
      free(new_line);
      return TST_ERROR;
   }
   else
   {
      new_line->next = tst->node_lines;
      tst->node_lines = new_line;
   }
   
   current_node = tst->node_lines->node_line;
   tst->free_list = current_node;
   for (i = 1; i < tst->node_line_width; i++)
   {
      current_node->middle = &(tst->node_lines->node_line[i]);
      current_node = current_node->middle;
   }
   current_node->middle = NULL;
   return 1;
}


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


void *tst_delete(unsigned char *key, struct tst *tst)
{
   struct node *current_node;
   struct node *current_node_parent;
   struct node *last_branch;
   struct node *last_branch_parent;
   struct node *next_node;
   struct node *last_branch_replacement;
   struct node *last_branch_dangling_child;
   int key_index;

   
   if(key[0] == 0)
      return NULL;
   if(tst->head[(int)key[0]] == NULL)
      return NULL;
   
   last_branch = NULL;
   last_branch_parent = NULL;
   current_node = tst->head[(int)key[0]];
   current_node_parent = NULL;
   key_index = 1;
   while(current_node != NULL)
   {
      if(key[key_index] == current_node->value)
      {
         
         if( (current_node->left != NULL) || (current_node->right != NULL) )
         {
            last_branch = current_node;
            last_branch_parent = current_node_parent;
         }
         if(key[key_index] == 0)
            break;
         else
         {
            current_node_parent = current_node;
            current_node = current_node->middle;
            key_index++;
            continue;
         }
      }
      else if( ((current_node->value == 0) && (key[key_index] < 64)) ||
         ((current_node->value != 0) && (key[key_index] <
         current_node->value)) )
      {
         last_branch_parent = current_node;
         current_node_parent = current_node;
         current_node = current_node->left;
         last_branch = current_node;
         continue;
      }
      else
      {
         last_branch_parent = current_node;
         current_node_parent = current_node;
         current_node = current_node->right;
         last_branch = current_node;
         continue;
      }
   
   }
   if(current_node == NULL)
      return NULL;
   
   if(last_branch == NULL)
   {
      
         next_node = tst->head[(int)key[0]];
         tst->head[(int)key[0]] = NULL;
   }
   else if( (last_branch->left == NULL) && (last_branch->right == NULL) )
   {
      
      if(last_branch_parent->left == last_branch)
         last_branch_parent->left = NULL;
      else
         last_branch_parent->right = NULL;
      
      next_node = last_branch;
   }
   else
   {
      
      if( (last_branch->left != NULL) && (last_branch->right != NULL) )
      {
         last_branch_replacement = last_branch->right;
         last_branch_dangling_child = last_branch->left;
      }
      else if(last_branch->right != NULL)
      {
         last_branch_replacement = last_branch->right;
         last_branch_dangling_child = NULL;
      }
      else
      {
         last_branch_replacement = last_branch->left;
         last_branch_dangling_child = NULL;
      }
      
      if(last_branch_parent == NULL)
         tst->head[(int)key[0]]=last_branch_replacement;
      else
      {
         if (last_branch_parent->left == last_branch)
            last_branch_parent->left = last_branch_replacement;
         else if (last_branch_parent->right == last_branch)
            last_branch_parent->right = last_branch_replacement;
         else
            last_branch_parent->middle = last_branch_replacement;
      }
      
      if(last_branch_dangling_child != NULL)
      {
         current_node = last_branch_replacement;
      
         while (current_node->left != NULL)
            current_node = current_node->left;
      
         current_node->left = last_branch_dangling_child;
      }
      
      next_node = last_branch;
   }
   
   do
   {
      current_node = next_node;
      next_node = current_node->middle;
      
      current_node->left = NULL;
      current_node->right = NULL;
      current_node->middle = tst->free_list;
      tst->free_list = current_node;
   }
   while(current_node->value != 0);
   
   return next_node;
   
}


void tst_cleanup(struct tst *tst)
{
   struct node_lines *current_line;
   struct node_lines *next_line;

   next_line = tst->node_lines;

   do
   {
      current_line = next_line;
      next_line = current_line->next;
      free(current_line->node_line);
      free(current_line);
   }
   while(next_line != NULL);

   free(tst);
}
