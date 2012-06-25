/*
 * rastamenu.c
 *
 * Code file for the Rasta menu objects.
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
#include "rastascope.h"
#include "rastamenu.h"
#include "rastatraverse.h"



/*
 * Structures
 */
struct _RastaMenuItem
{
    gchar *text;
    gchar *next_id;
    gchar *help;
};



/*
 * Prototypes
 */
static void rasta_menu_items_free(GList *menu_items);
static RastaMenuItem *rasta_menu_item_new(RastaContext *ctxt,
                                          xmlNodePtr node);




/*
 * Functions
 */


/*
 * static void rasta_menu_items_free(GList *menu_items)
 *
 * Frees the menu items
 */
static void rasta_menu_items_free(GList *menu_items)
{
    GList *elem;
    RastaMenuItem *item;

    elem = menu_items;
    while (elem != NULL)
    {
        item = (RastaMenuItem *)elem->data;

        g_free(item->text);
        g_free(item->next_id);
        g_free(item);

        elem = g_list_next(elem);
    }

    g_list_free(menu_items);
}  /* rasta_menu_items_frees() */


/*
 * static RastaMenuItem *rasta_menu_item_new(RastaContext *ctxt,
 *                                           xmlNodePtr node)
 *
 * Creates a menu item from a node
 */
static RastaMenuItem *rasta_menu_item_new(RastaContext *ctxt,
                                          xmlNodePtr node)
{
    gchar *id;
    xmlNodePtr screen;
    RastaMenuItem *item;

    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(node != NULL, NULL);

    id = xmlGetProp(node, "NAME");
    if (id == NULL)
        return(NULL);
    if (g_utf8_strlen(id, -1) == 0)
    {
        g_free(id);
        return(NULL);
    }
    screen = rasta_find_screen(ctxt, id);
    if (screen == NULL)
    {
        g_free(id);
        return(NULL);
    }

    item = g_new0(RastaMenuItem, 1);
    if (item == NULL)
    {
        g_free(id);
        return(NULL);
    }

    item->next_id = id;
    item->text = xmlGetProp(screen, "TEXT");
    if (item->text == NULL)
    {
        g_free(id);
        g_free(item);
        return(NULL);
    }

    item->help = rasta_query_help(ctxt, screen);

    return(item);
}  /* rasta_menu_item_new() */


/*
 * void rasta_menu_screen_next(RastaContext *ctxt,
 *                             const gchar *next_id)
 *
 * Selects the menu choice and goes to the next element
 */
void rasta_menu_screen_next(RastaContext *ctxt,
                            const gchar *next_id)
{
    xmlNodePtr node;

    g_return_if_fail(ctxt != NULL);
    g_return_if_fail(next_id != NULL);
    g_return_if_fail(g_utf8_strlen(next_id, -1) > 0);

    node = rasta_traverse_forward(ctxt, next_id);
    rasta_scope_push(ctxt, node);
    rasta_screen_prepare(ctxt);
}  /* rasta_menu_screen_next() */


/*
 * void rasta_menu_screen_load(RastaContext *ctxt,
 *                             RastaScreen *screen,
 *                             xmlNodePtr screen_node)
 *
 * Loads the screen structure from the given XML node
 */
void rasta_menu_screen_load(RastaContext *ctxt,
                            RastaScreen *screen,
                            xmlNodePtr screen_node)
{
    RastaMenuScreen *m_screen;

    g_return_if_fail(screen != NULL);
    g_return_if_fail(screen_node != NULL);
    g_return_if_fail(xmlStrcmp(screen_node->name, "MENUSCREEN") == 0);
    
    m_screen = RASTA_MENU_SCREEN(screen);
    m_screen->title = xmlGetProp(screen_node, "TEXT");
    m_screen->menu_items = NULL;
    m_screen->help = rasta_query_help(ctxt, screen_node);
}  /* rasta_menu_screen_load() */


/*
 * void rasta_menu_screen_init(RastaContext *ctxt,
 *                             RastaScreen *screen)
 *
 * Initializes the menu screen for its current use
 */
void rasta_menu_screen_init(RastaContext *ctxt,
                            RastaScreen *screen)
{
    xmlNodePtr cur;
    RastaScope *scope;
    RastaMenuScreen *m_screen;
    RastaMenuItem *item;

    g_return_if_fail(ctxt != NULL);
    g_return_if_fail(screen != NULL);
    g_return_if_fail(screen->type == RASTA_SCREEN_MENU);

    m_screen = RASTA_MENU_SCREEN(screen);

    rasta_menu_items_free(m_screen->menu_items);
    m_screen->menu_items = NULL;

    scope = rasta_scope_get_current(ctxt);
    cur = rasta_scope_get_path_node(scope);
    cur = cur->children;
    while (cur != NULL)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            ((xmlStrcmp(cur->name, "DIALOG") == 0) ||
             (xmlStrcmp(cur->name, "HIDDEN") == 0) ||
             (xmlStrcmp(cur->name, "MENU") == 0) ||
             (xmlStrcmp(cur->name, "ACTION") == 0)))
        {
            item = rasta_menu_item_new(ctxt, cur);
            if (item != NULL)
                m_screen->menu_items =
                    g_list_append(m_screen->menu_items, item);
        }

        cur = cur->next;
    }

    if (ctxt->state == RASTA_CONTEXT_INITIALIZED)
        ctxt->state = RASTA_CONTEXT_SCREEN;
}  /* rasta_menu_screen_init() */


/*
 * REnumeration *rasta_menu_screen_enumerate_items(RastaContext *ctxt,
 *                                                 RastaScreen *screen)
 *
 * Creates an enumeration object for the menu items
 */
REnumeration *rasta_menu_screen_enumerate_items(RastaContext *ctxt,
                                                RastaScreen *screen)
{
    RastaMenuScreen *m_screen;
    REnumeration *enumer;

    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(screen != NULL, NULL);
    g_return_val_if_fail(screen->type == RASTA_SCREEN_MENU, NULL);

    m_screen = RASTA_MENU_SCREEN(screen);

    enumer = r_enumeration_new_from_list(m_screen->menu_items);

    return(enumer);
}  /* rasta_menu_screen_enumerate_items() */


/*
 * gchar *rasta_menu_item_get_text(RastaMenuItem *item)
 *
 * Returns the text of the menu item
 */
gchar *rasta_menu_item_get_text(RastaMenuItem *item)
{
    g_return_val_if_fail(item != NULL, NULL);

    return(g_strdup(item->text));
}  /* rasta_menu_item_get_text() */


/*
 * gchar *rasta_menu_item_get_id(RastaMenuItem *item)
 *
 * Returns the id of the menu item
 */
gchar *rasta_menu_item_get_id(RastaMenuItem *item)
{
    g_return_val_if_fail(item != NULL, NULL);
    
    return(g_strdup(item->next_id));
}  /* rasta_menu_item_get_id() */


/*
 * gchar *rasta_menu_item_get_help(RastaMenuItem *item)
 *
 * Returns the id of the menu item
 */
gchar *rasta_menu_item_get_help(RastaMenuItem *item)
{
    g_return_val_if_fail(item != NULL, NULL);
    
    return(g_strdup(item->help));
}  /* rasta_menu_item_get_help() */
