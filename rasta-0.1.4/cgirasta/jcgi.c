/*
 * jcgi.c
 * 
 * JCGI interaction routines
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include <lockfile.h>

#include "jiterator.h"
#include "jconfig.h"
#include "jcgi.h"



/*
 * Defines
 */
#define HTML_UNENCODED_CHARS \
( \
        G_CSET_a_2_z \
        "_-0123456789" \
        G_CSET_A_2_Z \
        "/:." \
)



/*
 * Typedefs
 */
typedef struct _SymbolBuilder SymbolBuilder;



/*
 * Structures
 */
struct _JCGIState
{
    gchar *method;
    gchar *error;
    GList *parameter_names;
    GHashTable *parameters;
    GList *cookie_names;
    GHashTable *cookies;
};



/*
 * Forward declarations
 */
static void j_cgi_param_insert(JCGIState *state,
                               gpointer name,
                               gpointer value);
static void j_cgi_cookie_insert(JCGIState *state,
                                gpointer name,
                                gpointer value);
static gchar *j_cgi_handle_method(JCGIState *state);
static void j_cgi_encode_resolve(gchar *str);
static void j_cgi_parse(JCGIState *state, gchar *cgi_data);
static void j_cgi_parse_cookies(JCGIState *state);
static void j_cgi_free_parameter(gpointer key,
                                 gpointer value,
                                 gpointer thrway);
static void j_cgi_free_parameter_name(gpointer elem_data,
                                      gpointer thrway);



/*
 * Functions
 */


/*
 * static void j_cgi_param_insert(JCGIState *state,
 *                                gpointer name,
 *                                gpointer value)
 *
 * Inserts a parameter in the JCGIState structure.  
 * "name" and "value" MUST be dynamically allocated memory that can
 * be freed (nothing else must depend on their existance).
 */
static void j_cgi_param_insert(JCGIState *state,
                               gpointer name,
                               gpointer value)
{
    gpointer orig_name, orig_value;
    GList *elem;

    if (g_hash_table_lookup_extended(state->parameters,
                                     name,
                                     &orig_name,
                                     &orig_value))
    {
        g_free(name);
        name = orig_name;
        elem = (GList *)orig_value;
    }
    else
        elem = NULL;

    elem = g_list_prepend(elem, value);
    g_hash_table_insert(state->parameters,
                        name,
                        elem);
}  /* j_cgi_param_insert() */


/*
 * static void j_cgi_cookie_insert(JCGIState *state,
 *                                 gpointer name,
 *                                 gpointer value)
 *
 * Inserts a cookie in the JCGIState structure.  This wrapper
 * is mostly convenience for duplicate checking (last one wins).
 * "name" and "value" MUST be dynamically allocated memory that can
 * be freed (nothing else must depend on their existance).
 */
static void j_cgi_cookie_insert(JCGIState *state,
                                gpointer name,
                                gpointer value)
{
    gpointer orig_name, orig_value;

    if (g_hash_table_lookup_extended(state->cookies,
                                     name,
                                     &orig_name,
                                     &orig_value))
    {
        g_hash_table_remove(state->cookies, name);
        g_free(orig_name);
        g_free(orig_value);
    }
    g_hash_table_insert(state->cookies,
                        name,
                        value);
}  /* j_cgi_cookie_insert() */


/*
 * static gchar *j_cgi_handle_method(JCGIState *state)
 *
 * Decides what CGI method we are using and sets up the cgi_data
 * string with the appropriate data.
 */
static gchar *j_cgi_handle_method(JCGIState *state)
{
    gchar *cgi_data;
    gint rc, c, content_length;

    if (strcmp(state->method, "GET") == 0)
    {
        cgi_data = getenv("QUERY_STRING");
        if (cgi_data == NULL)
            return(NULL);

        cgi_data = g_strdup(cgi_data);
        if (cgi_data == NULL)
        {
            state->error = "Unable to allocate memory for CGI data";
            return(NULL);
        }
    }
    else if (strcmp(state->method, "POST") == 0)
    {
        content_length = atoi(getenv("CONTENT_LENGTH"));
        if (content_length == 0)
            return(NULL);

        cgi_data = g_new(char, content_length + 1);
        if (cgi_data == NULL)
        {
            state->error = "Unable to allocate memory for CGI data";
            return(NULL);
        }
            
        c = 0;
        while (c < content_length)
        {
            rc = read(STDIN_FILENO, cgi_data + c, content_length - c);
            if (rc == 0)
                break;
            else if (rc == -1)
            {
                if (errno == EINTR)
                    continue;
                else
                {
                    state->error = "Error reading CGI data";
                    return(NULL);
                }
            }
            else
               c += rc; 
        }
        cgi_data[c] = '\0';
    }
    else if (strcmp(state->method, "STDIN") == 0)
    {
        /* Haha, forgot about that! */
        cgi_data = g_strdup("");
    }
    else
    {
        state->error = "Invalid CGI method";
        return(NULL);
    }

    return(cgi_data);
}  /* j_cgi_handle_method() */


