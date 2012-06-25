/*
 * jcgi.h
 * 
 * Header file for cgi functions.
 *
 * Copyright (C) 2002 Joel Becker <jlbec@evilplan.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef __JCGI_H
#define __JCGI_H



/*
 * Typedefs
 */
typedef struct _JCGIState JCGIState;



/*
 * Functions
 */
JCGIState *j_cgi_init();
const gchar *j_cgi_get_error(JCGIState *state);
gchar *j_cgi_get_method(JCGIState *state);
JIterator *j_cgi_get_parameter_names(JCGIState *state);
JIterator *j_cgi_get_parameter_values(JCGIState *state,
                                      const gchar *name);
gchar *j_cgi_get_parameter(JCGIState *state, const gchar *name);
JIterator *j_cgi_get_cookie_names(JCGIState *state);
gchar *j_cgi_get_cookie(JCGIState *state, const gchar *name);
void j_cgi_free(JCGIState *state);
gchar *j_cgi_encode(const gchar *url);

#endif /* __JCGI_H */
