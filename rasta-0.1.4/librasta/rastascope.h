/*
 * rastascope.h
 *
 * Header file for scope code
 *
 * Copyright (C) 2001 Oracle Corporation, Joel Becker
 * <joel.becker@oracle.com> and Manish Singh (manish.singh@oracle.com>
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


#ifndef __RASTA_SCOPE_H
#define __RASTA_SCOPE_H

/*
 * Defines
 */
#define         RASTA_SCOPE(x)          ((RastaScope *)(x))



/*
 * Typedefs
 */
typedef         struct _RastaScope      RastaScope;



/*
 * Prototypes
 */
void rasta_scope_push(RastaContext *ctxt, xmlNodePtr node);
gboolean rasta_scope_is_top(RastaContext *ctxt);
void rasta_scope_pop(RastaContext *ctxt);
RastaScope *rasta_scope_get_current(RastaContext *ctxt);
gchar *rasta_scope_get_id(RastaScope *scope);
RastaScreen *rasta_scope_get_screen(RastaScope *scope);
void rasta_scope_set_screen(RastaScope *scope,
                            RastaScreen *screen);
xmlNodePtr rasta_scope_get_path_node(RastaScope *scope);
void rasta_scope_set_path_node(RastaScope *scope,
                               xmlNodePtr node);
gint rasta_scope_add_state_xml(RastaContext *ctxt,
                               xmlDocPtr doc);

#endif  /* __RASTA_SCOPE_H */