/*
 * static void j_cgi_encode_resolve(gchar *str)
 *
 * Resolve cgi %xx encodings.  Boa already does this for
 * 'GET' method parsing.  Bad boa.  BAAAAD.
 */
static void j_cgi_encode_resolve(gchar *str)
{
    gint len, cur_pos;
    gchar new_chr;
    gchar *buf, *endptr;

    g_return_if_fail(str != NULL);

    cur_pos = 0;
    len = strlen(str);
    buf = g_new(char, 3);
    for (cur_pos = 0; cur_pos < (len - 2); cur_pos++)
    {
        if (str[cur_pos] == '%')
        {
            memmove(buf, &str[cur_pos + 1], 2);
            buf[2] = '\0';
            new_chr = (gchar)strtol(buf, &endptr, 16);
            if (buf != endptr)
            {
                str[cur_pos] = new_chr;
                len -= 2;
                memmove(&str[cur_pos + 1], &str[cur_pos + 3],
                        len - cur_pos);
            }
        }
    }
}  /* j_cgi_encode_resolve() */


/*
 * gchar *j_cgi_encode(const gchar *url)
 *
 * Encode a string.
 */
gchar *j_cgi_encode(const gchar *url)
{
    gint len, i;
    gchar *result;
    GString *str;

    g_return_val_if_fail(url != NULL, NULL);

    len = strlen(url);
    str = g_string_sized_new(len);
    if (str == NULL)
        return(NULL);

    for (i = 0; i < len; i++)
    {
        if (strchr(HTML_UNENCODED_CHARS, url[i]) == NULL)
        {
            g_string_append_printf(str, "%%%02X", url[i]);
        }
        else
            g_string_append_c(str, url[i]);
    }

    result = str->str;
    g_string_free(str, FALSE);

    return(result);
}  /* j_cgi_encode() */


/*
 * static void j_cgi_parse(JCGIState *state, gchar *cgi_data)
 *
 * Parses CGI information and adds it to the JCGIState structure
 */
static void j_cgi_parse(JCGIState *state, gchar *cgi_data)
{
    gint c, name_length, value_length;
    gchar *name, *name_ptr, *value, *value_ptr;
    gboolean done;

    enum
    {
        JCGI_STATE_NAME,
        JCGI_STATE_VALUE
    } parse_state;
    
    parse_state = JCGI_STATE_NAME;
    done = FALSE;
    c = 0;
    name_ptr = cgi_data;
    name_length = 0;
    value_ptr = NULL;
    value_length = 0;
    while (done == FALSE)
    {
        if (parse_state == JCGI_STATE_NAME)
        {
            switch (cgi_data[c])
            { 
                case '=':
                    parse_state = JCGI_STATE_VALUE;
                    value_ptr = name_ptr + name_length + 1;
                    break;

                case '\0':
                    done = TRUE;
                    /* FALL THROUGH */

                case '&':
                    name = g_new(char, name_length + 1);
                    if (name == NULL)
                    {
                        state->error =
                            "Unable to allocate memory for CGI data";
                        return;
                    }
                    memcpy(name, name_ptr, name_length);
                    name[name_length] = '\0';
                    j_cgi_encode_resolve(name);
                    if (name[0] != '\0')
                    {
                        gchar *value;
                        value = g_strdup("");
                        state->parameter_names =
                            g_list_append(state->parameter_names,
                                           g_strdup(name));
                        j_cgi_param_insert(state, name, value);
                    }
                    else
                        g_free(name);
                    name_ptr += name_length + 1;
                    name_length = 0;
                    break;

                case '+':
                    cgi_data[c] = ' ';
                    /* FALL THROUGH */

                default:
                    name_length++;
                    break;
            }    
        }
        else if (parse_state == JCGI_STATE_VALUE)
        {
            switch (cgi_data[c])
            {
                case '\0':
                    done = TRUE;
                    /* FALL THROUGH */

                case '&':
                    parse_state = JCGI_STATE_NAME;

                    name = g_new(char, name_length + 1);
                    if (name == NULL)
                    {
                        state->error =
                            "Unable to allocate memory for CGI data";
                        return;
                    }
                    memcpy(name, name_ptr, name_length);
                    name[name_length] = '\0';
                    j_cgi_encode_resolve(name);

                    value = g_new(char, value_length + 1);
                    if (value == NULL)
                    {
                        state->error =
                            "Unable to allocate memory for CGI data";
                        return;
                    }
                    memcpy(value, value_ptr, value_length);
                    value[value_length] = '\0';
                    j_cgi_encode_resolve(value);
                    
                    if (name[0] != '\0')
                    {
                        state->parameter_names =
                            g_list_append(state->parameter_names,
                                           g_strdup(name));
                        j_cgi_param_insert(state, name, value);
                    }
                    else
                    {
                        g_free(name);
                        g_free(value);
                    }
                    name_ptr += name_length + value_length + 2;
                    name_length = 0;
                    value_ptr = NULL;
                    value_length = 0;

                    break;

                case '+':
                    cgi_data[c] = ' ';
                    /* FALL THROUGH */

                default:
                    value_length++;
                    break;
            }
        }
        c++;
    }
}  /* j_cgi_parse() */


