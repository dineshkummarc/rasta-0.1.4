/*
 * cgirasta.c
 *
 * CGI program for Rasta navigation
 *
 * Copyright (C) 2002 Joel Becker <jlbec@evilplan.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include "config.h"

#define _XOPEN_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlerror.h>
#include <libxml/valid.h>

#include "rasta.h"

#include "jiterator.h"
#include "jsha.h"
#include "jcgi.h"



/*
 * Defines
 */
#define DEFAULT_TEMPLATE_PATH   (_RASTA_DATA_DIR \
                                 G_DIR_SEPARATOR_S \
                                 "templates")
#define AUTH_TIMEOUT_SECS       300
#define SESSION_TIMEOUT_SECS    300
#define RASTACGIAUTH_DTD        "rastacgiauth.dtd"
#define RASTACGICONF_DTD        "rastacgiconf.dtd"
#define RASTACGIUSER_DTD        "rastacgiuser.dtd"
#define RASTACGISESSION_DTD     "rastacgisession.dtd"
#define VALID_REPLACE_CHARS \
( \
        G_CSET_a_2_z \
        "_-:0123456789" \
        G_CSET_A_2_Z \
        G_CSET_LATINS \
        G_CSET_LATINC \
)



/*
 * Typedefs
 */
typedef struct _RastaCGIConf RastaCGIConf;
typedef struct _RastaCGIUser RastaCGIUser;
typedef struct _RastaCGISession RastaCGISession;
typedef struct _RastaCGIContext RastaCGIContext;



/*
 * Enumerations
 */
typedef enum _RastaCGIAuthLevel
{
    RASTA_CGI_AUTH_NONE,        /* Unauthorized user */
    RASTA_CGI_AUTH_USER,        /* Regular user */
    RASTA_CGI_AUTH_ADMIN        /* Admin user */
} RastaCGIAuthLevel;


typedef enum _RastaCGISessionType
{
    RASTA_CGI_SESSION_DESCRIPTION,      /* Use the description path */
    RASTA_CGI_SESSION_USER              /* Use the management path */
} RastaCGISessionType;



/*
 * Structures
 */
struct _RastaCGIConf
{
    gchar *base_path;
    gchar *config_path;
    gchar *description_path;
    gchar *template_path;
    gchar *user_db_path;
    gchar *auth_db_path;
    gchar *session_db_path;
    RastaCGIAuthLevel user_modify_level;
};


struct _RastaCGIUser
{
    gchar *name;
    gchar *crypt;
    gchar *userid;
    gchar *auth_cookie;
    RastaCGIAuthLevel auth_level;
};


struct _RastaCGISession
{
    gchar *key;
    gchar *state_file;
    RastaCGISessionType type;
};


struct _RastaCGIContext
{
    gchar *cgi_script;
    JCGIState *cgi;
    RastaCGIConf conf;
    RastaCGIUser user;
    RastaCGISession session;
    RastaContext *ctxt;
};



/*
 * Prototypes
 */
static gchar *load_template(RastaCGIContext *ctxt,
                            const gchar *template_name);
static void send_html(RastaCGIContext *ctxt, const gchar *html_text);
static void send_fatal_html(const gchar *err);
static void send_error(RastaCGIContext *ctxt,
                       const gchar *err,
                       gboolean allow_back);
static gint load_cgi_conf(RastaCGIContext *ctxt);
static gint load_auth(RastaCGIContext *ctxt);
static gint save_auth(RastaCGIContext *ctxt);
static gint drop_auth(RastaCGIContext *ctxt);
static gint load_session(RastaCGIContext *ctxt);
static gint load_user(RastaCGIContext *ctxt, const gchar *userid);
static gint prep_db(RastaCGIContext *ctxt);
static void free_session(RastaCGIContext *ctxt);
static void free_user(RastaCGIContext *ctxt);
static void free_cgi_conf(RastaCGIContext *ctxt);
static gchar *get_new_hash(const gchar *user);
static gchar *html_replace(gchar *templ, GHashTable *symbols);
static gint quick_check(RastaCGIContext *ctxt);
static gint check_timestamp(const gchar *timestamp, gint seconds);
static gint handle_session(RastaCGIContext *ctxt);
static gint handle_login(RastaCGIContext *ctxt);
static void send_login(RastaCGIContext *ctxt);
static gint validate_doc(xmlDocPtr doc,
                         const xmlChar *name,
                         const xmlChar *id);



/*
 * Functions
 */


/*
 * static gint handle_login(RastaCGIContext *ctxt)
 *
 * Reads login information and attempts to log the user in
 */
static gint handle_login(RastaCGIContext *ctxt)
{
    gchar salt[3];
    gint rc;
    gchar *user, *pass, *crypted;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->cgi != NULL, -EINVAL);

    drop_auth(ctxt);

    /* Login or Re-Login, the old value is gone */
    if (ctxt->user.userid)
    {
        g_free(ctxt->user.userid); 
        ctxt->user.userid = NULL;
    }


    user = j_cgi_get_parameter(ctxt->cgi, "u");
    if (!user)
        goto out_send;
    if (user[0] == '\0')
    {
        g_free(user);
        goto out_send;
    }

    rc = load_user(ctxt, user);
    if (rc || !ctxt->user.userid || (ctxt->user.userid[0] == '\0'))
    {
        ctxt->user.userid = user;
        goto out_send;
    }
    g_free(user);

    pass = j_cgi_get_parameter(ctxt->cgi, "p");
    if (!pass)
        pass = g_strdup("");
    if (!pass)
        return(-ENOMEM);

    if (ctxt->user.crypt && ctxt->user.crypt[0])
    {
        if (!pass[0])
            goto out_send_pass;
        memcpy(salt, ctxt->user.crypt, 2);
        salt[2] = '\0';
        crypted = crypt(pass, salt);
        rc = strcmp(crypted ? crypted : "", ctxt->user.crypt);
        if (rc)
            goto out_send_pass;
    }
    else if (pass[0])
    {
        /* Stored passwd is blank, but user passed one, auth fails */
        goto out_send_pass;
    }

    save_auth(ctxt);

