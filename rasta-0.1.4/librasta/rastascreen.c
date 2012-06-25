/*
 * rastascreen.c
 *
 * Generic screen functions for the RastaScreen object
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

#include <sys/types.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "rasta.h"
#include "rastacontext.h"
#include "rastascreen.h"
#include "rastadialog.h"
#include "rastahidden.h"
#include "rastamenu.h"
#include "rastaaction.h"
#include "rastatraverse.h"
#include "rastascope.h"




/*
 * Prototypes
 */
static RastaScreen *rasta_screen_load(RastaContext *ctxt,
                                      const gchar *id);
static void rasta_screen_init(RastaContext *ctxt,
                              RastaScreen *screen);
static RastaScreen *rasta_screen_cache_lookup(RastaContext *ctxt,
                                              const gchar *id);
static void rasta_screen_cache_put(RastaContext *ctxt,
                                   RastaScreen *screen);


/*
 * Functions
 */


/*
 * RastaScreenType rasta_screen_get_type(RastaScreen *screen)
 *
 * Returns the type of the given screen
 */
RastaScreenType rasta_screen_get_type(RastaScreen *screen)
{
    g_return_val_if_fail(screen != NULL, RASTA_SCREEN_NONE);

    return(screen->type);
}  /* rasta_screen_get_type() */


/*
 * gchar *rasta_screen_get_title(RastaScreen *screen)
 * 
 * Returns the title of the given screen
 */
gchar *rasta_screen_get_title(RastaScreen *screen)
{
    RastaAnyScreen *any;

    g_return_val_if_fail(screen != NULL, NULL);

    any = RASTA_ANY_SCREEN(screen);

    return(g_strdup(any->title));
}  /* rasta_screen_get_title() */


/*
 * void rasta_screen_previous(RastaContext *ctxt)
 *
 * Backs up to the previous screen
 */
void rasta_screen_previous(RastaContext *ctxt)
{
    RastaScope *scope;
    RastaScreen *screen;

    do
    {
        rasta_scope_pop(ctxt);
        scope = rasta_scope_get_current(ctxt);
        screen = rasta_scope_get_screen(scope);
    }
    while (screen->type == RASTA_SCREEN_HIDDEN);

    /* Clear any initcommand the old screen wanted */
    if (ctxt->state == RASTA_CONTEXT_INITCOMMAND)
        ctxt->state = RASTA_CONTEXT_SCREEN;

    rasta_screen_init(ctxt, screen);

    /* Clear any initcommand again, so as to not trounce user values */
    if (ctxt->state == RASTA_CONTEXT_INITCOMMAND)
        ctxt->state = RASTA_CONTEXT_SCREEN;
}  /* rasta_screen_previous() */


/*
 * void rasta_screen_prepare(RastaContext *ctxt)
 *
 * Prepares and loads a screen once it is found.
 */
void rasta_screen_prepare(RastaContext *ctxt)
{
    RastaScope *scope;
    RastaScreen *screen;
    gchar *id;
    
    g_return_if_fail(ctxt != NULL);

    scope = rasta_scope_get_current(ctxt);
    id = rasta_scope_get_id(scope);
    if (id == NULL)  /* Invalid scope */
        return;

    screen = rasta_screen_load(ctxt, id);
    g_free(id);
    rasta_scope_set_screen(scope, screen);
    rasta_screen_init(ctxt, screen);
}  /* rasta_screen_prepare() */


/*
 * static RastaScreen *rasta_screen_load(RastaContext *ctxt,
 *                                       const gchar *id)
 *
 * This loads the RastaScreen object from the XML data.  This should
 * only get called once per screen, as the screen cache will hold it.
 */