/*
 * static void j_cgi_parse_cookies(JCGIState *state)
 *
 * Parses the HTTP_COOKIE value.
 */
static void j_cgi_parse_cookies(JCGIState *state)
{
    gint i, len;
    gchar *cookie_string, *key, *value;
    gchar **cookie_entries;
    gchar **cookie;

    cookie_string = g_strdup(getenv("HTTP_COOKIE"));
    if (cookie_string == NULL)
        return;
    if (strlen(cookie_string) == 0)
    {
        g_free(cookie_string);
        return;
    }

    g_strdelimit(cookie_string, "\t", ' ');
    cookie_entries = g_strsplit(cookie_string, " ", 0);
    g_free(cookie_string);
    if (cookie_entries != NULL)
    {
        for (i = 0; cookie_entries[i] != NULL; i++)
        {
            len = strlen(cookie_entries[i]);
            if (cookie_entries[i][len - 1] == ';')
                cookie_entries[i][len - 1] = '\0';
            cookie = g_strsplit(cookie_entries[i], "=", 2);
            if (cookie == NULL)
                continue;

            if ((cookie[0] == NULL) ||
                (strlen(cookie[0]) == 0))
            {
                g_strfreev(cookie);
                continue;
            }
            key = g_strdup(cookie[0]);
            if (key == NULL)
            {
                g_strfreev(cookie);
                break;
            }
            value = g_strdup(cookie[1] ? cookie[1] : "");
            g_strfreev(cookie);
            if (value == NULL)
            {
                g_free(key);
                break;
            }
            j_cgi_encode_resolve(key);
            j_cgi_encode_resolve(value);
            state->cookie_names = g_list_append(state->cookie_names,
                                                 key);
            j_cgi_cookie_insert(state, key, value);
        }
    }
}  /* j_cgi_parse_cookies() */


/*
 * JCGIState *j_cgi_init()
 *
 * Parses CGI information and creates a JCGIState variable containing
 * the information
 */
JCGIState *j_cgi_init()
{
    JCGIState *state;
    gchar *cgi_data;

    state = g_new(JCGIState, 1);
    if (state == NULL)
        return(NULL);

    state->error = NULL;
    state->parameter_names = NULL;    
    state->cookie_names = NULL;
    state->parameters = g_hash_table_new(g_str_hash, g_str_equal);
    if (state->parameters == NULL)
    {
        state->error = "Unable to allocate memory for parameter hash";
        return(state);
    }
    state->cookies = g_hash_table_new(g_str_hash, g_str_equal);
    if (state->cookies == NULL)
    {
        state->error = "Unable to allocate memory for cookie hash";
        return(state);
    }

    state->method = getenv("REQUEST_METHOD");

    if (state->method == NULL)
    {
        state->method = "STDIN";
        /* do stdin stuff */
    }

    state->method = g_strdup(state->method);
    if (state->method == NULL)
    {
        state->error = "Unable to allocate memory for REQUEST_METHOD";
        return(state);
    }

    j_cgi_parse_cookies(state);

    cgi_data = j_cgi_handle_method(state);
    if (cgi_data == NULL)
        return(state);

    j_cgi_parse(state, cgi_data);

    return(state);
}  /* j_cgi_init() */


/*
 * gchar *j_cgi_get_method(JCGIState *state)
 *
 * Returns the CGI method that this program was called with.
 */
gchar *j_cgi_get_method(JCGIState *state)
{
    g_return_val_if_fail(state != NULL, NULL);

    return(g_strdup(state->method));
}  /* j_cgi_get_method() */


/*
 * JIterator *j_cgi_get_parameter_names(JCGIState *state)
 *
 * Returns a JIterator of all the parameter names passed to this
 * CGI program.
 */
JIterator *j_cgi_get_parameter_names(JCGIState *state)
{
    g_return_val_if_fail(state != NULL, NULL);

    return(j_iterator_new_from_list(state->parameter_names));
}  /* j_cgi_get_parameter_names() */