out:
    return(0);

out_send_pass:
    g_free(pass);

out_send:
    send_login(ctxt);
    goto out;
}  /* handle_login() */


/*
 * static void send_login(RastaCGIContext *ctxt)
 *
 * Sends the login screen.
 */
static void send_login(RastaCGIContext *ctxt)
{
    gchar *login_box, *html_text, *template;
    GHashTable *hash;

    hash = g_hash_table_new_full(g_str_hash,
                                 g_str_equal,
                                 g_free,
                                 g_free);
    if (!hash)
    {
        send_fatal_html("Unable to allocate memory while processing login template");
        return;
    }

    login_box =
        g_strdup_printf("<DIV ALIGN=\"RIGHT\"\n"
                        "<FORM METHOD=\"POST\" ACTION=\"%s\">\n"
                        "<P>\n"
                        "Userid:\n"
                        "<INPUT TYPE=\"text\" NAME=\"u\" VALUE=\"%s\">\n"
                        "</P>\n"
                        "<P>\n"
                        "Password:\n"
                        "<INPUT TYPE=\"password\" NAME=\"p\" VALUE=\"\">\n"
                        "</P>\n"
                        "<INPUT TYPE=\"hidden\" NAME=\"k\" VALUE=\"login\">\n"
                        "<INPUT TYPE=\"submit\" NAME=\"submit\" VALUE=\"Login\">\n"
                        "</FORM>\n"
                        "</DIV>\n",
                        ctxt->cgi_script,
                        ctxt->user.userid ? ctxt->user.userid : "");

    g_hash_table_insert(hash, "login", login_box);

    template = load_template(ctxt, "login.html");
    if (!template)
    {
        send_fatal_html("Unable to load login template");
        goto out_free_hash;
    }

    html_text = html_replace(template, hash);
    g_free(template);

    if (html_text)
        send_html(ctxt, html_text);
    else
        send_fatal_html("Unable to allocate memory while processing error template");

    g_free(html_text);
out_free_hash:
    g_hash_table_destroy(hash);
    return;
}  /* send_login() */


/*
 * static gint handle_session(RastaCGIContext *ctxt)
 *
 * The main session routine.  It is responsible for choosing the proper
 * path:
 *
 * Send the login screen
 * Handle a login and provide the initial session
 * Handle a USER session
 * Handle a DESCRIPTION session
 *
 * Returning nonzero is a FATAL error.  All errors in the context of
 * the session are non-fatal to cgirasta and are handled in HTML.
 */
static gint handle_session(RastaCGIContext *ctxt)
{
    gint rc;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);

    rc = 0;
    if (ctxt->session.key &&
        !strcmp(ctxt->session.key, "login"))
        rc = handle_login(ctxt);
    else if (ctxt->user.userid &&
             (ctxt->user.userid[0] != '\0'))
    {
    }
    else
        send_login(ctxt);

    return(rc);
}  /* handle_session() */


/*
 * static gchar *load_template(RastaCGIContext *ctxt,
 *                             RastaCGITemplateID id)
 *
 * Load a template given the paths in ctxt->conf and the name.
 */
static gchar *load_template(RastaCGIContext *ctxt,
                            const gchar *template_name)
{
    GError *error;
    gchar *template, *template_file;

    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(ctxt->conf.template_path != NULL, NULL);
    g_return_val_if_fail(ctxt->conf.template_path[0] != '\0', NULL);

    template_file = g_build_filename(ctxt->conf.template_path,
                                     template_name,
                                     NULL);
    if (!template_file)
    {
        send_fatal_html("Unable to allocate memory while loading template");
        return(NULL);
    }

    error = NULL;
    if (g_file_get_contents(template_file,
                            &template,
                            NULL, 
                            &error))
        goto success;

    if (error->code == G_FILE_ERROR_NOENT)
    {
        g_free(template_file);
        template_file = g_build_filename(_RASTA_DATA_DIR
                                         G_DIR_SEPARATOR_S
                                         "templates",
                                         template_name,
                                         NULL);
        if (!template_file)
            send_fatal_html("Unable to allocate memory while loading template");
        else
        {
            g_clear_error(&error);
            if (g_file_get_contents(template_file,
                                    &template,
                                    NULL, 
                                    &error))
                goto success;

            template = NULL;
        }
    }
    else
        template = NULL;

    g_clear_error(&error);

success:
    g_free(template_file);

    return(template);
}  /* load_template() */


/*
 * static void send_html(RastaCGIContext *ctxt, const gchar *html_text)
 *
 * Sends HTML text to stdout with proper headers
 */
static void send_html(RastaCGIContext *ctxt, const gchar *html_text)
{
    gint len;
    gchar *cookie;

    if (!ctxt || !html_text)
    {
        send_fatal_html("No HTML!");
        return;
    }

    if (ctxt->user.auth_cookie)
    {
        cookie = g_strdup_printf("Set-Cookie: a=%s\r\n",
                                 ctxt->user.auth_cookie);
    }
    else
        cookie = NULL;

    len = strlen(html_text);
    fprintf(stdout,
            "Content-type: text/html\r\n"
            "Content-length: %d\r\n"
            "%s"
            "\r\n"
            "%s",
            len,
            cookie ? cookie : "",
            html_text);
    g_free(cookie);
}  /* send_html() */


