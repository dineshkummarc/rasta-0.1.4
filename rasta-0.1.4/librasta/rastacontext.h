/*
 * rastacontext.h
 *
 * Private header file for the RastaContext object.
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


#ifndef _RASTA_CONTEXT_H
#define _RASTA_CONTEXT_H


/*
 * Typedefs
 */
typedef         enum    _RastaContextState      RastaContextState;



/*
 * Enums
 */
enum _RastaContextState
{
    RASTA_CONTEXT_UNINITIALIZED = 0,
    RASTA_CONTEXT_INITIALIZED   = 1,
    RASTA_CONTEXT_SCREEN        = 2,
    RASTA_CONTEXT_INITCOMMAND   = 3
};



/*
 * Structures
 */
struct _RastaContext
{
    xmlDocPtr doc;                 /* Document structure */
    xmlNsPtr ns;                   /* Document namespace */
    xmlNodePtr screens;            /* Parent of screens */
    xmlNodePtr path_root;          /* Parent of the path */
    xmlParserCtxtPtr parser;       /* Parser structure */
    GList *scopes;                 /* Scope stack */
    GHashTable *screen_cache;      /* Cache of loaded screens */
    gchar *filename;               /* Name of the XML data file */
    gchar *fastpath;               /* Fastpath id */
    RastaContextState state;       /* State flags */
};

#endif /* _RASTA_CONTEXT_H */

