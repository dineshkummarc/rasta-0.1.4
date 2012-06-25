/*
 * rastahidden.c
 *
 * Code file for the Rasta hidden objects.
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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "rasta.h"
#include "rastacontext.h"
#include "rastascreen.h"
#include "rastahidden.h"
#include "rastatraverse.h"
#include "rastascope.h"
#include "rastaexec.h"




/*
 * Functions
 */


/*
 * void rasta_hidden_screen_load(RastaContext *ctxt,
 *                               RastaScreen *screen,
 *                               xmlNodePtr screen_node)
 *
 * Loads a hidden screen from the XML
 */
void rasta_hidden_screen_load(RastaContext *ctxt,
                              RastaScreen *screen,
                              xmlNodePtr screen_node)
{
    gchar *ptr;
    RastaHiddenScreen *h_screen;
    xmlNodePtr cur;

    g_return_if_fail(screen != NULL);
    g_return_if_fail(screen_node != NULL);
    g_return_if_fail(xmlStrcmp(screen_node->name, "HIDDENSCREEN") == 0);
    
    h_screen = RASTA_HIDDEN_SCREEN(screen);
    h_screen->title = xmlGetProp(screen_node, "TEXT");
    h_screen->init_command = NULL;
    h_screen->help = rasta_query_help(ctxt, screen_node);

    cur = screen_node->children;
    while (cur != NULL)
    {
        if (cur->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcmp(cur->name, "INITCOMMAND") == 0)
            {
                h_screen->init_command =
                    xmlNodeListGetString(ctxt->doc, cur->children, 1);

                h_screen->encoding = xmlGetProp(cur, "OUTPUTENCODING");

                ptr = xmlGetProp(cur, "ESCAPESTYLE");
                if (ptr == NULL)
                {
                    h_screen->escape_style = RASTA_ESCAPE_STYLE_NONE;
                    continue;
                }

                if (strcmp(ptr, "single") == 0)
                    h_screen->escape_style = RASTA_ESCAPE_STYLE_SINGLE;
                else if (strcmp(ptr, "double") == 0)
                    h_screen->escape_style = RASTA_ESCAPE_STYLE_DOUBLE;
                else
                    h_screen->escape_style = RASTA_ESCAPE_STYLE_NONE;

                g_free(ptr);
            }
        }

        cur = cur->next;
    }
}  /* rasta_hidden_screen_load() */


/*
 * void rasta_hidden_screen_init(RastaContext *ctxt,
 *                               RastaScreen *screen)
 *
 * Initializes the hidden screen for use.  It does initcommand
 * checking too
 */
void rasta_hidden_screen_init(RastaContext *ctxt,
                              RastaScreen *screen)
{
    RastaHiddenScreen *h_screen;

    g_return_if_fail(ctxt != NULL);
    g_return_if_fail(screen != NULL);
    g_return_if_fail(screen->type == RASTA_SCREEN_HIDDEN);

    h_screen = RASTA_HIDDEN_SCREEN(screen);

    if (ctxt->state == RASTA_CONTEXT_INITIALIZED)
        ctxt->state = RASTA_CONTEXT_SCREEN;

    /* Check for the initcommand */
    if (h_screen->init_command != NULL)
    {
        if (ctxt->state == RASTA_CONTEXT_SCREEN)
            ctxt->state = RASTA_CONTEXT_INITCOMMAND;
        else if (ctxt->state == RASTA_CONTEXT_INITCOMMAND)
            ctxt->state = RASTA_CONTEXT_SCREEN;
        else
            g_warning("Invalid context state in INITCOMMAND\n");
    }
}  /* rasta_hidden_screen_init() */


/*
 * void rasta_hidden_screen_next(RastaContext *ctxt)
 *
 * Selects the next screen.
 */
void rasta_hidden_screen_next(RastaContext *ctxt)
{
    xmlNodePtr node;

    g_return_if_fail(ctxt != NULL);

    node = rasta_traverse_forward(ctxt, NULL);
    rasta_scope_push(ctxt, node);
    rasta_screen_prepare(ctxt);
}  /* rasta_dialog_screen_next() */

