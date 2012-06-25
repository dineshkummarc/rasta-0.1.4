/*
 * rastaexec.c
 *
 * Code file for the Rasta subprocess exec functions.
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

#include <string.h>
#include <glib.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>

#include "rasta.h"
#include "rastaexec.h"



/*
 * Prototypes
 */
static void child_pgrp(gpointer user_data);



/*
 * Functions
 */


/*
 * static void child_pgrp(gpointer user_data)
 *
 * Child setup handler for g_spawn_aync_with_pipes.  Currently it
 * calls setpgrp(2) to isolate the child.
 */
static void child_pgrp(gpointer user_data)
{
    setpgrp();
}  /* child_pgrp() */


/*
 * gint rasta_exec_command_v(pid_t *pid,
 *                           gint *infd,
 *                           gint *outfd,
 *                           gint *errfd,
 *                           RastaExecFDProtocol in_prot,
 *                           RastaExecFDProtocol out_prot,
 *                           RastaExecFDProtocol err_prot,
 *                           gchar * const args[])
 *
 * Executes a command, returning the process id via the passed pid
 * prarameter.  The file descriptors are handled based on the passed
 * protocol specifications:
 * RASTA_EXEC_FD_INHERIT causes the child to inherid the fd.
 * RASTA_EXEC_FD_NULL causes the child's fd to be redirected to/from
 *     /dev/null.
 * RASTA_EXEC_FD_PIPE causes the child's fd to be attached to a pipe,
 *     and the relevant descriptor is passed back via the fd pointer.  
 *     The fd pointer must be non-NULL.
 *
 * Returns 0 if successful, otherwise returns -ERROR.
 * Errors are in the GSPAWN domain.
 */
gint rasta_exec_command_v(pid_t *pid,
                          gint *infd,
                          gint *outfd,
                          gint *errfd,
                          RastaExecFDProtocol in_prot,
                          RastaExecFDProtocol out_prot,
                          RastaExecFDProtocol err_prot,
                          gchar * args[])
{
    pid_t n_pid;  /* Never pass an empty pid */
    gint rc = 0;
    gboolean res;
    GError *err = NULL;
    GSpawnFlags flags;

    flags = G_SPAWN_DO_NOT_REAP_CHILD;
    switch (in_prot)
    {
        case RASTA_EXEC_FD_PIPE:
            if (infd == NULL)
                return(-G_SPAWN_ERROR_INVAL);
            break;

        case RASTA_EXEC_FD_INHERIT:
            infd = NULL; /* Sanity */
            flags |= G_SPAWN_CHILD_INHERITS_STDIN;
            break;

        case RASTA_EXEC_FD_NULL:
            infd = NULL; /* Sanity */
            break;

        default:
            g_assert_not_reached();
            break;
    }

    switch (out_prot)
    {
        case RASTA_EXEC_FD_PIPE:
            if (outfd == NULL)
                return(-G_SPAWN_ERROR_INVAL);
            break;

        case RASTA_EXEC_FD_INHERIT:
            outfd = NULL; /* Sanity */
            break;

        case RASTA_EXEC_FD_NULL:
            outfd = NULL; /* Sanity */
            flags |= G_SPAWN_STDOUT_TO_DEV_NULL;
            break;

        default:
            g_assert_not_reached();
            break;
    }

    switch (err_prot)
    {
        case RASTA_EXEC_FD_PIPE:
            if (errfd == NULL)
                return(-G_SPAWN_ERROR_INVAL);
            break;

        case RASTA_EXEC_FD_INHERIT:
            errfd = NULL; /* Sanity */
            break;

        case RASTA_EXEC_FD_NULL:
            errfd = NULL; /* Sanity */
            flags |= G_SPAWN_STDERR_TO_DEV_NULL;
            break;

        default:
            g_assert_not_reached();
            break;
    }

    res = g_spawn_async_with_pipes(NULL,
                                   args,
                                   NULL,
                                   flags,
                                   (GSpawnChildSetupFunc)child_pgrp,
                                   NULL,
                                   &n_pid,
                                   infd,
                                   outfd,
                                   errfd,
                                   &err);
    if (res == FALSE)
    {
        rc = err->code ? -(err->code) : -1;
        g_clear_error(&err);
    }
    else if (pid != NULL)
        *pid = n_pid;

    return(rc);
}  /* rasta_exec_command_v() */


/*
 * gint rasta_exec_command_l(pid_t *pid,
 *                           gint *infd,
 *                           gint *outfd,
 *                           gint *errfd,
 *                           RastaExecFDProtocol in_prot,
 *                           RastaExecFDProtocol out_prot,
 *                           RastaExecFDProtocol err_prot,
 *                           const gchar *command,
 *                           ...)
 *
 * Executes a command, returning the process id and the 
 * stdout and stderr file descriptors via the passed parameters.
 * This implementation grabs the va_list and formats it for
 * rasta_exec_command_v()
 */
