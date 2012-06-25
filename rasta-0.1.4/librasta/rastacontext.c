/*
 * rastacontext.c
 *
 * Functions for manipulating the RastaContext structure
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
#include <errno.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/xmlerror.h>

#include "rasta.h"
#include "rastacontext.h"
#include "rastatraverse.h"
#include "rastascope.h"
#include "rastascreen.h"



/*
 * Prototypes
 */
static gint validate_dtd(xmlDocPtr doc,
                         const xmlChar *name,
                         const xmlChar *id);
static gboolean rasta_context_init_screens(RastaContext *ctxt);
static xmlNodePtr rasta_context_validate_state(RastaContext *ctxt,
                                               xmlDocPtr state_doc);
static gint rasta_context_prepare_state(RastaContext *ctxt,
                                        xmlDocPtr state_doc);
static gint rasta_context_travel_state(RastaContext *ctxt,
                                       xmlDocPtr state_doc,
                                       xmlNodePtr cur);
static void rasta_context_revert_state(RastaContext *ctxt);
static gint rasta_context_build_state_xml(RastaContext *ctxt,
                                          xmlDocPtr *doc);


/*
 * Functions
 */


/*
 * static gint validate_dtd(xmlDocPtr doc,
 *                          const xmlChar *name,
 *                          const xmlChar *id)
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
static gint validate_dtd(xmlDocPtr doc,
                         const xmlChar *name,
                         const xmlChar *id)
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
}  /* validate_dtd() */


/*
 * RastaContext *rasta_context_init(const gchar *filename,
 *                                  const gchar *filename)
 *
 * Initializes the Rasta context from a file and a fastpath
 */
RastaContext *rasta_context_init(const gchar *filename,
                                 const gchar *fastpath)
{
    RastaContext *new_ctxt;

    g_return_val_if_fail(filename != NULL, NULL);

    new_ctxt = g_new(RastaContext, 1);
    if (new_ctxt == NULL)
        return(NULL);

    new_ctxt->state = RASTA_CONTEXT_UNINITIALIZED;
    new_ctxt->filename = g_strdup(filename);
    new_ctxt->fastpath = g_strdup(fastpath);
    new_ctxt->parser = NULL;
    new_ctxt->scopes = NULL;
    new_ctxt->screens = NULL;
    new_ctxt->screen_cache = NULL;
    
    new_ctxt->doc = xmlParseFile(new_ctxt->filename);
    if (new_ctxt->doc == NULL)
    {
        rasta_context_destroy(new_ctxt);
        return(NULL);
    }

    if (validate_dtd(new_ctxt->doc, "RASTA",
                     "file://" _RASTA_DATA_DIR
                     G_DIR_SEPARATOR_S RASTA_DTD) != 0)
    {
        rasta_context_destroy(new_ctxt);
        return(NULL);
    }

    new_ctxt->state = RASTA_CONTEXT_INITIALIZED;

    if (rasta_context_init_screens(new_ctxt) == FALSE)
    {
        rasta_context_destroy(new_ctxt);
        return(NULL);
    }

    return(new_ctxt);
}  /* rasta_context_init() */


/*
 * RastaContext *rasta_context_init_push(const gchar *filename,
 *                                       const gchar *fastpath)
 *
 * Creates a "push" context for initialization; this allows the
 * using application to provide data in chunks
 */
RastaContext *rasta_context_init_push(const gchar *filename,
                                      const gchar *fastpath)
{
    RastaContext *new_ctxt;

    g_return_val_if_fail(filename != NULL, NULL);

    new_ctxt = g_new(RastaContext, 1);
    if (new_ctxt == NULL)
        return(NULL);

    new_ctxt->state = RASTA_CONTEXT_UNINITIALIZED;
    new_ctxt->filename = g_strdup(filename);
    new_ctxt->fastpath = g_strdup(fastpath);
    new_ctxt->doc = NULL;
    new_ctxt->scopes = NULL;
    new_ctxt->screens = NULL;
    new_ctxt->screen_cache = NULL;
    
    new_ctxt->parser = xmlCreatePushParserCtxt(NULL, NULL,
                                               NULL, 0,
                                               new_ctxt->filename);
    if (new_ctxt->parser == NULL)
    {
        rasta_context_destroy(new_ctxt);
        return(NULL);
    }

    return(new_ctxt);
}  /* rasta_context_init_push() */