static RastaScreen *rasta_screen_load(RastaContext *ctxt,
                                      const gchar *id)
{
    RastaScreen *screen;
    xmlNodePtr screen_node;

    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(id != NULL, NULL);
    g_return_val_if_fail(xmlStrlen(id) != 0, NULL);

    screen = rasta_screen_cache_lookup(ctxt, id);
    if (screen != NULL)
        return(screen);

    screen_node = rasta_find_screen(ctxt, id);
    if (screen_node == NULL)
        return(NULL);

    screen = g_new0(RastaScreen, 1);
    if (screen == NULL)
        return(NULL);

    if (xmlStrcmp(screen_node->name, "MENUSCREEN") == 0)
    {
        screen->type = RASTA_SCREEN_MENU;
        rasta_menu_screen_load(ctxt, screen, screen_node);
    }
    else if (xmlStrcmp(screen_node->name, "DIALOGSCREEN") == 0)
    {
        screen->type = RASTA_SCREEN_DIALOG;
        rasta_dialog_screen_load(ctxt, screen, screen_node);
    }
    else if (xmlStrcmp(screen_node->name, "HIDDENSCREEN") == 0)
    {
        screen->type = RASTA_SCREEN_HIDDEN;
        rasta_hidden_screen_load(ctxt, screen, screen_node);
    }
    else if (xmlStrcmp(screen_node->name, "ACTIONSCREEN") == 0)
    {
        screen->type = RASTA_SCREEN_ACTION;
        rasta_action_screen_load(ctxt, screen, screen_node);
    }
    else
        screen->type = RASTA_SCREEN_NONE;

    rasta_screen_cache_put(ctxt, screen);

    return(screen);
}  /* rasta_screen_load() */


/*
 * static void rasta_screen_init(RastaContext *ctxt,
 *                               RastaScreen *screen)
 *
 * This initializes a screen every time it is used.  It handles
 * setting up initcommands and such.
 */
static void rasta_screen_init(RastaContext *ctxt,
                              RastaScreen *screen)
{
    g_return_if_fail(screen != NULL);

    if (screen->type == RASTA_SCREEN_MENU)
        rasta_menu_screen_init(ctxt, screen);
    else if (screen->type == RASTA_SCREEN_DIALOG)
        rasta_dialog_screen_init(ctxt, screen);
    else if (screen->type == RASTA_SCREEN_HIDDEN)
        rasta_hidden_screen_init(ctxt, screen);
    else if (screen->type == RASTA_SCREEN_ACTION)
        rasta_action_screen_init(ctxt, screen);
 }  /* rasta_screen_init() */


/*
 * static RastaScreen *rasta_screen_cache_lookup(RastaContext *ctxt,
 *                                               const gchar *id)
 *
 * Checks for a screen entry in the screen cache
 */
static RastaScreen *rasta_screen_cache_lookup(RastaContext *ctxt,
                                              const gchar *id)
{
    RastaScreen *screen;

    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(id != NULL, NULL);
    g_return_val_if_fail(g_utf8_strlen(id, -1) != 0, NULL);

    if (ctxt->screen_cache == NULL)
        return(NULL);

    screen = RASTA_SCREEN(g_hash_table_lookup(ctxt->screen_cache,
                                              id));

    return(screen);
}  /* rasta_screen_cache_lookup() */


/*
 * static void rasta_screen_cache_put(RastaContext *ctxt,
 *                                    RastaScreen *screen)
 *
 * Inserts a screen cache entry
 */
static void rasta_screen_cache_put(RastaContext *ctxt,
                                   RastaScreen *screen)
{
    gchar *id;
    RastaAnyScreen *any;

    g_return_if_fail(ctxt != NULL);
    g_return_if_fail(screen != NULL);

    any = RASTA_ANY_SCREEN(screen);

    id = g_strdup(any->title);

    if (ctxt->screen_cache == NULL)
    {
        ctxt->screen_cache = g_hash_table_new(g_str_hash, g_str_equal);
        if (ctxt->screen_cache == NULL)
        {
            g_free(id);
            return;
        }
    }

    g_hash_table_insert(ctxt->screen_cache, id, screen);
}  /* rasta_screen_cache_put() */