/*
 * static void send_fatal_html(const gchar *err)
 *
 * Sends a static string when there isn't memory for templatizing
 */
static void send_fatal_html(const gchar *err)
{
    fprintf(stdout,
            "Content-type: text/html\r\n"
            "\r\n"
            "<HTML>\n"
            "<HEAD><TITLE>A Fatal Error Occurred</TITLE></HEAD>\n"
            "<BODY>\n"
            "<H1>A Fatal Error Occurred</H1>\n"
            "%s\n"
            "</BODY>\n"
            "</HTML>\n",
            err ? err : "");
}  /* send_fatal_html() */


/*
 * static void send_error(RastaCGIContext *ctxt,
 *                        const gchar *err,
 *                        gboolean allow_back)
 *
 * Sends error text inside the error template.  If allow_back is TRUE,
 * it provides a button to return to the previous state
 */
static void send_error(RastaCGIContext *ctxt,
                       const gchar *err,
                       gboolean allow_back)
{
    gchar *button_str, *html_text, *template;
    GHashTable *hash;

    hash = g_hash_table_new_full(g_str_hash,
                                 g_str_equal,
                                 g_free,
                                 g_free);
    if (!hash)
    {
        send_fatal_html("Unable to allocate memory while processing error template");
        return;
    }

    g_hash_table_insert(hash, g_strdup("error"), g_strdup(err));
    
    if (allow_back)
    {
        button_str =
            g_strdup_printf("<FORM METHOD=\"POST\" ACTION=\"%s\">\n"
                            "<INPUT TYPE=\"hidden\" NAME=\"k\" VALUE=\"%s\">\n"
                            "<INPUT TYPE=\"submit\" NAME=\"submit\" VALUE=\"Back\">\n"
                            "</FORM>\n",
                            ctxt->cgi_script, 
                            ctxt->session.key ? ctxt->session.key : "");
        if (!button_str)
        {
            send_fatal_html("Unable to allocate memory while processing error template");
            goto out_free_hash;
        }
        g_hash_table_insert(hash, g_strdup("buttons"), button_str);
    }

    template = load_template(ctxt, "error.html");
    if (!template)
    {
        send_fatal_html("Unable to load error template");
        goto out_free_hash;
    }

    html_text = html_replace(template, hash);
    g_free(template);

    if (html_text)
        send_html(ctxt, html_text);
    else
        send_fatal_html("Unable to allocate memory while processing error template");

    g_free(html_text);
out_free_hash:
    g_hash_table_destroy(hash);
}  /* send_error() */


/*
 * static gint prep_db(RastaCGIContext *ctxt)
 *
 * Makes sure all the db subdirs exist.  The DB path must exist.
 */
static gint prep_db(RastaCGIContext *ctxt)
{
    gint rc;
    struct stat stat_buf;

    rc = stat(ctxt->conf.base_path, &stat_buf);
    if (rc)
        return(-errno);
    if (!S_ISDIR(stat_buf.st_mode))
        return(-ENOTDIR);

    rc = stat(ctxt->conf.user_db_path, &stat_buf);
    if (rc)
    {
        if (errno != ENOENT)
            return(-errno);
        rc = mkdir(ctxt->conf.user_db_path, 0755);
        if (rc)
            return(-errno);
    }
    else if (!S_ISDIR(stat_buf.st_mode))
        return(-ENOTDIR);

    rc = stat(ctxt->conf.auth_db_path, &stat_buf);
    if (rc)
    {
        if (errno != ENOENT)
            return(-errno);
        rc = mkdir(ctxt->conf.auth_db_path, 0755);
        if (rc)
            return(-errno);
    }
    else if (!S_ISDIR(stat_buf.st_mode))
        return(-ENOTDIR);

    rc = stat(ctxt->conf.session_db_path, &stat_buf);
    if (rc)
    {
        if (errno != ENOENT)
            return(-errno);
        rc = mkdir(ctxt->conf.session_db_path, 0755);
        if (rc)
            return(-errno);
    }
    else if (!S_ISDIR(stat_buf.st_mode))
        return(-ENOTDIR);

    return(0); 
}  /* prep_db() */


/*
 * static gint build_db_paths(RastaCGIContext *ctxt)
 *
 * Builds all the cached paths
 */
static gint build_db_paths(RastaCGIContext *ctxt)
{
    gint rc;
    gchar *base_path;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);

    base_path = getenv("PATH_TRANSLATED");
    if (!base_path || (base_path[0] == '\0'))
        return(-ENOTDIR);

    ctxt->conf.base_path = g_strdup(base_path);
    if (!ctxt->conf.base_path)
        return(-ENOMEM);

    rc = -EBADF;
    ctxt->conf.config_path =
        g_build_filename(base_path, "cgirasta.conf", NULL);
    if (!ctxt->conf.config_path)
        goto out;
    ctxt->conf.description_path =
        g_build_filename(base_path, "cgi.rasta", NULL);
    if (!ctxt->conf.description_path)
        goto out;
    ctxt->conf.user_db_path =
        g_build_filename(base_path, "users", NULL);
    if (!ctxt->conf.user_db_path)
        goto out;
    ctxt->conf.auth_db_path =
        g_build_filename(base_path, "auths", NULL);
    if (!ctxt->conf.auth_db_path)
        goto out;
    ctxt->conf.session_db_path =
        g_build_filename(base_path, "sessions", NULL);
    if (!ctxt->conf.session_db_path)
        goto out;
    ctxt->conf.template_path =
        g_build_filename(base_path, "templates", NULL);
    if (!ctxt->conf.template_path)
        goto out;

    rc = 0;