/*
 *
 * gint rasta_context_parse_chunk(RastaContext *ctxt,
 *                                gpointer data_chunk,
 *                                gint size)
 *
 * Parses a chunk of data; a data size of 0 indicates end of file
 */
gint rasta_context_parse_chunk(RastaContext *ctxt,
                               gpointer data_chunk,
                               gint size)
{
    gint rc;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->parser != NULL, -EINVAL);
    g_return_val_if_fail(size > -1, -EINVAL);
    g_return_val_if_fail((size == 0) || (data_chunk != NULL), -EINVAL);

    rc = xmlParseChunk(ctxt->parser, data_chunk, size,
                       (size == 0) ? 1 : 0);

    if (rc != 0)
    {
        if (ctxt->parser->myDoc != NULL)
            xmlFreeDoc(ctxt->parser->myDoc);
        xmlFreeParserCtxt(ctxt->parser);
        ctxt->parser = NULL;
        rasta_context_destroy(ctxt);
    }
    else if (size == 0)
    {
        ctxt->doc = ctxt->parser->myDoc;
        xmlFreeParserCtxt(ctxt->parser);
        ctxt->parser = NULL;

        if (validate_dtd(ctxt->doc, "RASTA",
                         "file://" _RASTA_DATA_DIR
                         G_DIR_SEPARATOR_S RASTA_DTD) != 0)
        {
            rasta_context_destroy(ctxt);
            rc = -EINVAL;
        }
        else
        {
            ctxt->state = RASTA_CONTEXT_INITIALIZED;
            if (rasta_context_init_screens(ctxt) == FALSE)
            {
                rasta_context_destroy(ctxt);
                rc = -EINVAL;
            }
        }
    }

    return(rc);
}  /* rasta_context_parse_chunk() */


/*
 * void rasta_context_destroy(RastaContext *ctxt)
 *
 * Frees the RastaContext structure and all of its elements
 */
void rasta_context_destroy(RastaContext *ctxt)
{
    if (ctxt == NULL)
        return;

    if (ctxt->parser != NULL)
    {
        if (ctxt->parser->myDoc != NULL)
            ctxt->doc = ctxt->parser->myDoc;
        xmlFreeParserCtxt(ctxt->parser);
    }
    if (ctxt->doc != NULL)
        xmlFreeDoc(ctxt->doc);
    g_free(ctxt->filename);
    g_free(ctxt->fastpath);
    /* FIXME: Clear scopes */
}  /* rasta_context_destroy() */


/*
 * static gboolean rasta_context_init_screens(RastaContext *ctxt)
 *
 * Initialzes the first scope and screen (hopefully)
 */
static gboolean rasta_context_init_screens(RastaContext *ctxt)
{
    gboolean rc;
    RastaScreen *screen;
    RastaScope *scope;

    g_return_val_if_fail(ctxt != NULL, FALSE);

    if (rasta_validate_root(ctxt) == FALSE)
        return(FALSE);

    rc = rasta_find_fastpath(ctxt);

    rasta_scope_push(ctxt, ctxt->path_root);

    if (rc == FALSE)
    {
        screen = g_new0(RastaScreen, 1);
        screen->type = RASTA_SCREEN_NONE;
        scope = rasta_scope_get_current(ctxt);
        rasta_scope_set_screen(scope, screen);
    }
    else
        rasta_screen_prepare(ctxt);

    return(TRUE);
}  /* rasta_context_init_screens() */


/*
 * RastaContext *rasta_context_load_state(RastaContext *ctxt,
 *                                        const gchar *filename)
 *
 * Moves a RastaContext to a given state.  The RastaContext must
 * be in its initial state with no fastpath.
 */
