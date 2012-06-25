/*
 * radcommon.c
 * 
 * Common functions for rastaadd and rastadel.
 *
 * Copyright (C) 2001 New Internet Computer, Inc., Joel Becker
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
#include <libxml/valid.h>
#include <libxml/xmlerror.h>

#include "rasta.h"
#include "radcommon.h"

/*
 * Defines
 */
#define BACKUP_EXTENSION ".bak"
#define TEMP_EXTENSION ".tmp"


/*
 * Functions
 */


/*
 * gint rad_validate_file(RADContext *ctxt)
 *
 * Validates a toplevel rasta file.  It sets the pointers to the
 * screen nodes and to the toplevel menu node.
 */
gint rad_validate_file(RADContext *ctxt)
{
    xmlNodePtr cur;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->main.doc != NULL, -EINVAL);

    cur = xmlDocGetRootElement(ctxt->main.doc);
    g_assert(cur != NULL);
    ctxt->main.ns = xmlSearchNsByHref(ctxt->main.doc, cur,
                                      RASTA_NAMESPACE);
    g_assert(ctxt->main.ns != NULL);

    cur = cur->children;
    while (cur != NULL)
    {
        if (cur->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcmp(cur->name, "SCREENS") == 0)
            {
                if (cur->ns != ctxt->main.ns)
                    return(-ESRCH);
                else
                    ctxt->main.screens = cur;
            }
            if (xmlStrcmp(cur->name, "PATH") == 0)
            {
                if (cur->ns != ctxt->main.ns)
                    return(-ESRCH);
                else
                    ctxt->main.path = cur;
            }
        }
        cur = cur->next;
    }
    if ((ctxt->main.screens == NULL) || (ctxt->main.path == NULL))
        return(-ESRCH);

    return(0);
}  /* rad_validate_file() */


/*
 * xmlNodePtr rad_find_path(RADContext *ctxt, const gchar *path)
 *
 * Finds the node representing the passed in child path and returns
 * it.  Empty paths are verboten.  A path must at least contain the
 * root element "/".
 *
 * /foo/bar/baz -------------.
 *                           |
 * <PATH>                    |
 *    <MENU NAME="foo">      |
 *        <MENU NAME="bar">  |
 *            <MENU NAME="baz">
 *
 *  And yes, this is ugly O() as well.
 */
xmlNodePtr rad_find_path(RADContext *ctxt, const gchar *path)
{
    gint i;
    xmlNodePtr cur;
    gchar **p_elem;
    gchar *test_id;

    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(path != NULL, NULL);
    g_return_val_if_fail(path[0] == '/', NULL);

    cur = ctxt->main.path;
    if (strcmp(path, "/") == 0)
        return(cur);

    p_elem = g_strsplit(path, "/", 0);
    /* i is 1 to skip the "" before the leading '/' */
    for (i = 1; (p_elem[i] != NULL) && (cur != NULL); i++)
    {
        cur = cur->children;
        while (cur != NULL)
        {
            if ((cur->type == XML_ELEMENT_NODE) &&
                ((xmlStrcmp(cur->name, "MENU") == 0) ||
                 (xmlStrcmp(cur->name, "DIALOG") == 0) ||
                 (xmlStrcmp(cur->name, "HIDDEN") == 0)))
            {
                test_id = xmlGetProp(cur, "NAME");
                if (test_id != NULL)
                {
                    if (xmlStrcmp(test_id, p_elem[i]) == 0)
                    {
                        g_free(test_id);
                        break;
                    }
                    g_free(test_id);
                }
            }

            cur = cur->next;
        }
    }
    g_strfreev(p_elem);

    return(cur);
}  /* rad_find_path() */


/*
 * gint rad_validate_doc(xmlDocPtr doc,
 *                       const gchar *name,
 *                       const gchar *id)
 *
 * Makes sure the internal subset of doc is the proper DTD, then
 * validates.
 *
 * The issue is that RASTA description files are often hand-built.
 * Even when machine built, they may be hand-copied to a different
 * system.  The path to the DTD could then change, so the SystemID
 * declared in the document could point to the wrong place.  Rather than
 * error, which would cause unsuspecting users confusion, we force the
 * internal subset to represent a known-good DTD -- the one installed.
 * Note that we have to force the internal subset.  If we merely do
 * a posteriori validation (xmlValidateDtd()) we don't have a proper
 * internal subset for attribute default values.  This way lies SEGV.
 *
 * Returns 0 for valid documents.
 */
gint rad_validate_doc(xmlDocPtr doc,
                      const gchar *name,
                      const gchar *id)
{
    xmlValidCtxt val = {0, };
    xmlDtdPtr dtd;
    xmlNodePtr cur;

    dtd = xmlGetIntSubset(doc);
    if (dtd && ((xmlStrcmp(dtd->name, name) != 0) ||
                (xmlStrcmp(dtd->SystemID, id) != 0)))
    {
        cur = doc->children;
        while (cur != NULL)
        {
            if (cur->type == XML_DTD_NODE)
                break;
            cur = cur->next;
        }
        if (cur != NULL)
        {
            xmlUnlinkNode(cur);
            xmlFreeNode(cur);
        }
        doc->intSubset = NULL;

        dtd = xmlCreateIntSubset(doc, name, NULL, id);
        if (dtd == NULL)
            return(-ENOMEM);
    }

    val.userData = NULL;
    val.error = NULL;
    val.warning = NULL;
    return(!xmlValidateDocument(&val, doc));
}  /* rad_validate_doc() */


