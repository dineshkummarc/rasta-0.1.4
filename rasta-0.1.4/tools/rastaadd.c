/*
 * rastaadd.c
 * 
 * A tool to add subtrees to rasta description files.
 *
 * Copyright (C) 2001 Oracle Corporation, Joel Becker
 * <joel.becker@oracle.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have recieved a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "rasta.h"
#include "radcommon.h"


/*
 * Prototypes
 */
static void print_usage(gint rc);
static gint validate_addition(RADContext *ctxt);
static gint load_options(RADContext *ctxt, gint argc, gchar *argv[]);
static gint add_screens(RADContext *ctxt);
static gint add_paths(RADContext *ctxt);
static gint process_path(RADContext *ctxt, xmlNodePtr new_path,
                         xmlNodePtr parent_path);
static gint process_screens(RADContext *ctxt, xmlNodePtr new_screens,
                            xmlNodePtr screen_parent);



/*
 * Functions
 */


/*
 * static gint validate_addition(RADContext *ctxt)
 *
 * Validates a rasta file to be added to the toplevel tree.  It
 * sets the pointers to the screen nodes and to the PATHINSERT node.
 */
static gint validate_addition(RADContext *ctxt)
{
    xmlNodePtr cur;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->delta.doc != NULL, -EINVAL);

    cur = xmlDocGetRootElement(ctxt->delta.doc);
    g_assert(cur != NULL);
    ctxt->delta.ns = xmlSearchNsByHref(ctxt->delta.doc, cur,
                                       RASTA_NAMESPACE);
    g_assert(ctxt->delta.ns != NULL);

    cur = cur->children;
    while (cur != NULL)
    {
        if (cur->type == XML_ELEMENT_NODE)
        {
            if ((xmlStrcmp(cur->name, "SCREENINSERT") == 0) &&
                (ctxt->delta.screens == NULL))
            {
                if (cur->ns != ctxt->delta.ns)
                    return(-ESRCH);
                else
                    ctxt->delta.screens = cur;
            }
            if ((xmlStrcmp(cur->name, "PATHINSERT") == 0) &&
                (ctxt->delta.path == NULL))
            {
                if (cur->ns != ctxt->delta.ns)
                    return(-ESRCH);
                else if (ctxt->delta.path == NULL)
                    ctxt->delta.path = cur;
            }
        }
        cur = cur->next;
    }
    if ((ctxt->delta.screens == NULL) && (ctxt->delta.path == NULL))
        return(-ESRCH);

    return(0);
}  /* validate_addition() */


/*
 * static gint process_path(RADContext *ctxt, xmlNodePtr new_path,
 *                          xmlNodePtr parent_path)
 *
 * Adds new_path as a child of parent_path.
 */
static gint process_path(RADContext *ctxt, xmlNodePtr new_path,
                         xmlNodePtr parent_path)
{
    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(new_path != NULL, -EINVAL);
    g_return_val_if_fail(parent_path != NULL, -EINVAL);

    xmlAddChildList(parent_path, new_path);

    return(0);
}  /* process_path() */


/*
 * static gint process_screens(RADContext *ctxt, xmlNodePtr new_screens,
 *                             xmlNodePtr screen_parent)
 *
 * Processes a screen.  If there is an old screen and force is not
 * enabled, it errors.
 */
static gint process_screens(RADContext *ctxt, xmlNodePtr new_screens,
                            xmlNodePtr screen_parent)
{
    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(new_screens != NULL, -EINVAL);
    g_return_val_if_fail(screen_parent != NULL, -EINVAL);

    xmlAddChildList(screen_parent, new_screens);

    return(0);
}  /* process_screen() */


/*
 * static gint add_screens(RADContext *ctxt)
 *
 * Adds the SCREEN elements to the main context.  This is ugly and
 * O(n^2).  Someone got a better way?
 */
static gint add_screens(RADContext *ctxt)
{
    gint rc;

    rc = process_screens(ctxt, ctxt->delta.screens->children,
                         ctxt->main.screens);
    return(rc);
}  /* add_screens() */


/*
 * static gint add_paths(RADContext *ctxt)
 *
 * Adds the PATHINSERT elements to the main context.  This is ugly as
 * well.
 */
static gint add_paths(RADContext *ctxt)
{
    gint rc;
    gchar *path;
    xmlNodePtr cur, target;

    cur = ctxt->delta.path;
    while (cur != NULL)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            (xmlStrcmp(cur->name, "PATHINSERT") == 0))
        {
            path = xmlGetProp(cur, "PATH");
            target = rad_find_path(ctxt, path);
            xmlFree(path);
            if (target == NULL)
                return(-ESRCH);
            rc = process_path(ctxt, cur->children, target);
            if (rc != 0)
                return(rc);
        }
        cur = cur->next;
    }

    return(0);
}  /* add_paths() */


/*
 * static void print_usage(gint rc)
 * 
 * Prints a usage message and exits
 */
