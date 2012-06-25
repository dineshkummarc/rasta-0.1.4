/*
 * rastascope.c
 *
 * Code for scope handling in rasta
 *
 * Copyright (c) 2001 Oracle Corporation, Joel Becker
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
#include <errno.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "rasta.h"
#include "rastacontext.h"
#include "rastascope.h"



/*
 * Structures
 */
struct _RastaScope
{
    gchar *id;
    xmlNodePtr path_node;
    RastaScreen *screen;
    GHashTable *symbols;
};



/*
 * prototypes
 */
static RastaScope *rasta_scope_new(xmlNodePtr node);
static void rasta_scope_free(RastaScope *scope);
static void rasta_scope_build_symbol_xml(gpointer key,
                                         gpointer value,
                                         gpointer user_data);



/*
 * Functions
 */


/*
 * static RastaScope *rasta_scope_new(xmlNodePtr node)
 *
 * Creates a new RastaScope object
 */
static RastaScope *rasta_scope_new(xmlNodePtr node)
{
    RastaScope *scope;

    scope = g_new0(RastaScope, 1);
    if (scope == NULL)
        return(NULL);

#ifdef DEBUG
    g_print("Creating new scope\n");
#endif

    if (node == NULL)
        return(scope);  /* Scope with null node means no screen found */

    scope->path_node = node;

    scope->id = xmlGetProp(scope->path_node, "NAME");
    if ((scope->id == NULL) || (g_utf8_strlen(scope->id, -1) == 0))
    {
        rasta_scope_free(scope);
        return(NULL);
    }
#ifdef DEBUG
    g_print("Scope id is \"%s\"\n", scope->id);
#endif

    scope->symbols = g_hash_table_new_full(g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           g_free);
    if (scope->symbols == NULL)
    {
        rasta_scope_free(scope);
        return(NULL);
    }

    return(scope);
}  /* rasta_scope_new() */


/*
 * static void rasta_scope_free(RastaScope *scope)
 *
 * Destroys a scope object
 */
static void rasta_scope_free(RastaScope *scope)
{
    if (scope->id != NULL)
        g_free(scope->id);
    if (scope->symbols != NULL)
        g_hash_table_destroy(scope->symbols);

    g_free(scope);
}  /* rasta_scope_free() */


/*
 * void rasta_scope_push(RastaContext *ctxt, xmlNodePtr node)
 *
 * Pushes a new scope onto the scope stack
 */
void rasta_scope_push(RastaContext *ctxt, xmlNodePtr node)
{
    RastaScope *scope;

    g_return_if_fail(ctxt != NULL);

    scope = rasta_scope_new(node);
    if (scope == NULL)
        return;

    ctxt->scopes = g_list_prepend(ctxt->scopes, scope);
}  /* rasta_scope_push() */


/*
 * gboolean rasta_scope_is_top(RastaContext *ctxt)
 *
 * Returns TRUE if the context is at the initial scope
 */
gboolean rasta_scope_is_top(RastaContext *ctxt)
{
    g_return_val_if_fail(ctxt != NULL, TRUE);
    g_assert(ctxt->scopes != NULL);

    return(ctxt->scopes->next == NULL);
}  /* rasta_scope_is_top() */


/*
 * void rasta_scope_pop(RastaContext *ctxt)
 *
 * Pops a scope from the stack
 */
void rasta_scope_pop(RastaContext *ctxt)
{
    RastaScope *scope;
    GList *elem;

    g_return_if_fail(ctxt != NULL);

    g_assert(ctxt->scopes != NULL);

    if (rasta_scope_is_top(ctxt))
        return;

    elem = ctxt->scopes;
    scope = RASTA_SCOPE(elem->data);
    ctxt->scopes = g_list_remove_link(ctxt->scopes, elem);
    g_list_free(elem);

    rasta_scope_free(scope);
}  /* rasta_scope_pop() */


/*
 * RastaScope *rasta_scope_get_current(RastaContext *ctxt)
 *
 * Returns the current scope object
 */
RastaScope *rasta_scope_get_current(RastaContext *ctxt)
{
    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(ctxt->scopes != NULL, NULL);

    return(RASTA_SCOPE(ctxt->scopes->data));
}  /* rasta_scope_get_current() */


/*
 * gchar *rasta_scope_get_id(RastaScope *scope)
 *
 * Returns the id of the passed scope
 */
gchar *rasta_scope_get_id(RastaScope *scope)
{
    g_return_val_if_fail(scope != NULL, NULL);
    
    return(g_strdup(scope->id));
}  /* rasta_scope_get_id() */


/*
 * RastaScreen *rasta_scope_get_screen(RastaScope *scope)
 *
 * Gets the screen object for the passed scope
 */
RastaScreen *rasta_scope_get_screen(RastaScope *scope)
{
    g_return_val_if_fail(scope != NULL, NULL);

    return(scope->screen);
}  /* rasta_scope_get_screen() */


/*
 * void rasta_scope_set_screen(RastaScope *scope,
 *                             RastaScreen *screen);
 *
 * Sets the screen for the current scope
 */