/*
 * xmlDocPtr rad_parse_file(gchar *filename)
 *
 * Parses the given file, validates it for RASTA use, and then
 * returns the document pointer
 */
xmlDocPtr rad_parse_file(gchar *filename)
{
    xmlDocPtr doc;
    xmlNsPtr ns;
    xmlNodePtr cur;

    g_return_val_if_fail(filename != NULL, NULL);
    g_return_val_if_fail(filename[0] != '\0', NULL);

    doc = xmlParseFile(filename);
    if (doc == NULL)
        return(NULL);

    cur = xmlDocGetRootElement(doc);
    if (cur == NULL)
        goto out_err;

    ns = xmlSearchNsByHref(doc, cur, RASTA_NAMESPACE);
    if (ns == NULL)
        goto out_err;

    if ((xmlStrcmp(cur->name, "RASTA") != 0) ||
        (xmlStrcmp(cur->name, "RASTAMODIFY") != 0))
        return(doc);

out_err:
    xmlFreeDoc(doc);
    return(NULL);
}  /* rad_parse_file() */

/*
 * int rad_get_lock(gchar *filename)
 *
 * Acquires a lock based on a hash of the filename.  Nope, this isn't
 * really NFS safe, but usually rastaadd is run on system files, aka
 * local files.
 */
int rad_get_lock(gchar *filename)
{
    gint semid, rc;
    key_t key;
    struct sembuf sops[2] = 
    {
        {0, 0, IPC_NOWAIT},
        {0, 1, SEM_UNDO}
    };

    g_return_val_if_fail(filename != NULL, -EINVAL);
    g_return_val_if_fail(filename[0] != '\0', -EINVAL);

    key = (key_t)g_str_hash(filename);
    if (key == 0)
        key = 1;

    semid = semget(key, 1, 0600 | IPC_CREAT | IPC_EXCL);
    if (semid == -1)
        return(-errno);

    /* lock exists */

    do
    {
        rc = semop(semid, sops, 2);
        if ((rc == -1) && (errno != EINTR))
        {
            /* delete lock */
            semctl(semid, 0, IPC_RMID, 0);
            return(-errno);
        }
    }
    while (rc < 0);

    /* lock is held */

    return(semid);
}  /* rad_get_lock() */


/*
 * void rad_drop_lock(gint semid)
 *
 * Drops the lock pointed to by semid.
 */
void rad_drop_lock(gint semid)
{
    gint rc;
    struct sembuf sops[1] =
    {
        {0, -1, IPC_NOWAIT}
    };

    g_return_if_fail(semid != -1);

    do
    {
        rc = semop(semid, sops, 1);
        if (rc == -1)
        {
            if (errno == EIDRM)
                return;
            else if (errno != EINTR)
                break;
        }
    }
    while (rc < 0);

    /* lock released */

    rc = semctl(semid, 0, IPC_RMID, 0);
    
    /* lock deleted */
}  /* rad_drop_lock() */


/*
 * void rad_clean_context(RADContext *ctxt)
 *
 * Cleans up a context.
 */
void rad_clean_context(RADContext *ctxt)
{
    if (ctxt->main_filename != NULL)
        g_free(ctxt->main_filename);
    if (ctxt->delta_filename != NULL)
        g_free(ctxt->delta_filename);

    /*
     * Currently, the documents have intermingling pointers, so a 
     * Free is not possible without SIGSEGV.
     */
#if 0
    if (ctxt->delta.doc != NULL)
        xmlFreeDoc(ctxt->delta.doc);
    if (ctxt->main.doc != NULL)
        xmlFreeDoc(ctxt->main.doc);
#endif
}  /* rad_clean_context() */


/*
 * gint rad_save_file(RADContext *ctxt)
 *
 * Saves the XML back to disk.
 */
gint rad_save_file(RADContext *ctxt)
{
    FILE *outf;
    gint rc;
    gchar *tmp_name, *bak_name;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);

    rc = -ENOMEM;
    tmp_name = g_strconcat(ctxt->main_filename, TEMP_EXTENSION, NULL);
    bak_name = g_strconcat(ctxt->main_filename, BACKUP_EXTENSION, NULL);
    if ((tmp_name == NULL) || (bak_name == NULL))
        goto out;

    outf = fopen(ctxt->main_filename, "w");
    if (outf == NULL)
    {
        rc = -errno;
        goto out;
    }

    rc = xmlDocDump(outf, ctxt->main.doc);
    fclose(outf);

out:
    g_free(tmp_name);
    g_free(bak_name);

    return(rc == -1);
}
