/*
 *  print_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2020 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   print_data - prints data from the output log
 **
 ** SYNOPSIS
 **   void print_data(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.05.1997 H.Kiehl Created
 **   18.03.2000 H.Kiehl Modified to make it more generic.
 **   10.04.2004 H.Kiehl Added TLS/SSL support.
 **   31.01.2006 H.Kiehl Added SFTP support.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* snprintf(), pclose()                 */
#include <string.h>              /* strcpy(), strcat(), strerror()       */
#include <stdlib.h>              /* exit()                               */
#include <unistd.h>              /* write(), close()                     */
#include <time.h>                /* strftime()                           */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/List.h>
#include <Xm/LabelP.h>
#include <errno.h>
#include "mafd_ctrl.h"
#include "show_olog.h"

/* External global variables. */
extern Display      *display;
extern Widget       listbox_w,
                    printshell,
                    statusbox_w,
                    summarybox_w;
extern char         file_name[],
                    header_line[],
                    **search_file_name,
                    **search_dir,
                    **search_recipient,
                    search_file_size_str[],
                    summary_str[],
                    total_summary_str[];
extern int          items_selected,
                    no_of_search_dirs,
                    no_of_search_dirids,
                    no_of_search_file_names,
                    no_of_search_hosts,
                    no_of_search_jobids,
                    sum_line_length;
extern unsigned int *search_dirid,
                    *search_jobid;
extern XT_PTR_TYPE  device_type,
                    range_type,
                    toggles_set;
extern time_t       start_time_val,
                    end_time_val;
extern FILE         *fp;

/* Local function prototypes. */
static void         write_header(int, char *),
                    write_summary(int, char *);


