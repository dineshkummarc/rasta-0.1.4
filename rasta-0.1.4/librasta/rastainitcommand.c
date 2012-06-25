/*
 * rastainitcommand.c
 *
 * Source for initcommand functions
 *
 * Copyright (C) 2001 Oracle Corporation, Joel Becker
 * <joel.becker@oracle.com> and Manish Singh <manish.singh@oracle.com>
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have recieved a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "rasta.h"
#include "rastacontext.h"
#include "rastascreen.h"
#include "rastadialog.h"
#include "rastahidden.h"
#include "rastaexec.h"



/*
 * Prototypes
 */
static gboolean rasta_initcommand_parse(RastaContext *ctxt,
                                        const gchar *init_text);



/*
 * Functions
 */


/*
 * static gboolean rasta_initcommand_parse(RastaContext *ctxt,
 *                                         const gchar *init_text)
 *
 * Parses the initcommand output and fills the symbol table.
 * Initcommand text is line-by-line, a line containing a variable
 * name followed by a line containing the value.  This can be repeated
 * for as many variables as wanted.  As it is line oriented, if the
 * value contains a newline, it should be embedded as \n.
 */
static gboolean rasta_initcommand_parse(RastaContext *ctxt,
                                        const gchar *init_text)
{
    gchar **lines;
    gint count;

    g_return_val_if_fail(ctxt != NULL, FALSE);
    g_return_val_if_fail(init_text != NULL, FALSE);

    if (init_text[0] == '\0')
        return(TRUE);

    lines = g_strsplit(init_text, "\n", 0);
    if (lines == NULL)
        return(FALSE);

    for (count = 0; lines[count] != NULL; count += 2)
    {
        if (lines[count + 1] == NULL)
        {
            if (lines[count][0] != '\0')
            {
                g_warning("Missing value for variable \"%s\"\n",
                          lines[count]);
            }
            break;
        }

        /* FIXME: Needs unicode s/\\n/\n/g */
        rasta_symbol_put(ctxt, lines[count], lines[count + 1]);
    }

    g_strfreev(lines);

    return(TRUE);
}  /* rasta_initcommand_parse() */


/*
 * gboolean rasta_initcommand_is_required(RastaContext *ctxt,
 *                                        RastaScreen *screen)
 *
 * Returns TRUE if an initcommand is required
 */
gboolean rasta_initcommand_is_required(RastaContext *ctxt,
                                       RastaScreen *screen)
{
    g_return_val_if_fail(screen != NULL, FALSE);
    g_return_val_if_fail((screen->type == RASTA_SCREEN_DIALOG) ||
                         (screen->type == RASTA_SCREEN_HIDDEN), FALSE);

    return(ctxt->state == RASTA_CONTEXT_INITCOMMAND);
}  /* rasta_initcommand_is_required() */


/*
 * gchar *rasta_initcommand_get_encoding(RastaScreen *screen)
 *
 * Returns the encoding for the initcommand
 */
gchar *rasta_initcommand_get_encoding(RastaScreen *screen)
{
    const gchar *encoding;

    g_return_val_if_fail(screen != NULL, NULL);
    g_return_val_if_fail((screen->type == RASTA_SCREEN_DIALOG) ||
                         (screen->type == RASTA_SCREEN_HIDDEN), NULL);

    if (screen->type == RASTA_SCREEN_DIALOG)
        encoding = RASTA_DIALOG_SCREEN(screen)->encoding;
    else if (screen->type == RASTA_SCREEN_HIDDEN)
        encoding = RASTA_HIDDEN_SCREEN(screen)->encoding;

    if (strcmp(encoding, "system") == 0)
        g_get_charset(&encoding);

    return(g_strdup(encoding));
}  /* rasta_initcommand_get_encoding() */


/*
 * void rasta_initcommand_failed(RastaContext *ctxt,
 *                               RastaScreen *screen)
 *
 * Processes a failed initcommand.  This would only be due to
 * an error running the initcommand.  Hopefully, this merely means
 * a resource is temporarily unavailable.  Otherwise the initcommand
 * in the XML is completely invalid.
 */
