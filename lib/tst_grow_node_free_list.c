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

