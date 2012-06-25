/*
 * radcommon.h
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


#ifndef __RADCOMMON_H

/*
 * Typedefs
 */
typedef struct _RADDocInfo RADDocInfo;
typedef struct _RADContext RADContext;



/*
 * Structures
 */

struct _RADDocInfo
{
    xmlDocPtr doc;
    xmlNsPtr ns;
    xmlNodePtr screens;
    xmlNodePtr path;
};

struct _RADContext
{
    gchar *main_filename;       /* Description file */
    gchar *delta_filename;      /* Additions to the description */
    RADDocInfo main;            /* Description document */
    RADDocInfo delta;           /* Addition document */
    gboolean force;             /* Force overwrite - unused */
    gboolean verbose;           /* Spit out verbose things */
    gboolean backup;            /* Make a backup of main_filename */
};


/*
 * Prototypes
 */
key_t rad_get_lock(gchar *filename);
void rad_drop_lock(gint semid);
xmlDocPtr rad_parse_file(gchar *filename);
gint rad_validate_doc(xmlDocPtr doc,
                      const gchar *name,
                      const gchar *id);
gint rad_validate_file(RADContext *ctxt);
void rad_clean_context(RADContext *ctxt);
xmlNodePtr rad_find_path(RADContext *ctxt, const gchar *path);
gint rad_save_file(RADContext *ctxt);

#endif  /* __RADCOMMON_H */