void rasta_scope_set_screen(RastaScope *scope,
                            RastaScreen *screen)
{
    g_return_if_fail(scope != NULL);
    g_return_if_fail(screen != NULL);

    scope->screen = screen;
}  /* rasta_scope_set_screen() */


/*
 * xmlNodePtr rasta_scope_get_path_node(RastaScope *scope)
 *
 * Returns the path node for the current scope
 */
xmlNodePtr rasta_scope_get_path_node(RastaScope *scope)
{
    g_return_val_if_fail(scope != NULL, NULL);

    return(scope->path_node);
}  /* rasta_scope_get_path_node() */


/*
 * void rasta_scope_set_path_node(RastaScope *scope,
 *                                xmlNodePtr node)
 *
 * Sets the path node for the scope
 * (This isn't used afaik anywhere, but for completeness)
 */
void rasta_scope_set_path_node(RastaScope *scope,
                               xmlNodePtr node)
{
    g_return_if_fail(scope != NULL);
    g_return_if_fail(node != NULL);

    scope->path_node = node;
}  /* rasta_scope_set_path_node() */


/*
 * void rasta_symbol_put(RastaContext *ctxt,
 *                       const gchar *sym_name,
 *                       const gchar *sym_value)
 *
 * Inserts a symbol into the current scope's symbol table
 */
void rasta_symbol_put(RastaContext *ctxt,
                      const gchar *sym_name,
                      const gchar *sym_value)
{
    RastaScope *scope;

    g_return_if_fail(ctxt != NULL);
    g_return_if_fail(ctxt->scopes != NULL);
    g_return_if_fail(sym_name != NULL);
    g_return_if_fail(g_utf8_strlen(sym_name, -1) > 0);

    scope = RASTA_SCOPE(ctxt->scopes->data);

    g_hash_table_insert(scope->symbols,
                        g_strdup(sym_name),
                        g_strdup(sym_value));
}  /* rasta_symbol_put() */


/*
 * gchar *rasta_symbol_lookup(RastaContext *ctxt,
 *                            const gchar *sym_name)
 *
 * Looks up a symbol in the symbol tables
 */
gchar *rasta_symbol_lookup(RastaContext *ctxt,
                           const gchar *sym_name)
{
    gchar *sym_val;
    GList *elem;
    RastaScope *scope;
    
    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(sym_name != NULL, NULL);
    g_return_val_if_fail(g_utf8_strlen(sym_name, -1) > 0, NULL);

    elem = ctxt->scopes;
    sym_val = NULL;
    while (elem != NULL)
    {
        scope = RASTA_SCOPE(elem->data);

        sym_val = g_hash_table_lookup(scope->symbols, sym_name);
        if (sym_val != NULL)
            break;
        sym_val = NULL;
        elem = g_list_next(elem);
    }

    return(g_strdup(sym_val));
}  /* rasta_symbol_lookup() */


/* static void rasta_scope_build_symbol_xml(gpointer key,
 *                                          gpointer value,
 *                                          gpointer user_data)
 *
 * Builds the XML for one scope's symbols.
 */
static void rasta_scope_build_symbol_xml(gpointer key,
                                         gpointer value,
                                         gpointer user_data)
{
    xmlNodePtr node, cur;

    g_return_if_fail(key != NULL);
    g_return_if_fail(user_data != NULL);

    cur = (xmlNodePtr)user_data;
    node = xmlNewNode(NULL, "SYMBOL");
    if (node == NULL)
        return;

    xmlSetProp(node, "NAME", (gchar *)key);
    xmlSetProp(node, "VALUE", (gchar *)(value ? value : ""));

    xmlAddChild(cur, node);
}  /* rasta_scope_build_symbol_xml() */


/*
 * gint rasta_scope_add_state_xml(RastaContext *ctxt,
 *                                xmlDocPtr doc)
 *
 * Adds XML data for the scopes to doc
 */
gint rasta_scope_add_state_xml(RastaContext *ctxt,
                               xmlDocPtr doc)
{
    xmlNodePtr par, cur;
    RastaScope *scope;
    GList *elem;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->scopes != NULL, -EINVAL);
    g_return_val_if_fail(doc != NULL, -EINVAL);

    par = xmlDocGetRootElement(doc);
    g_return_val_if_fail(par != NULL, -EINVAL);

    elem = ctxt->scopes;
    while(elem->next != NULL)
        elem = g_list_next(elem);
    
    while (elem != NULL)
    {
        scope = (RastaScope *)elem->data;
        g_assert(scope != NULL);

        cur = xmlNewNode(NULL, "SCOPE");
        if (cur == NULL)
            return(-ENOMEM);

        xmlSetProp(cur, "NAME", scope->id);
        xmlAddChild(par, cur);

        g_hash_table_foreach(scope->symbols,
                             rasta_scope_build_symbol_xml,
                             cur);

        par = cur;
        elem = g_list_previous(elem);
    }

    return(0);
}  /* rasta_scope_add_state_xml() */