out:
    if (rc)
        free_cgi_conf(ctxt);

    return(rc);
}  /* build_db_paths() */


/*
 * static gint validate_doc(xmlDocPtr doc,
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
static gint validate_doc(xmlDocPtr doc,
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
}  /* validate_doc() */


/*
 * static gint load_cgi_conf(RastaCGIContext *ctxt)
 *
 * Reads the CGI configuration and fills in ctxt->conf
 */
static gint load_cgi_conf(RastaCGIContext *ctxt)
{
    xmlDocPtr doc;
    xmlNsPtr ns;
    xmlNodePtr cur;
    gint rc;
    gchar *name, *value;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);

    doc = xmlParseFile(ctxt->conf.config_path);
    if (!doc)
        return(-EBADF);

    rc = validate_doc(doc, "RASTACGICONF",
                      "file:" _RASTA_DATA_DIR
                      G_DIR_SEPARATOR_S RASTACGICONF_DTD);
    if (rc)
        goto out_free_doc;

    rc = -EBADF;
    cur = xmlDocGetRootElement(doc);
    if (!cur)
        goto out_free_doc;

    ns = xmlSearchNsByHref(doc, cur, RASTA_NAMESPACE);
    if (!ns)
        goto out_free_doc;

    if (xmlStrcmp(cur->name, "RASTACGICONF"))
        goto out_free_doc;

    cur = cur->children;
    while (cur)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            (cur->ns == ns))
        {
            if (!xmlStrcmp(cur->name, "PREFERENCE"))
            {
                name = xmlGetProp(cur, "NAME");
                if (name)
                {
                    value = xmlGetProp(cur, "VALUE");
                    if (!xmlStrcmp(name, "usermodify"))
                    {
                        if (!value ||
                            !xmlStrcmp(value, "admin"))
                        {
                            /* Shouldn't happen, but we don't care */
                            ctxt->conf.user_modify_level =
                                RASTA_CGI_AUTH_ADMIN;
                        }
                        else if (!xmlStrcmp(value, "user"))
                        {
                            ctxt->conf.user_modify_level =
                                RASTA_CGI_AUTH_USER;
                        }
                        else if (!xmlStrcmp(value, "none"))
                        {
                            ctxt->conf.user_modify_level =
                                RASTA_CGI_AUTH_NONE;
                        }
                        else
                        {
                            g_assert_not_reached();
                        }
                    }
                    g_free(value);
                    g_free(name);
                }
            }
        }
        cur = cur->next;
    }

    rc = 0;

out_free_doc:
    xmlFreeDoc(doc);

    return(rc);
}  /* load_cgi_conf() */


/*
 * static gint check_timestamp(const gchar *timestamp, gint seconds)
 *
 * Returns 0 if the timestamp is less than seconds old
 */
static gint check_timestamp(const gchar *timestamp, gint seconds)
{
    time_t stamp_t, cur_t;
    gchar *ptr;

    g_return_val_if_fail(timestamp != NULL, 1);

    stamp_t = (time_t)strtol(timestamp, &ptr, 10);
    if (!ptr || (*ptr != '\0'))
        return 1;

    cur_t = time(NULL);
    return(cur_t > (stamp_t + seconds));
}  /* check_timestamp() */


/*
 * static gint load_user(RastaCGIContext *ctxt, const gchar *userid)
 *
 * Loads the information about said user.
 */
static gint load_user(RastaCGIContext *ctxt, const gchar *userid)
{
    gint rc;
    xmlDocPtr doc;
    xmlNsPtr ns;
    xmlNodePtr cur;
    gchar *user_file, *key, *auth_level;

    user_file = g_build_filename(ctxt->conf.user_db_path,
                                 userid,
                                 NULL);
    if (!user_file)
        return(-ENOMEM);

    doc = xmlParseFile(user_file);
    g_free(user_file);
    if (!doc)
        return(-EBADF);

    rc = validate_doc(doc, "RASTACGIUSER",
                      "file:" _RASTA_DATA_DIR
                      G_DIR_SEPARATOR_S RASTACGIUSER_DTD);
    if (rc)
        goto out_free_doc;

    rc = -EBADF;
    cur = xmlDocGetRootElement(doc);
    if (!cur)
        goto out_free_doc;

    ns = xmlSearchNsByHref(doc, cur, RASTA_NAMESPACE);
    if (!ns)
        goto out_free_doc;

    if (xmlStrcmp(cur->name, "RASTACGIUSER"))
        goto out_free_doc;

    cur = cur->children;
    while (cur)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            (cur->ns = ns))
        {
            if (!xmlStrcmp(cur->name, "USER"))
            {
                key = xmlGetProp(cur, "USERID");
                if (key)
                {
                    if (!xmlStrcmp(key, userid))
                    {
                        g_free(key);
                        break;
                    }
                    g_free(key);
                }
            }
        }
        cur = cur->next;
    }

    rc = -ENOENT;
    if (!cur)
        goto out_free_doc;

    ctxt->user.userid = g_strdup(userid);
    ctxt->user.name = xmlGetProp(cur, "NAME");
    ctxt->user.crypt = xmlGetProp(cur, "CRYPT");
    if (!ctxt->user.name || !ctxt->user.crypt)
        goto out_free_user;

    auth_level = xmlGetProp(cur, "AUTHLEVEL");
    if (!auth_level) /* Paranoia */
        ctxt->user.auth_level = RASTA_CGI_AUTH_NONE;
    else
    {
        if (!strcmp(auth_level, "user"))
            ctxt->user.auth_level = RASTA_CGI_AUTH_USER;
        else if (!strcmp(auth_level, "admin"))
            ctxt->user.auth_level = RASTA_CGI_AUTH_ADMIN;
        else  /* "none" or invalid */
            ctxt->user.auth_level = RASTA_CGI_AUTH_NONE;

        g_free(auth_level);
    }

    rc = 0;