/*######################### print_data_button() #########################*/
void
print_data_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   char message[MAX_MESSAGE_LENGTH],
        sum_sep_line[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 1];

   /* Prepare separator line. */
   (void)memset(sum_sep_line, '=', sum_line_length);
   sum_sep_line[sum_line_length] = '\0';

   if (range_type == SELECTION_TOGGLE)
   {
      int no_selected,
          *select_list;

      if (XmListGetSelectedPos(listbox_w, &select_list, &no_selected) == False)
      {
         show_message(statusbox_w, "No data selected for printing!");
         XtPopdown(printshell);

         return;
      }
      else
      {
         int           fd,
                       prepare_status;
         char          *line,
                       line_buffer[256];
         XmStringTable all_items;

         prepare_status = prepare_file(&fd, (device_type == MAIL_TOGGLE) ? 0 : 1);
         if ((prepare_status != SUCCESS) && (device_type == MAIL_TOGGLE))
         {
            prepare_tmp_name();
            prepare_status = prepare_file(&fd, 1);
         }
         if (prepare_status == SUCCESS)
         {
            register int i,
                         length;

            write_header(fd, sum_sep_line);

            XtVaGetValues(listbox_w, XmNitems, &all_items, NULL);
            for (i = 0; i < no_selected; i++)
            {
               XmStringGetLtoR(all_items[select_list[i] - 1],
                               XmFONTLIST_DEFAULT_TAG, &line);
               length = snprintf(line_buffer, 256, "%s\n", line);
               if (length > 256)
               {
                  (void)fprintf(stderr, "Buffer to small %d > 256 (%s %d)\n",
                               length, __FILE__, __LINE__);
                  length = 256;
               }
               if (write(fd, line_buffer, length) != length)
               {
                  (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
                  XtFree(line);
                  exit(INCORRECT);
               }
               XtFree(line);
               XmListDeselectPos(listbox_w, select_list[i]);
            }
            write_summary(fd, sum_sep_line);
            items_selected = NO;

            /*
             * Remember to insert the correct summary, since all files
             * have now been deselected.
             */
            (void)strcpy(summary_str, total_summary_str);
            SHOW_SUMMARY_DATA();

            if (device_type == PRINTER_TOGGLE)
            {
               if (close(fd) < 0)
               {
                  (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
               }
               send_print_cmd(message, MAX_MESSAGE_LENGTH);
            }
            else
            {
               if (close(fd) < 0)
               {
                  (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
               }
               if (device_type == MAIL_TOGGLE)
               {
                  send_mail_cmd(message, MAX_MESSAGE_LENGTH);
               }
               else
               {
                  (void)snprintf(message, MAX_MESSAGE_LENGTH,
                                 "Send job to file %s.", file_name);
               }
            }
         }
         XtFree((char *)select_list);
      }
   }
   else /* Print everything! */
   {
      int           fd,
                    no_of_items,
                    prepare_status;
      char          *line,
                    line_buffer[256];
      XmStringTable all_items;

      prepare_status = prepare_file(&fd, (device_type == MAIL_TOGGLE) ? 0 : 1);
      if ((prepare_status != SUCCESS) && (device_type == MAIL_TOGGLE))
      {
         prepare_tmp_name();
         prepare_status = prepare_file(&fd, 1);
      }
      if (prepare_status == SUCCESS)
      {
         register int i,
                      length;

         write_header(fd, sum_sep_line);

         XtVaGetValues(listbox_w,
                       XmNitemCount, &no_of_items,
                       XmNitems,     &all_items,
                       NULL);
         for (i = 0; i < no_of_items; i++)
         {
            XmStringGetLtoR(all_items[i], XmFONTLIST_DEFAULT_TAG, &line);
            length = snprintf(line_buffer, 256, "%s\n", line);
            if (length > 256)
            {
               (void)fprintf(stderr, "Buffer to small %d > 256 (%s %d)\n",
                            length, __FILE__, __LINE__);
               length = 256;
            }
            if (write(fd, line_buffer, length) != length)
            {
               (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               XtFree(line);
               exit(INCORRECT);
            }
            XtFree(line);
         }
         write_summary(fd, sum_sep_line);

         if (device_type == PRINTER_TOGGLE)
         {
            if (close(fd) < 0)
            {
               (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
            }
            send_print_cmd(message, MAX_MESSAGE_LENGTH);
         }
         else
         {
            if (close(fd) < 0)
            {
               (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
            }
            if (device_type == MAIL_TOGGLE)
            {
               send_mail_cmd(message, MAX_MESSAGE_LENGTH);
            }
            else
            {
               (void)snprintf(message, MAX_MESSAGE_LENGTH,
                              "Send job to file %s.", file_name);
            }
         }
      }
   }

   show_message(statusbox_w, message);
   XtPopdown(printshell);

   return;
}


/*--------------------------- write_header() ----------------------------*/
static void
write_header(int fd, char *sum_sep_line)
{
   int  length,
        tmp_length;
   char buffer[1024];

   length = snprintf(buffer, 1024,
                     "                                AFD OUTPUT LOG\n\n");

   if ((start_time_val < 0) && (end_time_val < 0))
   {
      length += snprintf(&buffer[length], 1024 - length,
                         "\tTime Interval : earliest entry - latest entry");
   }
   else if ((start_time_val > 0) && (end_time_val < 0))
        {
           length += strftime(&buffer[length], 1024 - length,
                              "\tTime Interval : %m.%d. %H:%M",
                              localtime(&start_time_val));
           length += snprintf(&buffer[length], 1024 - length, " - latest entry");
        }
        else if ((start_time_val < 0) && (end_time_val > 0))
             {
                length += strftime(&buffer[length], 1024 - length,
                                   "\tTime Interval : earliest entry - %m.%d. %H:%M",
                                   localtime(&end_time_val));
             }
             else
             {
                length += strftime(&buffer[length], 1024 - length,
                                   "\tTime Interval : %m.%d. %H:%M",
                                   localtime(&start_time_val));
                length += strftime(&buffer[length], 1024 - length,
                                   " - %m.%d. %H:%M",
                                   localtime(&end_time_val));
             }
   if (length >= 1024)
   {
      length = 1024;
      goto write_data;
   }

   if (no_of_search_file_names > 1)
   {
      int i;

      length += snprintf(&buffer[length], 1024 - length,
                         "\n\tFile name     : %s\n", search_file_name[0]);
      if (length > 1024)
      {
         length = 1024;
         goto write_data;
      }
      for (i = 1; i < no_of_search_file_names; i++)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\t                %s\n", search_file_name[i]);
         if (length >= 1024)
         {
            length = 1024;
            goto write_data;
         }
      }
      length += snprintf(&buffer[length], 1024 - length,
                         "\tFile size     : %s\n", search_file_size_str);
   }
   else
   {
      length += snprintf(&buffer[length], 1024 - length,
                         "\n\tFile name     :\n\tFile size     : %s\n",
                         search_file_size_str);
   }
   if (length >= 1024)
   {
      length = 1024;
      goto write_data;
   }

   if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))
   {
      int i;

      if (no_of_search_dirs > 0)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tDirectory     : %s\n", search_dir[0]);
         if (length > 1024)
         {
            length = 1024;
            goto write_data;
         }
         for (i = 1; i < no_of_search_dirs; i++)
         {
            length += snprintf(&buffer[length], 1024 - length,
                               "\t                %s\n", search_dir[i]);
            if (length >= 1024)
            {
               length = 1024;
               goto write_data;
            }
         }
      }
      if (no_of_search_dirids > 0)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tDir Identifier: %x", search_dirid[0]);
         if (length >= 1024)
         {
            length = 1024;
            goto write_data;
         }
         for (i = 1; i < no_of_search_dirids; i++)
         {
            length += snprintf(&buffer[length], 1024 - length,
                               ", %x", search_dirid[i]);
            if (length >= 1024)
            {
               length = 1024;
               goto write_data;
            }
         }
         if (length == 1024)
         {
            buffer[1023] = '\n';
         }
         else
         {
            buffer[length] = '\n';
            length++;
         }
      }
   }
   else
   {
      length += snprintf(&buffer[length], 1024 - length, "\tDirectory     :\n");
   }
   if (length >= 1024)
   {
      length = 1024;
      goto write_data;
   }

   if (no_of_search_hosts > 0)
   {
      int i;

      length += snprintf(&buffer[length], 1024 - length, "\tRecipient     : %s",
                        search_recipient[0]);
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
      for (i = 1; i < no_of_search_hosts; i++)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            ", %s", search_recipient[i]);
         if (length >= 1024)
         {
            length = 1024;
            goto write_data;
         }
      }
      if (length == 1024)
      {
         buffer[1023] = '\n';
      }
      else
      {
         buffer[length] = '\n';
         length++;
      }
   }
   else
   {
      length += snprintf(&buffer[length], 1024 - length, "\tRecipient     :\n");
   }
   if (length >= 1024)
   {
      length = 1024;
      goto write_data;
   }

   if (no_of_search_jobids > 0)
   {
      int i;

      length += snprintf(&buffer[length], 1024 - length,
                         "\tJob ID        : %x", search_jobid[0]);
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
      for (i = 1; i < no_of_search_jobids; i++)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            ", %x", search_jobid[i]);
         if (length >= 1024)
         {
            length = 1024;
            goto write_data;
         }
      }
      if (length == 1024)
      {
         buffer[1023] = '\n';
      }
      else
      {
         buffer[length] = '\n';
         length++;
      }
   }     
   else
   {
      length += snprintf(&buffer[length], 1024 - length, "\tJob ID        :\n");
   }

   tmp_length = length;