gint rasta_context_load_state(RastaContext *ctxt,
                              const gchar *filename)
{
    gint rc;
    xmlDocPtr state_doc;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->parser == NULL, -EINVAL);
    g_return_val_if_fail(ctxt->fastpath == NULL, -EINVAL);
    g_return_val_if_fail(rasta_context_is_initial_screen(ctxt),
                         -EINVAL);
    g_return_val_if_fail(filename != NULL, -EINVAL);
    g_return_val_if_fail(filename[0] != '\0', -EINVAL);

    state_doc = xmlParseFile(filename);
    if (state_doc == NULL)
        return(-EBADF);

    if (validate_dtd(state_doc, "RASTASTATE",
                     "file://" _RASTA_DATA_DIR
                     G_DIR_SEPARATOR_S RASTASTATE_DTD) == 0)
        rc = rasta_context_prepare_state(ctxt, state_doc);
    else
        rc = -EBADF;
    xmlFreeDoc(state_doc);

    return(rc);
}  /* rasta_context_load_state() */


/*
 * RastaContext *rasta_context_load_state_push(RastaContext *ctxt,
 *                                             const gchar *filename)
 *
 * Creates a "push" context for state loading; this allows the
 * using application to provide data in chunks.
 */
gint rasta_context_load_state_push(RastaContext *ctxt,
                                   const gchar *filename)
{
    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->parser == NULL, -EINVAL);
    g_return_val_if_fail(ctxt->fastpath == NULL, -EINVAL);
    g_return_val_if_fail(rasta_context_is_initial_screen(ctxt),
                         -EINVAL);
    g_return_val_if_fail(filename != NULL, -EINVAL);
    g_return_val_if_fail(filename[0] != '\0', -EINVAL);

    ctxt->parser = xmlCreatePushParserCtxt(NULL, NULL,
                                           NULL, 0,
                                           filename);
    if (ctxt->parser == NULL)
        return(-EBADF);

    return(0);
}  /* rasta_context_load_state_push() */


/*
 *
 * gint rasta_context_parse_state_chunk(RastaContext *ctxt,
 *                                      gpointer data_chunk,
 *                                      gint size)
 *
 * Parses a chunk of data; a data size of 0 indicates end of file.
 */
gint rasta_context_parse_state_chunk(RastaContext *ctxt,
                                     gpointer data_chunk,
                                     gint size)
{
    gint rc;
    xmlDocPtr state_doc;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->parser != NULL, -EINVAL);
    g_return_val_if_fail(rasta_context_is_initial_screen(ctxt),
                         -EINVAL);
    g_return_val_if_fail(size > -1, -EINVAL);
    g_return_val_if_fail((size == 0) || (data_chunk != NULL), -EINVAL);

    rc = xmlParseChunk(ctxt->parser, data_chunk, size,
                       (size == 0) ? 1 : 0);

    if (rc != 0)
    {
        if (ctxt->parser->myDoc)
            xmlFreeDoc(ctxt->parser->myDoc);
        xmlFreeParserCtxt(ctxt->parser);
        ctxt->parser = NULL;
    }
    else if (size == 0)
    {
        state_doc = ctxt->parser->myDoc;
        xmlFreeParserCtxt(ctxt->parser);
        ctxt->parser = NULL;

        if (validate_dtd(state_doc, "RASTASTATE",
                         "file://" _RASTA_DATA_DIR
                         G_DIR_SEPARATOR_S RASTASTATE_DTD) == 0)
            rc = rasta_context_prepare_state(ctxt, state_doc);
        else
            rc = -EBADF;
        xmlFreeDoc(state_doc);
    }

    return(rc);
}  /* rasta_context_parse_state_chunk() */


/*
 * static xmlNodePtr rasta_context_validate_state(RastaContext *ctxt,
 *                                                xmlDocPtr state_doc)
 *
 * Validates a state document and returns the root scope.
 */