out_free_user:
    if (rc)
        free_user(ctxt);

out_free_doc:
    xmlFreeDoc(doc);

    return(rc);
}  /* load_user() */


/*
 * static gint load_auth(RastaCGIContext *ctxt)
 *
 * Checks the CGI info to see if there is a valid user.  If there
 * is, load that user's information.  If not, return empty user
 * iformation.  A return of nonzero means a FATAL error.  Failing to
 * load a user's authorization (or having none passed to begin with)
 * is NOT a fatal error.  Failure to load can occur when the user
 * has yet to log in or is in the process of logging in.  This is
 * quite fine, and returns 0.
 */
static gint load_auth(RastaCGIContext *ctxt)
{
    gint rc;
    gchar *auth_cookie, *auth_file;
    gchar *key, *userid, *timestamp;
    xmlDocPtr doc;
    xmlNsPtr ns;
    xmlNodePtr cur;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->cgi != NULL, -EINVAL);

    rc = 0;

    auth_cookie = j_cgi_get_cookie(ctxt->cgi, "a");
    if (!auth_cookie)
        return(0);
    if (auth_cookie[0] == '\0')
    {
        g_free(auth_cookie);
        return(0);
    }

    rc = 1;
    auth_file = g_build_filename(ctxt->conf.auth_db_path,
                                 auth_cookie,
                                 NULL);
    if (!auth_file)
        goto out_free_cookie;

    doc = xmlParseFile(auth_file);
    g_free(auth_file);
    if (!doc)
        goto out_free_cookie;

    rc = validate_doc(doc, "RASTACGIAUTH",
                      "file:" _RASTA_DATA_DIR
                      G_DIR_SEPARATOR_S RASTACGIAUTH_DTD);
    if (rc)
        goto out_free_doc;

    rc = 1;
    cur = xmlDocGetRootElement(doc);
    if (!cur)
        goto out_free_doc;

    ns = xmlSearchNsByHref(doc, cur, RASTA_NAMESPACE);
    if (!ns)
        goto out_free_doc;

    if (xmlStrcmp(cur->name, "RASTACGIAUTH"))
        goto out_free_doc;

    cur = cur->children;
    while (cur)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            (cur->ns = ns))
        {
            if (!xmlStrcmp(cur->name, "AUTH"))
            {
                key = xmlGetProp(cur, "KEY");
                if (key)
                {
                    if (!xmlStrcmp(key, auth_cookie))
                    {
                        g_free(key);
                        break;
                    }
                    g_free(key);
                }
            }
        }
        cur = cur->next;
    }

    if (!cur)
        goto out_free_doc;

    timestamp = xmlGetProp(cur, "TIMESTAMP");
    if (timestamp)
    {
        rc = check_timestamp(timestamp, AUTH_TIMEOUT_SECS);
        g_free(timestamp);
    }
    else
        rc = 1;

    /* FIXME: Should remove the auth */
    if (rc)
        goto out_free_doc;

    userid = xmlGetProp(cur, "USERID");
    if (!userid)
        goto out_free_doc;

    rc = load_user(ctxt, userid);
    g_free(userid);

out_free_doc:
    xmlFreeDoc(doc);
out_free_cookie:
    if (rc)
        g_free(auth_cookie);
    else
    {
        ctxt->user.auth_cookie = auth_cookie;
        save_auth(ctxt);
    }

    return(0);
}  /* load_auth() */


/*
 * static gint save_auth(RastaCGIContext *ctxt)
 * 
 * Save the context's authorization.
 */
static gint save_auth(RastaCGIContext *ctxt)
{
    gchar *auth_file, *timestamp;
    gint rc;
    FILE *out_f;
    xmlDocPtr doc;
    xmlDtdPtr dtd;
    xmlNsPtr ns;
    xmlAttrPtr attr;
    xmlNodePtr cur, node;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->user.userid != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->user.userid[0] != '\0', -EINVAL);

    if (ctxt->user.auth_cookie && (ctxt->user.auth_cookie[0] == '\0'))
    {
        g_free(ctxt->user.auth_cookie);
        ctxt->user.auth_cookie = NULL;
    }

    if (!ctxt->user.auth_cookie)
    {
        ctxt->user.auth_cookie = get_new_hash(ctxt->user.userid);
        if (!ctxt->user.auth_cookie)
            return(-ENOMEM);
    }

    auth_file = g_build_filename(ctxt->conf.auth_db_path,
                                 ctxt->user.auth_cookie,
                                 NULL);
    if (!auth_file)
        return(-ENOMEM);

    rc = -ENOMEM;
    doc = xmlNewDoc("1.0");
    if (!doc)
        goto out_free_auth;
    dtd = xmlCreateIntSubset(doc, "RASTACGIAUTH", NULL,
                             "file:" _RASTA_DATA_DIR
                             G_DIR_SEPARATOR_S RASTACGIAUTH_DTD);
    if (!dtd)
        goto out_free_doc;
    cur = xmlNewNode(NULL, "RASTACGIAUTH");
    if (!cur)
        goto out_free_doc;
    ns = xmlNewNs(cur, RASTA_NAMESPACE, NULL);
    xmlDocSetRootElement(doc, cur);
    if (!ns)
        goto out_free_doc;
    node = xmlNewNode(NULL, "AUTH");
    if (!node)
        goto out_free_doc;
    xmlAddChild(cur, node);
    attr = xmlSetProp(node, "USERID", ctxt->user.userid);
    if (!attr)
        goto out_free_doc;
    attr = xmlSetProp(node, "KEY", ctxt->user.auth_cookie);
    if (!attr)
        goto out_free_doc;
    timestamp = g_strdup_printf("%ld", time(NULL));
    if (!timestamp)
        goto out_free_doc;
    attr = xmlSetProp(node, "TIMESTAMP", timestamp);
    g_free(timestamp);
    if (!attr)
        goto out_free_doc;
    
    out_f = fopen(auth_file, "w");
    rc = -errno;
    if (!out_f)
        goto out_free_doc;

    rc = xmlDocDump(out_f, doc);
    if (rc > -1)
        rc = 0;