#ifdef _WITH_FTP_SUPPORT
   if (toggles_set & SHOW_FTP)
   {
      length += snprintf(&buffer[length], 1024 - length,
                         "\tProtocol      : FTP");
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
#endif
#ifdef _WITH_SMTP_SUPPORT
   if (toggles_set & SHOW_SMTP)
   {
      if (length == tmp_length)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tProtocol      : %s", SMTP_ID_STR);
      }
      else
      {
         length += snprintf(&buffer[length], 1024 - length,
                            ", %s", SMTP_ID_STR);
      }
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
#endif
#ifdef _WITH_DE_MAIL_SUPPORT
   if (toggles_set & SHOW_DEMAIL)
   {
      if (length == tmp_length)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tProtocol      : DEMAIL");
      }
      else
      {
         length += snprintf(&buffer[length], 1024 - length, ", DEMAIL");
      }
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
#endif
#ifdef _WITH_LOC_SUPPORT
   if (toggles_set & SHOW_FILE)
   {
      if (length == tmp_length)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tProtocol      : %s", FILE_ID_STR);
      }
      else
      {
         length += snprintf(&buffer[length], 1024 - length,
                            ", %s", FILE_ID_STR);
      }
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
   if (toggles_set & SHOW_EXEC)
   {
      if (length == tmp_length)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tProtocol      : %s", EXEC_ID_STR);
      }
      else
      {
         length += snprintf(&buffer[length], 1024 - length, ", %s", EXEC_ID_STR);
      }
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
#endif
#ifdef _WITH_SFTP_SUPPORT
   if (toggles_set & SHOW_SFTP)
   {
      if (length == tmp_length)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tProtocol      : SFTP");
      }
      else
      {
         length += snprintf(&buffer[length], 1024 - length, ", SFTP");
      }
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
#endif
#ifdef _WITH_SCP_SUPPORT
   if (toggles_set & SHOW_SCP)
   {
      if (length == tmp_length)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tProtocol      : SCP");
      }
      else
      {
         length += snprintf(&buffer[length], 1024 - length, ", SCP");
      }
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
#endif
#ifdef _WITH_WMO_SUPPORT
   if (toggles_set & SHOW_WMO)
   {
      if (length == tmp_length)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tProtocol      : WMO");
      }
      else
      {
         length += snprintf(&buffer[length], 1024 - length, ", WMO");
      }
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
#endif
#ifdef _WITH_MAP_SUPPORT
   if (toggles_set & SHOW_MAP)
   {
      if (length == tmp_length)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tProtocol      : MAP");
      }
      else
      {
         length += snprintf(&buffer[length], 1024 - length, ", MAP");
      }
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
#endif
#ifdef _WITH_DFAX_SUPPORT
   if (toggles_set & SHOW_DFAX)
   {
      if (length == tmp_length)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tProtocol      : DFAX");
      }
      else
      {
         length += snprintf(&buffer[length], 1024 - length, ", DFAX");
      }
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
#endif
#ifdef WITH_SSL
# ifdef _WITH_FTP_SUPPORT
   if (toggles_set & SHOW_FTPS)
   {
      if (length == tmp_length)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tProtocol      : FTPS");
      }
      else
      {
         length += snprintf(&buffer[length], 1024 - length, ", FTPS");
      }
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
# endif
# ifdef _WITH_HTTP_SUPPORT
   if (toggles_set & SHOW_HTTPS)
   {
      if (length == tmp_length)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tProtocol      : HTTPS");
      }
      else
      {
         length += snprintf(&buffer[length], 1024 - length, ", HTTPS");
      }
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
# endif
# ifdef _WITH_SMTP_SUPPORT
   if (toggles_set & SHOW_SMTPS)
   {
      if (length == tmp_length)
      {
         length += snprintf(&buffer[length], 1024 - length,
                            "\tProtocol      : SMTPS");
      }
      else
      {
         length += snprintf(&buffer[length], 1024 - length, ", SMTPS");
      }
      if (length >= 1024)
      {
         length = 1024;
         goto write_data;
      }
   }
# endif
#endif

   /* Don't forget the heading for the data. */
   length += snprintf(&buffer[length], 1024 - length, "\n\n%s\n%s\n",
                      header_line, sum_sep_line);
   if (length > 1024)
   {
      length = 1024;
   }

write_data:
   /* Write heading to file/printer. */
   if (write(fd, buffer, length) != length)
   {
      (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}


/*-------------------------- write_summary() ----------------------------*/
static void
write_summary(int fd, char *sum_sep_line)
{
   int  length;
   char buffer[1024];

   length = snprintf(buffer, 1024, "%s\n%s\n", sum_sep_line, summary_str);
   if (length > 1024)
   {
      length = 1024;
   }

   /* Write summary to file/printer. */
   if (write(fd, buffer, length) != length)
   {
      (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}