void rasta_initcommand_failed(RastaContext *ctxt,
                              RastaScreen *screen)
{
    g_return_if_fail(ctxt != NULL);
    g_return_if_fail(screen != NULL);

    ctxt->state = RASTA_CONTEXT_SCREEN;

    rasta_screen_previous(ctxt);
}  /* rasta_initcommand_failed() */


/*
 * void rasta_initcommand_complete(RastaContext *ctxt,
 *                                 RastaScreen *screen,
 *                                 gchar *output_str)
 *
 * Takes a completed initcommand and processes the output that
 * the front-end collected for us.
 */
void rasta_initcommand_complete(RastaContext *ctxt,
                                RastaScreen *screen,
                                gchar *output_str)
{
    g_return_if_fail(ctxt != NULL);
    g_return_if_fail(screen != NULL);
    g_return_if_fail(output_str != NULL);

    if (rasta_initcommand_parse(ctxt, output_str) == FALSE)
    {
        rasta_initcommand_failed(ctxt, screen);
        return;
    }

    switch (screen->type)
    {
        case RASTA_SCREEN_DIALOG:
            rasta_dialog_screen_init(ctxt, screen);
            break;

        case RASTA_SCREEN_HIDDEN:
            rasta_hidden_screen_init(ctxt, screen);
            break;

        default:
            g_assert_not_reached();
    }
}  /* rasta_initcommand_complete() */


/*
 * gint rasta_initcommand_run(RastaContext *ctxt,
 *                            RastaScreen *screen,
 *                            pid_t *pid,
 *                            gint *outfd,
 *                            gint *errfd)
 *
 * Fires off the initcommand, returning the fds and pid to the
 * calling process.  The calling process is responsible for converting
 * the fd output to UTF-8 before passing the data to librasta.
 */
gint rasta_initcommand_run(RastaContext *ctxt,
                           RastaScreen *screen,
                           pid_t *pid,
                           gint *outfd,
                           gint *errfd)
{
    gint rc, l_outfd, l_errfd;
    pid_t l_pid;
    RastaDialogScreen *d_screen;
    RastaHiddenScreen *h_screen;
    gchar *init_command = NULL;
    RastaEscapeStyleType escape_style = 0;
    gchar *utf8_cmd, *locale_cmd;
    gchar *argv[] =
    {
        "/bin/sh",
        "-c",
        NULL,
        NULL
    };


    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->state == RASTA_CONTEXT_INITCOMMAND,
                         -EINVAL);
    g_return_val_if_fail(screen != NULL, -EINVAL);
    g_return_val_if_fail((screen->type == RASTA_SCREEN_DIALOG) ||
                         (screen->type == RASTA_SCREEN_HIDDEN),
                         -EINVAL);

    switch (screen->type)
    {
        case RASTA_SCREEN_DIALOG:
            d_screen = RASTA_DIALOG_SCREEN(screen);
            init_command = d_screen->init_command;
            escape_style = d_screen->escape_style;
            break;
    
        case RASTA_SCREEN_HIDDEN:
            h_screen = RASTA_HIDDEN_SCREEN(screen);
            init_command = h_screen->init_command;
            escape_style = h_screen->escape_style;
            break;

        default:
            g_assert_not_reached();
            break;
    }

    utf8_cmd = rasta_exec_symbol_subst(ctxt,
                                       init_command,
                                       escape_style);

    if (utf8_cmd == NULL)
        return(-ENOMEM);

    locale_cmd = g_locale_from_utf8(utf8_cmd, -1, NULL, NULL, NULL);
    g_free(utf8_cmd);
    if (locale_cmd == NULL)
        return(-ENOMEM);

    argv[2] = locale_cmd;
    rc = rasta_exec_command_v(&l_pid,
                              NULL, &l_outfd, &l_errfd,
                              RASTA_EXEC_FD_NULL,
                              RASTA_EXEC_FD_PIPE,
                              RASTA_EXEC_FD_PIPE,
                              argv);

    g_free(locale_cmd);

    if (pid != NULL)
        *pid = rc ? -1 : l_pid;
    if (outfd != NULL)
        *outfd = rc ? -1 : l_outfd;
    if (errfd != NULL)
        *errfd = rc ? -1 : l_errfd;

    return(rc);
}  /* rasta_initcommand_run() */