out_free_doc:
    xmlFreeDoc(doc);

out_free_auth:
    g_free(auth_file);

    return(rc);
}  /* save_auth() */


/*
 * static gint drop_auth(RastaCGIContext *ctxt)
 * 
 * Drops the context's authorization.
 */
static gint drop_auth(RastaCGIContext *ctxt)
{
    gchar *auth_file;
    gint rc;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);

    /* No auth exists */
    if (!ctxt->user.auth_cookie)
        return(0);

    rc = 0;
    if (ctxt->user.auth_cookie[0] == '\0')
        goto out_free_cookie;

    auth_file = g_build_filename(ctxt->conf.auth_db_path,
                                 ctxt->user.auth_cookie,
                                 NULL);

    rc = -ENOMEM;
    if (!auth_file)
        goto out_free_cookie;

    rc = unlink(auth_file);
    if (rc)
        rc = -errno;

    g_free(auth_file);

out_free_cookie:
    g_free(ctxt->user.auth_cookie);
    ctxt->user.auth_cookie = NULL;

    return(rc);
}  /* drop_auth() */
 
/*
 * static gint load_session(RastaCGIContext *ctxt)
 *
 * Checks the CGI info to see if there is a valid user.  If there
 * is, load that user's information.  If not, return empty user
 * iformation.  A return of nonzero means a FATAL error.  Failing to
 * load a user's sessionorization (or having none passed to begin with)
 * is NOT a fatal error.  Failure to load can occur when the user
 * has yet to log in or is in the process of logging in.  This is
 * quite fine, and returns 0.
 */
static gint load_session(RastaCGIContext *ctxt)
{
    gint rc;
    gchar *session_key, *session_file;
    gchar *key, *userid, *timestamp, *type;
    xmlDocPtr doc;
    xmlNsPtr ns;
    xmlNodePtr cur;

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->cgi != NULL, -EINVAL);

    rc = 0;

    session_key = j_cgi_get_parameter(ctxt->cgi, "k");
    if (!session_key)
        return(0);
    if (session_key[0] == '\0')
        goto out_free_key;

    if (!xmlStrcmp(session_key, "login"))
    {
        ctxt->session.key = session_key;
        ctxt->session.state_file = NULL;
        ctxt->session.type = RASTA_CGI_SESSION_DESCRIPTION;
        return(0);
    }

    session_file = g_build_filename(ctxt->conf.session_db_path,
                                    session_key,
                                    NULL);
    if (!session_file)
        goto out_free_key;

    doc = xmlParseFile(session_file);
    g_free(session_file);
    if (!doc)
        goto out_free_key;

    rc = validate_doc(doc, "RASTACGISESSION",
                      "file:" _RASTA_DATA_DIR
                      G_DIR_SEPARATOR_S RASTACGISESSION_DTD);
    if (rc)
        goto out_free_doc;

    cur = xmlDocGetRootElement(doc);
    if (!cur)
        goto out_free_doc;

    ns = xmlSearchNsByHref(doc, cur, RASTA_NAMESPACE);
    if (!ns)
        goto out_free_doc;

    if (xmlStrcmp(cur->name, "RASTACGISESSION"))
        goto out_free_doc;

    cur = cur->children;
    while (cur)
    {
        if ((cur->type == XML_ELEMENT_NODE) &&
            (cur->ns = ns))
        {
            if (!xmlStrcmp(cur->name, "SESSION"))
            {
                key = xmlGetProp(cur, "KEY");
                if (key)
                {
                    userid = xmlGetProp(cur, "USERID");
                    if (userid)
                    {
                        if (!xmlStrcmp(key, session_key) &&
                            !xmlStrcmp(userid, ctxt->user.userid))
                        {
                            ctxt->session.key = key;
                            g_free(userid);
                            break;
                        }
                        g_free(userid);
                    }
                    g_free(key);
                }
            }
        }
        cur = cur->next;
    }

    if (!cur)
        goto out_free_doc;

    timestamp = xmlGetProp(cur, "TIMESTAMP");
    if (timestamp)
    {
        rc = check_timestamp(timestamp, SESSION_TIMEOUT_SECS);
        g_free(timestamp);
    }
    else
        rc = 1;

    /* FIXME: Should remove the session */
    if (rc)
        goto out_free_session;

    ctxt->session.state_file = xmlGetProp(cur, "FILE");

    type = xmlGetProp(cur, "TYPE");
    if (!type) /* Paranoia */
        ctxt->session.type = RASTA_CGI_SESSION_DESCRIPTION;
    else
    {
        if (!strcmp(type, "user"))
            ctxt->session.type = RASTA_CGI_SESSION_USER;
        else  /* "description" or invalid */
            ctxt->session.type = RASTA_CGI_SESSION_DESCRIPTION;
        g_free(type);
    }

    rc = 0;

out_free_session:
    if (rc)
        free_session(ctxt);
out_free_doc:
    xmlFreeDoc(doc);
out_free_key:
    g_free(session_key);

    return(0);
}  /* load_session() */