static xmlNodePtr rasta_context_validate_state(RastaContext *ctxt,
                                               xmlDocPtr state_doc)
{
    xmlNodePtr cur;
    xmlNsPtr ns;
    RastaScope *scope;
    gchar *id, *attr;

    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(state_doc != NULL, NULL);

    cur = xmlDocGetRootElement(state_doc);
    if (cur == NULL)
        return(NULL);

    ns = xmlSearchNsByHref(state_doc, cur, RASTA_NAMESPACE);
    if (ns == NULL)
        return(NULL);

    if (xmlStrcmp(cur->name, "RASTASTATE") != 0)
        return(NULL);

    cur = cur->children;
    while (cur != NULL)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            (xmlStrcmp(cur->name, "SCOPE") == 0) &&
            (cur->ns == ns))
        {
            attr = xmlGetProp(cur, "NAME");
            if (attr != NULL)
            {
                scope = rasta_scope_get_current(ctxt);
                g_assert(scope != NULL);
                id = rasta_scope_get_id(scope);
                if (id != NULL)
                {
                    if (xmlStrcmp(id, attr) == 0)
                    {
                        g_free(id);
                        g_free(attr);
                        break;
                    }
                    g_free(id);
                }
                g_free(attr);
            }
        }
        cur = cur->next;
    }

    return(cur);
}  /* rasta_context_validate_state() */


/*
 * static gint rasta_context_travel_state(RastaContext *ctxt,
 *                                        xmlDocPtr state_doc,
 *                                        xmlNodePtr cur)
 *
 * Walks a state document moving the context to the screens.
 */
static gint rasta_context_travel_state(RastaContext *ctxt,
                                       xmlDocPtr state_doc,
                                       xmlNodePtr cur)
{
    xmlNsPtr ns;
    gchar *id, *name, *val;
    RastaScreen *screen;
    RastaScope *scope;

    ns = cur->ns;
    cur = cur->children;
    while (cur != NULL)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            (cur->ns == ns))
        {
            if (xmlStrcmp(cur->name, "SYMBOL") == 0)
            {
                name = xmlGetProp(cur, "NAME");
                if (name == NULL)
                    return(-EBADF);
                if (name[0] == '\0')
                {
                    g_free(name);
                    return(-EBADF);
                }

                val = xmlGetProp(cur, "VALUE");
                rasta_symbol_put(ctxt, name, val);
                if (val)
                    g_free(val);
                g_free(name);
            }
            else if (xmlStrcmp(cur->name, "SCOPE") == 0)
            {
                name = xmlGetProp(cur, "NAME");
                if (name == NULL)
                    return(-EBADF);
                if (name[0] == '\0')
                {
                    g_free(name);
                    return(-EBADF);
                }

                screen = rasta_context_get_screen(ctxt);
                switch (screen->type)
                {
                    case RASTA_SCREEN_MENU:
                        rasta_menu_screen_next(ctxt, name);
                        break;

                    case RASTA_SCREEN_DIALOG:
                        rasta_dialog_screen_next(ctxt);
                        break;

                    case RASTA_SCREEN_HIDDEN:
                        rasta_hidden_screen_next(ctxt);
                        break;

                    case RASTA_SCREEN_ACTION:
                    default:
                        g_free(name);
                        return(-EBADF);
                        break;
                }

                /* We've jumped the state, no initcommand */
                if (ctxt->state == RASTA_CONTEXT_INITCOMMAND)
                    ctxt->state = RASTA_CONTEXT_SCREEN;

                scope = rasta_scope_get_current(ctxt);
                if (scope == NULL)
                {
                    g_free(name);
                    return(-EBADF);
                }
                id = rasta_scope_get_id(scope);
                if (id == NULL)
                {
                    g_free(name);
                    return(-EBADF);
                }
                
                if (xmlStrcmp(name, id) != 0)
                {
                    g_free(name);
                    g_free(id);
                    return(-EBADF);
                }

                g_free(name);
                g_free(id);

                cur = cur->children;
                continue;
            }
        }
        cur = cur->next;
    }
    return(0);
}  /* rasta_context_travel_state() */


/*
 * static void rasta_context_revert_state(RastaContext *ctxt)
 *
 * Revert to the initial screen.
 */
static void rasta_context_revert_state(RastaContext *ctxt)
{
    while (rasta_context_is_initial_screen(ctxt) == FALSE)
        rasta_screen_previous(ctxt);
}  /* rasta_context_revert_state() */


