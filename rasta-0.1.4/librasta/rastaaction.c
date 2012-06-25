/*
 * rastaaction.c
 *
 * Code file for the Rasta action objects.
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

#include <string.h>
#include <sys/types.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "rasta.h"
#include "rastacontext.h"
#include "rastascope.h"
#include "rastascreen.h"
#include "rastaaction.h"
#include "rastatraverse.h"
#include "rastaexec.h"


/*
 * Prototypes
 */



/*
 * Functions
 */


/*
 * void rasta_action_screen_load(RastaContext *ctxt,
 *                               RastaScreen *screen,
 *                               xmlNodePtr screen_node)
 *
 * Loads an action screen from the XML description.
 */
void rasta_action_screen_load(RastaContext *ctxt,
                              RastaScreen *screen,
                              xmlNodePtr screen_node)
{
    gchar *tmp;
    RastaActionScreen *a_screen;
    xmlNodePtr command_node;

    g_return_if_fail(screen != NULL);
    g_return_if_fail(screen_node != NULL);
    g_return_if_fail(xmlStrcmp(screen_node->name, "ACTIONSCREEN") == 0);
    
    a_screen = RASTA_ACTION_SCREEN(screen);
    a_screen->title = xmlGetProp(screen_node, "TEXT");
    a_screen->help = rasta_query_help(ctxt, screen_node);

    tmp = xmlGetProp(screen_node, "TTY");
    if (tmp == NULL)
        a_screen->tty_type = RASTA_TTY_YES;
    else
    {
        if (strcmp(tmp, "yes") == 0)
            a_screen->tty_type = RASTA_TTY_YES;
        else if (strcmp(tmp, "no") == 0)
            a_screen->tty_type = RASTA_TTY_NO;
        else if (strcmp(tmp, "self") == 0)
            a_screen->tty_type = RASTA_TTY_SELF;
        else
        {
            g_warning("Invalid TTY type: %s", tmp);
            a_screen->tty_type = RASTA_TTY_YES;
        }

        g_free(tmp);
    }

    tmp = xmlGetProp(screen_node, "CONFIRM");
    if (tmp == NULL)
        a_screen->confirm = FALSE;
    else
    {
        if (strcmp(tmp, "true") == 0)
            a_screen->confirm = TRUE;
        else if (strcmp(tmp, "false") == 0)
            a_screen->confirm = FALSE;
        else
        {
            g_warning("Invalid CONFIRM value: %s", tmp);
            a_screen->confirm = FALSE;
        }

        g_free(tmp);
    }

    command_node = screen_node->children;
    while (command_node != NULL)
    {
        if ((command_node->type == XML_ELEMENT_NODE) &&
            (xmlStrcmp(command_node->name, "ACTIONCOMMAND") == 0))
            break;

        command_node = command_node->next;
    }

    if (command_node == NULL)
    {
        a_screen->command = NULL;
        return;
    }

    tmp = xmlGetProp(command_node, "ESCAPESTYLE");
    if (tmp == NULL)
        a_screen->escape_style = RASTA_ESCAPE_STYLE_NONE;
    else
    {
        if (strcmp(tmp, "single") == 0)
            a_screen->escape_style = RASTA_ESCAPE_STYLE_SINGLE;
        else if (strcmp(tmp, "double") == 0)
            a_screen->escape_style = RASTA_ESCAPE_STYLE_DOUBLE;
        else if (strcmp(tmp, "none") == 0)
            a_screen->escape_style = RASTA_ESCAPE_STYLE_NONE;
        else
        {
            g_warning("Invalid ESCAPESTYLE: %s", tmp);
            a_screen->escape_style = RASTA_ESCAPE_STYLE_NONE;
        }

        g_free(tmp);
    }

    a_screen->encoding = xmlGetProp(command_node, "OUTPUTENCODING");
    if (a_screen->encoding == NULL)  /* shouldn't happen */
        a_screen->encoding = g_strdup("system");

    a_screen->command =
        xmlNodeListGetString(ctxt->doc, command_node->children, 1);

}  /* rasta_action_screen_load() */


/*
 * void rasta_action_screen_init(RastaContext *ctxt,
 *                               RastaScreen *screen)
 *
 * Initializes the action screen for use.  This is nothing, really.
 */
void rasta_action_screen_init(RastaContext *ctxt,
                              RastaScreen *screen)
{
    g_return_if_fail(ctxt != NULL);
    g_return_if_fail(screen != NULL);
    g_return_if_fail(screen->type == RASTA_SCREEN_ACTION);

    if (ctxt->state == RASTA_CONTEXT_INITIALIZED)
        ctxt->state = RASTA_CONTEXT_SCREEN;
}  /* rasta_action_screen_init() */


/*
 * gchar *rasta_action_screen_get_command(RastaContext *ctxt,
 *                                        RastaScreen *screen)
 *
 * Returns the substituted command for the front-end to run.  This
 * command is in UTF-8.  The calling process is responsible for any
 * locale conversion required.
 */
gchar *rasta_action_screen_get_command(RastaContext *ctxt,
                                       RastaScreen *screen)
{
    RastaActionScreen *a_screen;
    gchar *cmd;
    
    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(screen != NULL, NULL);

    a_screen = RASTA_ACTION_SCREEN(screen);

    cmd = rasta_exec_symbol_subst(ctxt,
                                  a_screen->command,
                                  a_screen->escape_style);

    return(cmd);
}  /* rasta_action_screen_get_command() */


/*
 * RastaTTYType rasta_action_screen_get_tty_type(RastaScreen *screen)
 *
 * Returns the TTY type for this screen.
 */
RastaTTYType rasta_action_screen_get_tty_type(RastaScreen *screen)
{
    RastaActionScreen *a_screen;

    g_return_val_if_fail(screen != NULL, RASTA_TTY_YES);

    a_screen = RASTA_ACTION_SCREEN(screen);

    return(a_screen->tty_type);
}  /* rasta_action_screen_get_tty_type() */


/*
 * gchar *rasta_action_screen_get_encoding(RastaScreen *screen)
 *
 * Returns the encoding for the action's output.  The special value
 * "system" (the default) returns the current locale encoding.
 */
gchar *rasta_action_screen_get_encoding(RastaScreen *screen)
{
    const gchar *encoding;

    g_return_val_if_fail(screen != NULL, NULL);
    g_return_val_if_fail(screen->type == RASTA_SCREEN_ACTION, NULL);

    encoding = RASTA_ACTION_SCREEN(screen)->encoding;

    if (strcmp(encoding, "system") == 0)
        g_get_charset(&encoding);

    return(g_strdup(encoding));
}  /* rasta_action_screen_get_encoding() */


/*
 * gboolean rasta_action_screen_needs_confirm(RastaScreen *screen)
 *
 * Returns the TRUE if the action screen requires confirmation to
 * continue.
 */
gboolean rasta_action_screen_needs_confirm(RastaScreen *screen)
{
    RastaActionScreen *a_screen;

    g_return_val_if_fail(screen != NULL, FALSE);

    a_screen = RASTA_ACTION_SCREEN(screen);

    return(a_screen->confirm);
}  /* rasta_action_screen_needs_confirm() */
