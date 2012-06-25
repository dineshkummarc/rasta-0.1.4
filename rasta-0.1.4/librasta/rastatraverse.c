/*
 * rastatraverse.c
 *
 * Functions for traversing the path tree
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
#include "rastatraverse.h"
#include "rastascope.h"


/*
 * Functions
 */


/*
 * gboolean rasta_validate_root(RastaContext *ctxt)
 *
 * Finds the root of the data and verifies that the root is a node
 * of type RASTA, with the toplevel MENU or DIALOG beneath it.  It also
 * initializes the ctxt->screens and ctxt->path_root pointers.
 */
gboolean rasta_validate_root(RastaContext *ctxt)
{
    xmlNodePtr cur;
    xmlNsPtr namespace;

    g_return_val_if_fail(ctxt != NULL, FALSE);

    cur = xmlDocGetRootElement(ctxt->doc);

    if (cur == NULL)
        return(FALSE);

    namespace = xmlSearchNsByHref(ctxt->doc, cur, RASTA_NAMESPACE);
    if (namespace == NULL)
        return(FALSE);
    ctxt->ns = namespace;

    if (xmlStrcmp(cur->name, "RASTA") != 0)
        return(FALSE);

    cur = cur->children;
    while (cur != NULL)
    {
        if (cur->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcmp(cur->name, "SCREENS") == 0)
            {
                if (cur->ns != namespace)
                    return(FALSE);
                else
                    ctxt->screens = cur;
            }
            else if (xmlStrcmp(cur->name, "PATH") == 0)
            {
                if (cur->ns != namespace)
                    return(FALSE);
                else
                    ctxt->path_root = cur;
            }
        }
        cur = cur->next;
    }
    if ((ctxt->screens == NULL) || (ctxt->path_root == NULL))
        return(FALSE);

    cur = ctxt->path_root->children;
    while (cur != NULL)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            ((xmlStrcmp(cur->name, "MENU") == 0) ||
             (xmlStrcmp(cur->name, "DIALOG") == 0)))
        {
            if (cur->ns != namespace)
                return(FALSE);
            else
                break;
        }
        cur = cur->next;
    }
    if (cur == NULL)
        return(FALSE);

    ctxt->path_root = cur;
    return(TRUE);
}  /* rasta_validate_root() */


/*
 * gboolean rasta_find_fastpath(RastaContext *ctxt)
 *
 * Runs through the tree from the top node to a certain fastpath;
 * If the fastpath cannot be found, returns false;
 */
gboolean rasta_find_fastpath(RastaContext *ctxt)
{
    xmlNodePtr node, screen;
    xmlChar *cur_id;
    xmlChar *fastpath;
    gboolean prune;
    GHashTable *seen;

    g_return_val_if_fail(ctxt != NULL, FALSE);
    g_return_val_if_fail(ctxt->doc != NULL, FALSE);
    g_return_val_if_fail(ctxt->path_root != NULL, FALSE);

    /* Leave at the top if there is no fastpath */
    if (ctxt->fastpath == NULL)
        return(TRUE);

    seen = g_hash_table_new_full(g_str_hash,
                                 g_str_equal,
                                 g_free,
                                 g_free);

    if (seen == NULL)
        return(FALSE);

    node = ctxt->path_root->children;
    while (node != NULL)
    {
        prune = FALSE;
        if ((node->type == XML_ELEMENT_NODE) &&
            ((xmlStrcmp(node->name, "MENU") == 0) ||
             (xmlStrcmp(node->name, "DIALOG") == 0)))
        {
            cur_id = xmlGetProp(node, "NAME");
            if (cur_id != NULL)
            {
                fastpath = (gchar *)g_hash_table_lookup(seen, cur_id);
                if (fastpath == NULL)
                {
                    screen = rasta_find_screen(ctxt, cur_id);
                    if (screen != NULL)
                        fastpath = xmlGetProp(screen, "FASTPATH");

                    if (fastpath == NULL)
                        fastpath = g_strdup("true");

                    g_hash_table_insert(seen,
                                        g_strdup(cur_id),
                                        fastpath);
                }
                if ((fastpath != NULL) && 
                    (xmlStrcasecmp(fastpath, "false") == 0))
                    prune = TRUE;
            }
            if (prune == FALSE)
            {
                if (cur_id != NULL)
                {
                    if (xmlStrcmp(cur_id, ctxt->fastpath) == 0)
                    {
                        ctxt->path_root = node;
                        g_free(cur_id);
                        g_hash_table_destroy(seen);
                        return(TRUE);
                    }
                    g_free(cur_id);
                }
                if (node->children != NULL)
                {
                    node = node->children;
                    continue;
                }
            }
        }
        if (node->next != NULL)
            node = node->next;
        else
        {
            while (node != ctxt->path_root)
            {
                if (node->parent != NULL)
                    node = node->parent;
                if ((node != ctxt->path_root) && (node->next != NULL))
                {
                    node = node->next;
                    break;
                }
                if (node->parent == NULL)
                {
                    node = NULL;
                    break;
                }
                if (node == ctxt->path_root)
                {
                    node = NULL;
                    break;
                }
            }
            if (node == ctxt->path_root)
                node = NULL;
        }
    }

    g_hash_table_destroy(seen);
    return(FALSE);
}  /* rasta_find_fastpath() */


/*
 * xmlNodePtr rasta_find_screen(RastaContext *ctxt,
 *                              const gchar *screen_id)
 *
 * Looks in the SCREENS subtree for the given id and returns a
 * pointer to the node.
 */
