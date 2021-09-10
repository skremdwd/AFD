/*
 *  store_file_mask.c - Part of AFD, an automatic file distribution
 *                      program.
 *  Copyright (c) 2000 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   store_file_mask - stores all file mask to a file
 **
 ** SYNOPSIS
 **   void store_file_mask(char *dir_alias, struct dir_group *dir)
 **
 ** DESCRIPTION
 **   The function store_file_mask stores all file mask for the
 **   directory 'dir_alias' into the file:
 **       $AFD_WORK_DIR/files/incoming/file_mask/<dir_alias>
 **   For each file group it stores all file mask in a block
 **   where each file mask is separated by a binary zero.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.08.2000 H.Kiehl Created
 **   28.12.2003 H.Kiehl Made number of file mask unlimited.
 **   10.09.2021 H.Kiehl When creating the file create it with
 **                      a leading dot. After we finished writing
 **                      rename without the dot. Turns out that
 **                      creating + setting a lock was not atomic
 **                      enough and some gf_xxx process where
 **                      quicker in setting the lock on an empty
 **                      file.
 **
 */
DESCR__E_M3

#include <stdio.h>            /* sprintf(), rename()                     */
#include <string.h>           /* strerror(), memcpy()                    */
#include <stdlib.h>           /* malloc(), free()                        */
#include <unistd.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int  alfbl,               /* Additional locked file buffer length */
            alfc;                /* Additional locked file counter       */
extern char *alfiles,            /* Additional locked files              */
            *p_work_dir;


/*########################## store_file_mask() ##########################*/
void
store_file_mask(char *dir_alias, struct dir_group *dir)
{
   int    align_offset,
          fbl,
          fc,
          fd,
          i,
          mod;
   size_t length;
   char   *buffer,
          file_mask_file[MAX_PATH_LENGTH],
          tmp_file_mask_file[MAX_PATH_LENGTH];

   if ((i = snprintf(file_mask_file,
                     (MAX_PATH_LENGTH - MAX_DIR_ALIAS_LENGTH - 1),
                     "%s%s%s%s/", p_work_dir, AFD_FILE_DIR, INCOMING_DIR,
                     FILE_MASK_DIR)) >= (MAX_PATH_LENGTH - MAX_DIR_ALIAS_LENGTH - 1))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Storage for file_mask_file not large (%d bytes) enough!",
                 (MAX_PATH_LENGTH - MAX_DIR_ALIAS_LENGTH - 1));
      return;
   }
   (void)memcpy(tmp_file_mask_file, file_mask_file, i);
   tmp_file_mask_file[i] = '.';
   if (my_strlcpy(&file_mask_file[i], dir_alias,
                  (MAX_PATH_LENGTH - i)) >= (MAX_PATH_LENGTH - i))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Storage for file_mask_file not large (%d bytes) enough!",
                 (MAX_PATH_LENGTH - i));
      return;
   }
   if (my_strlcpy(&tmp_file_mask_file[i + 1], dir_alias,
                  (MAX_PATH_LENGTH - i - 1)) >= (MAX_PATH_LENGTH - i - 1))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Storage for tmp_file_mask_file not large (%d bytes) enough!",
                 (MAX_PATH_LENGTH - i - 1));
      return;
   }

   if ((fd = coe_open(tmp_file_mask_file, (O_RDWR | O_CREAT | O_TRUNC),
#ifdef GROUP_CAN_WRITE
                      (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))) == -1)
#else
                      (S_IRUSR | S_IWUSR))) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to coe_open() `%s' : %s",
                 tmp_file_mask_file, strerror(errno));
      return;
   }

   if (write(fd, &dir->fgc, sizeof(int)) != sizeof(int))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to write() %d bytes to `%s' : %s",
                 sizeof(int), tmp_file_mask_file, strerror(errno));
      (void)close(fd);
      return;
   }
   for (i = 0; i < dir->fgc; i++)
   {
      length = sizeof(int) + sizeof(int) + alfbl + dir->file[i].fbl;
      mod = length % 4;
      if (mod != 0)
      {
         align_offset = sizeof(int) - mod;
         length += align_offset;
      }
      else
      {
         align_offset = 0;
      }
      fbl = alfbl + dir->file[i].fbl + align_offset;
      if ((buffer = malloc(length)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    length, strerror(errno));
         break;
      }
      fc = alfc + dir->file[i].fc;
      (void)memcpy(buffer, &fc, sizeof(int));
      (void)memcpy(buffer + sizeof(int), &fbl, sizeof(int));
      (void)memcpy(buffer + sizeof(int) + sizeof(int), alfiles, alfbl);
      (void)memcpy(buffer + sizeof(int) + sizeof(int) + alfbl,
                   dir->file[i].files, dir->file[i].fbl);
      if (write(fd, buffer, length) != length)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to write() %d bytes to `%s' : %s",
                    length, tmp_file_mask_file, strerror(errno));
         free(buffer);
         break;
      }
      free(buffer);
   }
   if (close(fd) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__, "Failed to close() `%s' : %s",
                 tmp_file_mask_file, strerror(errno));
   }
   if (rename(tmp_file_mask_file, file_mask_file) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to rename() `%s' to `%s' : %s",
                 tmp_file_mask_file, file_mask_file, strerror(errno));
   }

   return;
}
