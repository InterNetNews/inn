/* -*- c -*-
 *
 * Author:      James A. Brister <brister@vix.com> -- berkeley-unix --
 * Start Date:  Thu, 01 Feb 1996 18:52:39 +1100
 * Project:     INN -- innfeed
 * File:        uio_maxiov.c
 * RCSId:       $Id$
 *
 * Copyright:   Copyright (c) 1996 by Internet Software Consortium
 *
 *              Permission to use, copy, modify, and distribute this
 *              software for any purpose with or without fee is hereby
 *              granted, provided that the above copyright notice and this
 *              permission notice appear in all copies.
 *
 *              THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE
 *              CONSORTIUM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 *              SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *              MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET
 *              SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 *              INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *              WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 *              WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 *              TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 *              USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Description: Attempt to figure out the maximum number of iovec's a
 *              writev() call will handle.
 * 
 */

#if ! defined (lint)
static const char *rcsid = "$Id$" ;
static void use_rcsid (const char *rid) {   /* Never called */
  use_rcsid (rcsid) ; use_rcsid (rid) ;
}
#endif

#include "config.h"

#include <sys/types.h>
#include <fcntl.h>

#if defined (HAVE_UNISTD_H)
#include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/uio.h>

#include <errno.h>


/* the largest number we'll try. */
#define MAXTRY 10000

int main(int argc, char ** argv)
{
  int fd = open ("/dev/null",O_WRONLY,0666) ;
  struct iovec *array ;
  int size, i, rval ;
  unsigned int amt ;
  char data ;

  (void) argc ;                 /* keep lint happy */
  (void) argv ;                 /* keep lint happy */
  
  if (fd < 0)
    {
      perror ("open (\"/dev/null\")") ;
      exit (1) ;
    }

  data = 'x' ;
  for (size = 1 ; size <= MAXTRY ; size++)
    {
      amt = sizeof (struct iovec) * size ;
      array = (struct iovec *) malloc (amt) ;
      if (array == NULL) 
        {
          printf ("Unable to allocate %d bytes\n", amt) ;
          exit (1) ;
        }
    
      for (i = 0 ; i < size ; i++)
        {
          array [i].iov_base = &data ;
          array [i].iov_len = sizeof (char) ;
        }

      if ((rval = writev (fd,array,size)) < 0)
        {
          if (errno != EINVAL)
            {
              perror ("writev") ;
              exit (1) ;
            }
          else
            {
              printf ("UIO_MAXIOV (MAX_WRITEV_VEC) looks to be %d\n", size - 1) ;
              exit (0) ;
            }
        }
      else if (size == MAXTRY)
        printf ("UIO_MAXIOV (MAX_WRITEV_VEC) looks to be *at least* %d\n", MAXTRY);

      free (array) ;
    }

  exit (0) ;
}