xmlNodePtr rasta_find_screen(RastaContext *ctxt,
                             const gchar *screen_id)
{
    gint rc;
    xmlNodePtr cur;
    xmlChar *test_id;
    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(ctxt->state != RASTA_CONTEXT_UNINITIALIZED,
                         NULL);
    g_return_val_if_fail(ctxt->screens != NULL, NULL);
    g_return_val_if_fail(screen_id != NULL, NULL);

    cur = ctxt->screens->children;
    while (cur != NULL)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            (cur->ns == ctxt->ns) &&
            ((xmlStrcmp(cur->name, "MENUSCREEN") == 0) ||
             (xmlStrcmp(cur->name, "DIALOGSCREEN") == 0) ||
             (xmlStrcmp(cur->name, "HIDDENSCREEN") == 0) ||
             (xmlStrcmp(cur->name, "ACTIONSCREEN") == 0)))
        {
            test_id = xmlGetProp(cur, "ID");
            if (test_id != NULL)
            {
                rc = xmlStrcmp(test_id, screen_id);
                g_free(test_id);
                if (rc == 0)
                    break;
            }
        }
        cur = cur->next;
    }

    return(cur);
}  /* rasta_find_screen() */


/*
 * xmlNodePtr rasta_traverse_forward(RastaContext *ctxt,
 *                                   const gchar *next_id)
 *
 * Looks in the PATH subtree for the next node with the given id.
 * If next_id is NULL, then this is a dialog screen's next.
 */
xmlNodePtr rasta_traverse_forward(RastaContext *ctxt,
                                  const gchar *next_id)
{
    gint rc = 1;
    xmlNodePtr cur;
    xmlChar *test_id, *test_key, *test_val;
    RastaScope *scope;
    
    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(ctxt->state != RASTA_CONTEXT_UNINITIALIZED,
                         NULL);
    g_return_val_if_fail(ctxt->scopes != NULL, NULL);

    scope = rasta_scope_get_current(ctxt);
    cur = rasta_scope_get_path_node(scope);

#ifdef DEBUG
    g_print("Cur name = \"%s\"\n", cur->name);
#endif

    /* I don't like this if, but I do like the glib-CRITICALs */
    if (next_id == NULL)
        g_return_val_if_fail((xmlStrcmp(cur->name, "DIALOG") == 0) ||
                             (xmlStrcmp(cur->name, "HIDDEN") == 0),
                             NULL);
    else 
        g_return_val_if_fail(xmlStrcmp(cur->name, "MENU") == 0, NULL);

    cur = cur->children;
    while (cur != NULL)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            (cur->ns == ctxt->ns))
        {
            if (next_id != NULL)  /* currenlty MENU */
            {
                if ((xmlStrcmp(cur->name, "MENU") == 0) ||
                    (xmlStrcmp(cur->name, "DIALOG") == 0) ||
                    (xmlStrcmp(cur->name, "HIDDEN") == 0) ||
                    (xmlStrcmp(cur->name, "ACTION") == 0))
                {
                    test_id = xmlGetProp(cur, "NAME");
                    if (test_id != NULL)
                    {
                        rc = xmlStrcmp(test_id, next_id);
                        g_free(test_id);
                        if (rc == 0)
                            break;
                    }
                }
            }
            else 
            {
                /*
                 * currently DIALOG || HIDDEN ||
                 *           MULTIPATH || DEFAULTPATH
                 */
                if ((xmlStrcmp(cur->name, "DIALOG") == 0) ||
                    (xmlStrcmp(cur->name, "HIDDEN") == 0) ||
                    (xmlStrcmp(cur->name, "ACTION") == 0))
                    break;
                else if (xmlStrcmp(cur->name, "MULTIPATH") == 0)
                {
                    test_key = xmlGetProp(cur, "SYMBOL");
                    if (test_key == NULL)
                        goto next_elem;
                    test_val = xmlGetProp(cur, "VALUE");
                    if (test_val == NULL)
                    {
                        g_free(test_key);
                        goto next_elem;
                    }

                    test_id = rasta_symbol_lookup(ctxt, test_key);
                    if (test_id != NULL)
                    {
                        rc = xmlStrcmp(test_val, test_id);
                        g_free(test_id);
                    }
                    g_free(test_key);
                    g_free(test_val);
                    if (rc == 0)
                    {
                        cur = cur->children;
                        continue;
                    }
                }
                else if (xmlStrcmp(cur->name, "DEFAULTPATH") == 0)
                {
                    cur = cur->children;
                    continue;
                }
            }
        }
        ;
next_elem:
        cur = cur->next;
    }  /* while (cur != NULL); */

#ifdef DEBUG
    g_print("New cur name = \"%s\"\n", cur->name);
#endif  /* DEBUG */
    return(cur);
}  /* rasta_traverse_forward() */


/*
 * gchar *rasta_query_help(RastaContext *ctxt, xmlNodePtr node)
 *
 * Returns the HELP subtag of the given node (if there is one)
 */
gchar *rasta_query_help(RastaContext *ctxt, xmlNodePtr node)
{
    gchar *help;

    g_return_val_if_fail(node != NULL, NULL);

    node = node->children;

    if (node == NULL)
        return(NULL);

    help = NULL;
    while (node != NULL)
    {
        if ((node->type == XML_ELEMENT_NODE) &&
            (xmlStrcmp(node->name, "HELP") == 0))
        {
            help = xmlNodeListGetString(ctxt->doc, node->children, 1);
            break;
        }
        node = node->next;
    }

    return(help);
}  /* rasta_query_help() */