/*
 * static void free_user(RastaCGIContext *ctxt)
 *
 * Frees any allocations made to ctxt->user
 */
static void free_user(RastaCGIContext *ctxt)
{
    if (ctxt->user.userid)
    {
        g_free(ctxt->user.userid);
        ctxt->user.userid = NULL;
    }
    if (ctxt->user.name)
    {
        g_free(ctxt->user.name);
        ctxt->user.name = NULL;
    }
    if (ctxt->user.crypt)
    {
        g_free(ctxt->user.crypt);
        ctxt->user.crypt = NULL;
    }
}  /* free_user() */


/*
 * static void free_session(RastaCGIContext *ctxt)
 *
 * Frees any allocations made to ctxt->session
 */
static void free_session(RastaCGIContext *ctxt)
{
    if (ctxt->session.key)
    {
        g_free(ctxt->session.key);
        ctxt->session.key = NULL;
    }
    if (ctxt->session.state_file)
    {
        g_free(ctxt->session.state_file);
        ctxt->session.state_file = NULL;
    }
}  /* free_session() */


/*
 * static void free_cgi_conf(RastaCGIContext *ctxt)
 *
 * Frees any allocations made to ctxt->conf
 */
static void free_cgi_conf(RastaCGIContext *ctxt)
{
    if (ctxt->conf.base_path)
    {
        g_free(ctxt->conf.base_path);
        ctxt->conf.base_path = NULL;
    }
    if (ctxt->conf.config_path)
    {
        g_free(ctxt->conf.config_path);
        ctxt->conf.config_path = NULL;
    }
    if (ctxt->conf.description_path)
    {
        g_free(ctxt->conf.description_path);
        ctxt->conf.description_path = NULL;
    }
    if (ctxt->conf.user_db_path)
    {
        g_free(ctxt->conf.user_db_path);
        ctxt->conf.user_db_path = NULL;
    }
    if (ctxt->conf.auth_db_path)
    {
        g_free(ctxt->conf.auth_db_path);
        ctxt->conf.auth_db_path = NULL;
    }
    if (ctxt->conf.session_db_path)
    {
        g_free(ctxt->conf.session_db_path);
        ctxt->conf.session_db_path = NULL;
    }
    if (ctxt->conf.template_path)
    {
        g_free(ctxt->conf.template_path);
        ctxt->conf.template_path = DEFAULT_TEMPLATE_PATH;
    }
}  /* free_cgi_conf() */



/*
 * static gchar *get_new_hash(const gchar *user)
 *
 * Creates a new hash for a session.
 *
 * FIXME: This hash is not seeded very well.  It is an SHA1 has of
 * 16 bytes.  The first bytes are the current time_t.  After that, the
 * user name is repeated as long as it fits.  Real security needs
 * something less predictable, but /dev/urandom is not standard so
 * I'd like something more portable.  Anyone?
 */
static gchar *get_new_hash(const gchar *user)
{
    gchar *ptr;
    gint remain, usize, i, len;
    guint8 *byte_ptr;
    guint32 data[HASH_DATA_SIZE];
    guint32 hash_data[HASH_BUFFER_SIZE + HASH_EXTRA_SIZE];
    GString *hash_string;

    g_return_val_if_fail(user != NULL, NULL);
    g_return_val_if_fail(user[0] != '\0', NULL);

    usize = strlen(user);

    remain = HASH_DATA_SIZE * sizeof(guint32);
    byte_ptr = (guint8 *)data;

    if (sizeof(time_t) > remain)
        return(NULL);

    time((time_t *)byte_ptr);
    byte_ptr += sizeof(time_t);
    remain -= sizeof(time_t);

    len = usize;
    while (remain)
    {
        if (len > remain)
            len = remain;
        memcpy(byte_ptr, user, len);
        remain -= len;
        byte_ptr += len;
    }

    memset(hash_data, 0,
           (HASH_BUFFER_SIZE + HASH_EXTRA_SIZE) * sizeof(gint32));
    SHATransform(hash_data, data);

    len = HASH_BUFFER_SIZE * sizeof(guint32);
    hash_string = g_string_sized_new(len + 1);
    if (!hash_string)
        return(NULL);

    byte_ptr = (guint8 *)hash_data;
    for (i = 0; i < len; i++)
        g_string_append_printf(hash_string, "%02X", byte_ptr[i]);

    ptr = hash_string->str;
    g_string_free(hash_string, FALSE);
    return(ptr);
}  /* get_new_hash() */


/*
 * static gchar *html_replace(const gchar *templ, GHashTable *symbols)
 *
 * Substitues expressions of the form #sometext# with the
 * corresponding value of the key "sometext" in the string template.
 */
