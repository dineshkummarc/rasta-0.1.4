/*
 * rastaexec.h
 *
 * Functions for executing helper programs.
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


#ifndef _RASTA_EXEC_H
#define _RASTA_EXEC_H

#include <sys/types.h>
#include <unistd.h>

/*
 * Prototypes
 */

/* Escape functions */
gchar *rasta_escape_none(const gchar *str);
gchar *rasta_escape_single(const gchar *str);
gchar *rasta_escape_double(const gchar *str);

/* Symbol replacement functions */
gchar *rasta_exec_symbol_subst(RastaContext *ctxt,
                               const gchar *str,
			       RastaEscapeStyleType escape);

#endif /* _RASTA_EXEC_H */