/*
 * static gint rasta_context_prepare_state(RastaContext *ctxt,
 *                                         xmlDocPtr state_doc)
 *
 * Given a state definition, attempts to move the ctxt to the new
 * state.
 */
static gint rasta_context_prepare_state(RastaContext *ctxt,
                                        xmlDocPtr state_doc)
{
    gint rc;
    xmlNodePtr cur;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(state_doc != NULL, -EINVAL);

    cur = rasta_context_validate_state(ctxt, state_doc);
    if (cur == NULL)
        return(-EBADF);

    rc = rasta_context_travel_state(ctxt, state_doc, cur);
    if (rc != 0)
        rasta_context_revert_state(ctxt);

    return(rc);
}  /* rasta_context_prepare_state() */


/*
 * static gint rasta_context_build_state_xml(RastaContext *ctxt,
 *                                       xmlDocPtr *doc)
 *
 * Builds state XML and returns the XML document in the doc pointer.
 */
static gint rasta_context_build_state_xml(RastaContext *ctxt,
                                          xmlDocPtr *doc)
{
    gint rc;
    xmlDocPtr new_doc;
    xmlDtdPtr dtd;
    xmlNsPtr ns;
    xmlNodePtr cur;

    new_doc = xmlNewDoc("1.0");
    dtd = xmlCreateIntSubset(new_doc, "RASTASTATE", NULL,
                             "file://" _RASTA_DATA_DIR
                             G_DIR_SEPARATOR_S RASTASTATE_DTD);
    cur = xmlNewNode(NULL, "RASTASTATE");
    ns = xmlNewNs(cur, RASTA_NAMESPACE, NULL);
    xmlDocSetRootElement(new_doc, cur);

    rc = rasta_scope_add_state_xml(ctxt, new_doc);

    *doc = new_doc;

    return(rc);
}  /* rasta_context_build_state_xml() */


/*
 * gint rasta_context_save_state(RastaContext *ctxt,
 *                               const gchar *filename)
 *
 * Writes the state as XML data to the given filename.
 */
gint rasta_context_save_state(RastaContext *ctxt,
                              const gchar *filename)
{
    gint rc;
    xmlDocPtr doc;
    FILE *f;

    rc = rasta_context_build_state_xml(ctxt, &doc);
    if (rc != 0)
        return(rc);

    f = fopen(filename, "w");
    if (f == NULL)
        return(-errno);

    rc = xmlDocDump(f, doc);
    fclose(f);

    return(rc);
}  /* rasta_context_save_state() */


/*
 * gint rasta_context_save_state_memory(RastaContext *ctxt,
 *                                      gchar **state_text,
 *                                      gint *state_size)
 *
 * Saves the state to XML and returns the XML in state_text.  The
 * size of the returned XML is returned in state_size.  The caller
 * is responsible for writing the state to a file.
 */
gint rasta_context_save_state_memory(RastaContext *ctxt,
                                     gchar **state_text,
                                     gint *state_size)
{
    gint rc;
    xmlDocPtr doc;

    rc = rasta_context_build_state_xml(ctxt, &doc);

    if (rc == 0)
        xmlDocDumpMemory(doc, (xmlChar **)state_text, state_size);

    return(rc);
}  /* rasta_context_save_state_memory() */


/*
 * RastaScreen *rasta_context_get_screen(RastaContext *ctxt)
 *
 * Returns the current screen object
 */
RastaScreen *rasta_context_get_screen(RastaContext *ctxt)
{
    RastaScope *scope;

    g_return_val_if_fail(ctxt != NULL, NULL);

    scope = rasta_scope_get_current(ctxt);

    return(rasta_scope_get_screen(scope));
}  /* rasta_get_screen() */


/*
 * gboolean rasta_context_is_initial_screen(RastaContext *ctxt)
 *
 * Returns if this is the screen that rasta started with
 */
gboolean rasta_context_is_initial_screen(RastaContext *ctxt)
{
    return(rasta_scope_is_top(ctxt));
}  /* rasta_context_is_initial_screen() */