static void print_usage(gint rc)
{
    FILE *output;

    output = rc ? stderr : stdout;

    fprintf(output,
            "Usage: rastaadd [--file <system_file>] [--force] [--verbose] <addition_file>\n");
    exit(rc);
}  /* print_usage() */


/*
 * static gint load_options(RADContext *ctxt, gint argc, gchar *argv[])
 *
 * Loads all the options.
 */
static gint load_options(RADContext *ctxt, gint argc, gchar *argv[])
{
    gint i;

    if (argc < 2)
        return(-EINVAL);

    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
            break;
        if (strcmp(argv[i], "--") == 0)
        {
            i++;
            break;
        }
        if ((strcmp(argv[i], "-h") == 0) ||
            (strcmp(argv[i], "-?") == 0) ||
            (strcmp(argv[i], "--help") == 0))
            return(1);
        else if (strcmp(argv[i], "--file") == 0)
        {
            i++;
            if (argv[i][0] == '-')
                return(-EINVAL);
            ctxt->main_filename = g_strdup(argv[i]);
        }
        else if (strcmp(argv[i], "--backup") == 0)
            ctxt->backup = TRUE;
        else if ((strcmp(argv[i], "-v") == 0) ||
                 (strcmp(argv[i], "--verbose") == 0))
            ctxt->verbose = TRUE;
        else
            return(-EINVAL);
    }

    if (ctxt->main_filename == NULL)
        ctxt->main_filename = g_build_filename(_RASTA_DIR,
                                               RASTA_SYSTEM_FILE,
                                               NULL);
    if (i < argc)
        ctxt->delta_filename = g_strdup(argv[i]);
    else
        return(-EINVAL);

    return(0);
}  /* load_options() */



/*
 * Main program
 */
gint main(gint argc, gchar *argv[])
{
    gint semid, rc;
    RADContext *ctxt;

    ctxt = g_new0(RADContext, 1);
    if (ctxt == NULL)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        return(-ENOMEM);
    }

    rc = load_options(ctxt, argc, argv);
    if (rc < 0)
        print_usage(rc);
    else if (rc > 0)
        print_usage(0);

    semid = rad_get_lock(ctxt->main_filename);
    if (semid < 0)
    {
        fprintf(stderr, "rastaadd: Unable to acquire lock\n");
        return(semid);
    }

    ctxt->main.doc = rad_parse_file(ctxt->main_filename);
    if (ctxt->main.doc == NULL)
    {
        fprintf(stderr, "rastaadd: Unable to parse file \"%s\"\n",
                ctxt->main_filename);
        rc = -ENOENT;
        goto out;
    }

    rc = rad_validate_doc(ctxt->main.doc, "RASTA",
                          "file:" _RASTA_DATA_DIR
                          G_DIR_SEPARATOR_S RASTA_DTD);
    if (rc != 0)
    {
        fprintf(stderr, "rastaadd: File \"%s\" is invalid\n",
                ctxt->main_filename);
        goto out;
    }

    rc = rad_validate_file(ctxt);
    if (rc != 0)
    {
        fprintf(stderr, "rastaadd: File \"%s\" is invalid\n",
                ctxt->main_filename);
        goto out;
    }

    ctxt->delta.doc = rad_parse_file(ctxt->delta_filename);
    if (ctxt->delta.doc == NULL)
    {
        fprintf(stderr, "rastaadd: Unable to parse file \"%s\"\n",
                ctxt->delta_filename);
        rc = -ENOENT;
        goto out;
    }

    rc = rad_validate_doc(ctxt->delta.doc, "RASTAMODIFY",
                          "file:" _RASTA_DATA_DIR
                          G_DIR_SEPARATOR_S RASTAMODIFY_DTD);
    if (rc != 0)
    {
        fprintf(stderr, "rastaadd: File \"%s\" is invalid\n",
                ctxt->main_filename);
        goto out;
    }

    rc = validate_addition(ctxt);
    if (rc != 0)
    {
        fprintf(stderr, "rastaadd: File \"%s\" is invalid\n",
                ctxt->delta_filename);
        goto out;
    }

    rc = add_screens(ctxt);
    if (rc != 0)
    {
        fprintf(stderr, "rastaadd: Unable to add new screens\n");
        goto out;
    }
    
    rc = add_paths(ctxt);
    if (rc != 0)
    {
        fprintf(stderr, "rastaadd: Unable to add new paths\n");
        goto out;
    }

    rc = rad_validate_doc(ctxt->main.doc, "RASTA",
                          "file:" _RASTA_DATA_DIR
                          G_DIR_SEPARATOR_S RASTA_DTD);
    if (rc != 0)
    {
        fprintf(stderr,
                "rastaadd: Additions create an invalid document\n");
        goto out;
    }

    rc = rad_save_file(ctxt);
    if (rc != 0)
        fprintf(stderr, "rastaadd: Unable to save new copy of \"%s\"\n",
                ctxt->main_filename);

out:
    rad_drop_lock(semid);
    rad_clean_context(ctxt);

    return(rc);
}  /* main() */