static gchar *html_replace(gchar *templ, GHashTable *symbols)
{
    GString *new_text;
    gchar *t_start, *t_end, *t_fmt, *t_symbol, *t_val;
    gboolean done;

    enum
    {
        REPLACE_NOVAR,
        REPLACE_INVAR
    } replace_state;

    g_return_val_if_fail(templ != NULL, NULL);


    if (symbols == NULL)
        return(g_strdup(templ));

    new_text = g_string_sized_new(strlen(templ));
    if (new_text == NULL)
        return(NULL);

    t_start = templ;
    t_end = templ;
    done = FALSE;
    replace_state = REPLACE_NOVAR;
    while (done == FALSE)
    {
        if (replace_state == REPLACE_NOVAR)
        {
            switch (*t_end)
            {
                case '\0':
                    done = TRUE;
                    /* FALL THROUGH */

                case '#':
                    replace_state = REPLACE_INVAR;

                    t_fmt = g_strdup_printf("%%.%ds",
                                            t_end - t_start);
                    g_string_append_printf(new_text,
                                           t_fmt,
                                           t_start);
                    g_free(t_fmt);
                    t_start = t_end;
                    break;

                default:
                    break;
            }
        }
        else if (replace_state == REPLACE_INVAR)
        {
            switch (*t_end)
            {
                case '#':
                    replace_state = REPLACE_NOVAR;

                    t_fmt = g_strdup_printf("%%.%ds",
                                            t_end - t_start - 1);
                    t_symbol = g_strdup_printf(t_fmt, t_start + 1);

                    t_val = g_hash_table_lookup(symbols,
                                                t_symbol);
                    if (t_val != NULL)
                        g_string_append(new_text, t_val);

                    g_free(t_symbol);

                    /* +1 to skip the ending '#' */
                    t_start = t_end + 1;

                    break;

                case '\0':
                    done = TRUE;
                    /* FALL THROUGH */

                default:
                    if ((*t_end == '\0') ||
                        (strchr(VALID_REPLACE_CHARS, *t_end) == NULL))
                    {
                        replace_state = REPLACE_NOVAR;

                        t_fmt = g_strdup_printf("%%.%ds",
                                                t_end - t_start);
                        g_string_append_printf(new_text,
                                               t_fmt,
                                               t_start);
                        g_free(t_fmt);
                        t_start = t_end;
                    }
                    break;
            }
        }
        else
            g_error("Shouldn't get here");

        t_end++;
    }

    t_start = g_strdup(new_text->str);
    g_string_free(new_text, TRUE);
    return(t_start);
}  /* html_replace() */


/*
 * static gint quick_check(RastaCGIContext *ctxt)
 *
 * A quick sanity check to be sure we have a proper cgirasta db
 */
static gint quick_check(RastaCGIContext *ctxt)
{
    gint rc;
    struct stat stat_buf = {0, };

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->conf.base_path != NULL, -EINVAL);
    g_return_val_if_fail(ctxt->conf.description_path != NULL, -EINVAL);

    rc = stat(ctxt->conf.config_path, &stat_buf);
    if (!rc)
    {
        if (!S_ISREG(stat_buf.st_mode))
            rc = -EBADF;
    }
    else
        rc = -errno;

    if (rc)
        goto out;

    rc = stat(ctxt->conf.description_path, &stat_buf);
    if (!rc)
    {
        if (!S_ISREG(stat_buf.st_mode))
            rc = -EBADF;
    }
    else
        rc = -errno;

out:
    return(rc);
}  /* quick_check() */



/*
 * Main program
 */


gint main(gint argc, gchar *argv[])
{
    gint rc;
    const gchar *name, *path;
    RastaCGIContext ctxt = {0, };

    /* Default, possibly overridden in build_db_paths() */
    ctxt.conf.template_path = DEFAULT_TEMPLATE_PATH;

    name = g_getenv("SCRIPT_NAME");
    path = g_getenv("PATH_INFO");
    if (!name || !path ||
        (name[0] == '\0') || (path[0] == '\0'))
    {
        send_error(&ctxt, "No path to configuration", FALSE);
        return(0);
    }

    /* separator starts "path" */
    ctxt.cgi_script = g_strdup_printf("%s%s", name, path);
    if (!ctxt.cgi_script)
    {
        send_error(&ctxt, "Unable to determine path to configuration",
                   FALSE);
        return(0);
    }

    rc = build_db_paths(&ctxt);
    if (rc)
    {
        send_error(&ctxt, "Unable to determine CGI data locations",
                   FALSE);
        return(0);
    }

    load_cgi_conf(&ctxt);
    if (rc)
    {
        send_error(&ctxt, "Unable to load CGI configuration", FALSE);
        goto out_free_conf;
    }

    /* Sanity before we (possibly) modify the filesystem */
    rc = quick_check(&ctxt);
    if (rc)
    {
        send_error(&ctxt, "Not a valid CGI data location", FALSE);
        goto out_free_conf;
    }

    rc = prep_db(&ctxt);
    if (rc)
    {
        send_error(&ctxt,
                   "There was a problem with the session database",
                   FALSE);
        goto out_free_conf;
    }

    ctxt.cgi = j_cgi_init();
    if (!ctxt.cgi)
    {
        send_error(&ctxt, "Unable to read CGI data", FALSE);
        goto out_free_conf;
    }
    if (j_cgi_get_error(ctxt.cgi))
    {
        send_error(&ctxt, j_cgi_get_error(ctxt.cgi), FALSE);
        goto out_free_cgi;
    }

    rc = load_auth(&ctxt);
    if (rc)
    {
        /*
         * Note that merely failing to load auth info is not fatal.
         * load_auth() returns an error only upon fatal problems.
         */
        send_error(&ctxt,
                   "There was a problem loading authorization information",
                   FALSE);
        goto out_free_cgi;
    }

    /* Even unauth'd people might have sessions, specifically 'login' */
    rc = load_session(&ctxt);
    if (rc)
    {
        /* like auth, error is only upon fatal problems */
        send_error(&ctxt,
                   "There was a problem loading session information",
                   FALSE);
        goto out_free_user;
    }

    /*
     * Errors are fatal now.  All things inside of handle_session() do
     * their own output
     */
    rc = handle_session(&ctxt);
    if (rc)
    {
        send_error(&ctxt,
                   "There was a problem handling the session",
                   FALSE);
        goto out_free_session;
    }

out_free_session:
    free_session(&ctxt);
out_free_user:
    free_user(&ctxt);
out_free_cgi:
    j_cgi_free(ctxt.cgi);
out_free_conf:
    free_cgi_conf(&ctxt);
    return(0);
}  /* main() */