gint rasta_exec_command_l(pid_t *pid,
                          gint *infd,
                          gint *outfd,
                          gint *errfd,
                          RastaExecFDProtocol in_prot,
                          RastaExecFDProtocol out_prot,
                          RastaExecFDProtocol err_prot,
                          const gchar *command,
                          ...)
{
    guint count, i, rc;
    va_list args;
    gchar *s;
    gchar **new_args;
    
    g_return_val_if_fail(command != NULL, -EINVAL);

    count = 0;
    va_start(args, command);
    s = va_arg(args, gchar*);
    while (s)
    {
        count++;
        s = va_arg(args, gchar*);
    }
    va_end(args);
    
    new_args = g_new(gchar*, count + 2);
    if (new_args == NULL) return(-ENOMEM);
    
    new_args[0] = g_strdup(command);
    va_start(args, command);
    for (i = 1; i < count + 2; i++)
        new_args[i] = va_arg(args, gchar*);
    va_end(args);

    rc = rasta_exec_command_v(pid,
                              infd, outfd, errfd,
                              in_prot, out_prot, err_prot,
                              new_args);
    g_strfreev(new_args);
    
    return(rc);
}

typedef gchar * (*EscapeFunc) (const gchar *str);

gchar *rasta_escape_none(const gchar *str)
{
    return g_strdup(str);
}

gchar *rasta_escape_single(const gchar *str)
{
    GString *buf;
    gint i, len;
    gchar *ret;

    len = strlen(str);
 
    buf = g_string_sized_new(len);

    for (i = 0; i < len; i++)
    {
        if (str[i] == '\'')
	    g_string_append(buf, "'\\''");
	else
	    g_string_append_c(buf, str[i]);
    }

    ret = g_strdup(buf->str);
    g_string_free(buf, TRUE);

    return ret;
}

gchar *rasta_escape_double(const gchar *str)
{
    GString *buf;
    gint i, len;
    gchar *ret;

    len = strlen(str);
 
    buf = g_string_sized_new(len);

    for (i = 0; i < len; i++)
    {
        switch (str[i])
        {
            case '"':
	        g_string_append(buf, "\\\"");
                break;

            case '*':
	        g_string_append(buf, "\\*");
                break;

            case '[':
	        g_string_append(buf, "\\[");
                break;

            case ']':
	        g_string_append(buf, "\\]");
                break;

            case '\\':
	        g_string_append(buf, "\\\\");
                break;

            default:
	        g_string_append_c(buf, str[i]);
                break;
        }
    }

    ret = g_strdup(buf->str);
    g_string_free(buf, TRUE);

    return ret;
}

gchar *rasta_exec_symbol_subst(RastaContext *ctxt,
                               const gchar *str,
                               RastaEscapeStyleType escape)
{
    GString *buf, *sym;
    gint i, len;
    gchar *ret;
    gboolean in_sym = FALSE;
    EscapeFunc escape_func = NULL;;

    switch (escape)
    {
        case RASTA_ESCAPE_STYLE_NONE:
            escape_func = rasta_escape_none;
	    break;

        case RASTA_ESCAPE_STYLE_SINGLE:
            escape_func = rasta_escape_single;
	    break;

        case RASTA_ESCAPE_STYLE_DOUBLE:
            escape_func = rasta_escape_double;
	    break;

	default:
	    g_assert_not_reached();
	    break;
    }

    len = strlen(str);
 
    buf = g_string_sized_new(len);
    sym = g_string_new("");

    for (i = 0; i < len; i++)
    {
        if (in_sym)
        {
            if (str[i] == '#')
            {
		gchar *val, *escaped_val;

                val = rasta_symbol_lookup(ctxt, sym->str);
                if (val != NULL)
                {
		    escaped_val = escape_func(val);
                    g_string_append(buf, escaped_val);
		    
		    g_free(escaped_val);
                    g_free(val);
		}

                g_string_assign(sym, "");
	        in_sym = FALSE;
            }
            else if ((str[i] == ' ') ||
                     (str[i] == '\t') ||
                     (str[i] == '\n'))
            {
                /* not a symbol, just a '#' */
                g_string_append_c(buf, '#');
                g_string_append(buf, sym->str);
                g_string_append_c(buf, str[i]);
                g_string_assign(sym, "");
                in_sym = FALSE;
            }
            else
                g_string_append_c(sym, str[i]);
        }
        else
        {
            if (str[i] == '#')
	        in_sym = TRUE;
	    else
	        g_string_append_c(buf, str[i]);
        }
    }

    ret = g_strdup(buf->str);
    g_string_free(buf, TRUE);
    g_string_free(sym, TRUE);

    return ret;
}