/*
 * JIterator *j_cgi_get_parameter_values(JCGIState *state,
 *                                       const gchar *name)
 *
 * Returns a JIterator of all values associated with the passed
 * parameter name, or NULL if the parameter was not specified.
 */
JIterator *j_cgi_get_parameter_values(JCGIState *state,
                                      const gchar *name)
{
    GList *elem, *elem_copy;
    JIterator *iter;

    g_return_val_if_fail(state != NULL, NULL);

    if (name == NULL)
    {
        state->error = "NULL value passed for parameter name";
        return(NULL);
    } 

    elem = g_hash_table_lookup(state->parameters, name);

    elem_copy = g_list_copy(elem);
    if ((elem_copy == NULL) && (elem != NULL))
       state->error = "Unable to allocate memory for parameter values";

    elem_copy = g_list_reverse(elem_copy);
    iter = j_iterator_new_from_list(elem_copy);
    g_list_free(elem_copy);

    return(iter);
}  /* j_cgi_get_parameter_values() */


/*
 * gchar *j_cgi_get_parameter(JCGIState *state, const gchar *name)
 *
 * Returns the last value associated with the passed parameter name, or
 * NULL if the parameter was not specified
 */
gchar *j_cgi_get_parameter(JCGIState *state, const gchar *name)
{
    GList *elem;
    gchar *t;

    g_return_val_if_fail(state != NULL, NULL);

    if (name == NULL)
    {
        state->error = "NULL value passed for parameter name";
        return(NULL);
    } 

    elem = g_hash_table_lookup(state->parameters, name);
    if (elem == NULL)
        return(NULL);

    t = g_strdup((gchar *)elem->data);

    if ((t == NULL) && (elem->data != NULL))
       state->error = "Unable to allocate memory for parameter value";
    return(t);
}  /* j_cgi_get_parameter() */


/*
 * JIterator *j_cgi_get_cookie_names(JCGIState *state)
 *
 * Returns a JIterator of all the cookie names passed to this
 * JCGI program.
 */
JIterator *j_cgi_get_cookie_names(JCGIState *state)
{
    g_return_val_if_fail(state != NULL, NULL);

    return(j_iterator_new_from_list(state->cookie_names));
}  /* j_cgi_get_cookie_names() */


/*
 * gchar *j_cgi_get_cookie(JCGIState *state, const gchar *name)
 *
 * Returns the value associated with the passed cookie name, or
 * NULL if the cookie was not specified
 */
gchar *j_cgi_get_cookie(JCGIState *state, const gchar *name)
{
    gchar *s, *t;

    g_return_val_if_fail(state != NULL, NULL);

    if (name == NULL)
    {
        state->error = "NULL value passed for cookie name";
        return(NULL);
    } 

    s = (gchar *)g_hash_table_lookup(state->cookies, name);
    t = g_strdup(s);

    if ((t == NULL) && (s != NULL))
       state->error = "Unable to allocate memory for cookie value";
    return(t);
}  /* j_cgi_get_cookie() */


/*
 * static void j_cgi_free_parameter(gpointer key,
 *                                  gpointer value,
 *                                  gpointer thrway)
 *
 * Frees a parameter's key and value
 */
static void j_cgi_free_parameter(gpointer key,
                                 gpointer value,
                                 gpointer thrway)
{
    GList *elem;

    g_free(key);

    for (elem = (GList *)value; elem != NULL; elem = g_list_next(elem))
        g_free(elem->data);

    g_list_free((GList *)value);
}  /* j_cgi_free_parameter() */


/*
 * static void j_cgi_free_parameter_name(gpointer elem_data,
 *                                       gpointer thrway)
 *
 * Frees a parameter name
 */
static void j_cgi_free_parameter_name(gpointer elem_data,
                                      gpointer thrway)
{
    g_free(elem_data);
}  /* j_cgi_free_parameter_name() */


/*
 * void j_cgi_free(JCGIState *state)
 *
 * Destroys a JCGIState variable
 */
void j_cgi_free(JCGIState *state)
{
    GList *elem;

    g_free(state->method);
    if (state->error != NULL)
        g_free(state->error);
    elem = (GList *)state->parameter_names;
    g_list_foreach(elem, j_cgi_free_parameter_name, NULL);
    g_list_free(elem);
    g_hash_table_foreach(state->parameters, j_cgi_free_parameter, NULL);
    g_hash_table_destroy(state->parameters);
    g_free(state);
}  /* j_cgi_free() */


/*
 * const gchar *j_cgi_get_error(JCGIState *state)
 *
 * Returns the error string
 */
const gchar *j_cgi_get_error(JCGIState *state)
{
    return(state->error);
}  /* j_cgi_get_error() */
