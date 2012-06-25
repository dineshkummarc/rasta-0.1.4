/*
 * rastadialog.c
 *
 * Code file for the Rasta dialog objects.
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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "rasta.h"
#include "rastacontext.h"
#include "rastascreen.h"
#include "rastadialog.h"
#include "rastatraverse.h"
#include "rastascope.h"
#include "rastaexec.h"



/*
 * Defines
 */
#define RASTA_ANY_DIALOG_FIELD(x)         ((RastaAnyDialogField *)(x))
#define RASTA_READONLY_DIALOG_FIELD(x)    ((RastaReadonlyDialogField *)(x))
#define RASTA_DESCRIPTION_DIALOG_FIELD(x) ((RastaDescriptionDialogField *)(x))
#define RASTA_ENTRY_DIALOG_FIELD(x)       ((RastaEntryDialogField *)(x))
#define RASTA_FILE_DIALOG_FIELD(x)        ((RastaFileDialogField *)(x))
#define RASTA_RING_DIALOG_FIELD(x)        ((RastaRingDialogField *)(x))
#define RASTA_LIST_DIALOG_FIELD(x)        ((RastaListDialogField *)(x))
#define RASTA_ENTRY_LIST_DIALOG_FIELD(x)   ((RastaEntryListDialogField *)(x))

#ifdef ENABLE_DEPRECATED
#define RASTA_INET_ADDR_DIALOG_FIELD(x)    ((RastaInetAddrDialogField *)(x))
#define RASTA_VOLUME_DIALOG_FIELD(x)    ((RastaVolumeDialogField *)(x))
#endif  /* ENABLE_DEPRECATED */




/*
 * Typedefs
 */
typedef struct _RastaAnyDialogField         RastaAnyDialogField;
typedef struct _RastaReadonlyDialogField    RastaReadonlyDialogField;
typedef struct _RastaDescriptionDialogField RastaDescriptionDialogField;
typedef struct _RastaEntryListDialogField   RastaEntryListDialogField;
typedef struct _RastaEntryListDialogField   RastaEntryDialogField;
typedef struct _RastaEntryListDialogField   RastaListDialogField;
typedef struct _RastaFileDialogField        RastaFileDialogField;
typedef struct _RastaRingDialogField        RastaRingDialogField;

#ifdef ENABLE_DEPRECATED
typedef struct _RastaInetAddrDialogField    RastaInetAddrDialogField;
typedef struct _RastaVolumeDialogField      RastaVolumeDialogField;
#endif  /* ENABLE_DEPRECATED */



/*
 * Structures
 */
struct _RastaAnyDialogField
{
    RastaDialogFieldType type;
    gboolean required;
    gchar *name;
    gchar *text;
    gchar *help;
};

struct _RastaReadonlyDialogField
{
    RastaDialogFieldType type;
    gboolean required;
    gchar *name;
    gchar *text;
    gchar *help;
};

struct _RastaDescriptionDialogField
{
    RastaDialogFieldType type;
    gboolean required;
    gchar *name;
    gchar *text;
    gchar *help;
};

struct _RastaFileDialogField
{
    RastaDialogFieldType type;
    gboolean required;
    gchar *name;
    gchar *text;
    gchar *help;
};

struct _RastaRingDialogField
{
    RastaDialogFieldType type;
    gboolean required;
    gchar *name;
    gchar *text;
    gchar *help;
    GList *ring_values;
};

struct _RastaEntryListDialogField
{
    RastaDialogFieldType type;
    gboolean required;
    gchar *name;
    gchar *text;
    gchar *help;
    guint max_length;
    gboolean numeric;
    gboolean hidden;
    gchar *format;
    gboolean multiple;
    gboolean single_column;
    RastaEscapeStyleType escape_style;
    gchar *list_command;
    gchar *encoding;
};

struct _RastaInetAddrDialogField
{
    RastaDialogFieldType type;
    gboolean required;
    gchar *name;
    gchar *text;
    gchar *help;
    guint max_length;
};

struct _RastaVolumeDialogField
{
    RastaDialogFieldType type;
    gboolean required;
    gchar *name;
    gchar *text;
    gchar *help;
};

struct _RastaRingValue
{
    gchar *text;
    gchar *value;
};



/*
 * Unions
 */
union _RastaDialogField
{
    RastaDialogFieldType type;
    RastaAnyDialogField any;
    RastaReadonlyDialogField readonly;
    RastaDescriptionDialogField description;
    RastaEntryDialogField entry;
    RastaFileDialogField file;
    RastaRingDialogField ring;
    RastaListDialogField list;
    RastaEntryListDialogField entrylist;
#ifdef ENABLE_DEPRECATED
    RastaInetAddrDialogField iaddr;
    RastaVolumeDialogField volume;
#endif /* ENABLE_DEPRECATED */
};



/*
 * Prototypes
 */
static RastaDialogField *rasta_dialog_field_new(RastaContext *ctxt,
                                                xmlNodePtr node);
static gboolean
rasta_readonly_dialog_field_new(RastaContext *ctxt,
                                RastaDialogField *field,
                                xmlNodePtr node);
static gboolean
rasta_description_dialog_field_new(RastaContext *ctxt,
                                   RastaDialogField *field,
                                   xmlNodePtr node);
static gboolean
rasta_entry_dialog_field_new(RastaContext *ctxt,
                             RastaDialogField *field,
                             xmlNodePtr node);
static gboolean
rasta_file_dialog_field_new(RastaContext *ctxt,
                            RastaDialogField *field,
                            xmlNodePtr node);
static gboolean
rasta_ring_dialog_field_new(RastaContext *ctxt,
                            RastaDialogField *field,
                            xmlNodePtr node);
static gboolean
rasta_list_dialog_field_new(RastaContext *ctxt,
                            RastaDialogField *field,
                            xmlNodePtr node);
static gboolean
rasta_entry_list_dialog_field_new(RastaContext *ctxt,
                                  RastaDialogField *field,
                                  xmlNodePtr node);

#ifdef ENABLE_DEPRECATED
static gboolean
rasta_inet_addr_dialog_field_new(RastaContext *ctxt,
                                 RastaDialogField *field,
                                 xmlNodePtr node);
static gboolean
rasta_volume_dialog_field_new(RastaContext *ctxt,
                              RastaDialogField *field,
                              xmlNodePtr node);
#endif  /* ENABLE_DEPRECATED */

static RastaRingValue *rasta_ring_value_new(RastaContext *ctxt,
                                          xmlNodePtr node);
static gint rasta_split_format_limit(const gchar *limit,
                                     gint format_vals[2]);



/*
 * Functions
 */


/*
 * static RastaRingValue *rasta_ring_value_new(RastaContext *ctxt,
 *                                           xmlNodePtr node)
 *
 * Creates a new ring item
 */
static RastaRingValue *rasta_ring_value_new(RastaContext *ctxt,
                                          xmlNodePtr node)
{
    RastaRingValue *item;

    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(node != NULL, NULL);

    item = g_new0(RastaRingValue, 1);
    if (item == NULL)
        return(NULL);

    item->text = xmlGetProp(node, "TEXT");
    if (item->text == NULL)
    {
        g_free(item);
        return(NULL);
    }
    if (g_utf8_strlen(item->text, -1) == 0)
    {
        g_free(item->text);
        g_free(item);
        return(NULL);
    }
    
    item->value = xmlGetProp(node, "VALUE");
    if (item->value == NULL)
    {
        g_free(item->text);
        g_free(item);
        return(NULL);
    }

#if DEBUG
    g_print("loaded ring value with text = \"%s\" and value = \"%s\"\n",
            item->text, item->value);
#endif /* DEBUG */
    return(item);
}  /* rasta_ring_value_new() */


/*
 * static gboolean
 * rasta_readonly_dialog_field_new(RastaContext *ctxt,
 *                                 RastaDialogField *field,
 *                                 xmlNodePtr node)
 */
static gboolean
rasta_readonly_dialog_field_new(RastaContext *ctxt,
                                RastaDialogField *field,
                                xmlNodePtr node)
{
    g_return_val_if_fail(field != NULL, FALSE);
    g_return_val_if_fail(node != NULL, FALSE);

    field->type = RASTA_FIELD_READONLY;

    return(TRUE);
}  /* rasta_readonly_dialog_field_new() */


/*
 * static gboolean
 * rasta_description_dialog_field_new(RastaContext *ctxt,
 *                                    RastaDialogField *field,
 *                                    xmlNodePtr node)
 */
static gboolean
rasta_description_dialog_field_new(RastaContext *ctxt,
                                   RastaDialogField *field,
                                    xmlNodePtr node)
{
    g_return_val_if_fail(field != NULL, FALSE);
    g_return_val_if_fail(node != NULL, FALSE);

    field->type = RASTA_FIELD_DESCRIPTION;

    return(TRUE);
}  /* rasta_description_dialog_field_new() */


/*
 * static gboolean
 * rasta_entry_dialog_field_new(RastaContext *ctxt,
 *                              RastaDialogField *field,
 *                              xmlNodePtr node)
 */
static gboolean
rasta_entry_dialog_field_new(RastaContext *ctxt,
                             RastaDialogField *field,
                             xmlNodePtr node)
{
    gchar *max_length, *ptr;
    RastaEntryDialogField *e_field;

    g_return_val_if_fail(field != NULL, FALSE);
    g_return_val_if_fail(node != NULL, FALSE);

    field->type = RASTA_FIELD_ENTRY;

    e_field = RASTA_ENTRY_DIALOG_FIELD(field);

    ptr = xmlGetProp(node, "HIDDEN");
    if (ptr != NULL)
    {
        if (xmlStrcmp(ptr, "true") == 0)
            e_field->hidden = TRUE;
        else
            e_field->hidden = FALSE;
        g_free(ptr);
    }
    else
        e_field->hidden = FALSE;

    ptr = xmlGetProp(node, "NUMERIC");
    if (ptr != NULL)
    {
        if (xmlStrcmp(ptr, "true") == 0)
            e_field->numeric = TRUE;
        else
            e_field->numeric = FALSE;
        g_free(ptr);
    }
    else
        e_field->numeric = FALSE;

    if (e_field->numeric == TRUE)
        e_field->format = xmlGetProp(node, "FORMAT");
    else
    {
        max_length = xmlGetProp(node, "LENGTH");
        if (max_length != NULL)
        {
            e_field->max_length = (guint)strtol(max_length, &ptr, 10);
            if (ptr[0] != '\0')
                e_field->max_length = 0;  /* FIXME:  Should this noisly error? */
            g_free(max_length);
        }
        else
            e_field->max_length = 0;
    }

    return(TRUE);
}  /* rasta_entry_dialog_field_new() */


/*
 * static gboolean
 * rasta_file_dialog_field_new(RastaContext *ctxt,
 *                             RastaDialogField *field,
 *                             xmlNodePtr node)
 */
static gboolean
rasta_file_dialog_field_new(RastaContext *ctxt,
                            RastaDialogField *field,
                            xmlNodePtr node)
{
    g_return_val_if_fail(field != NULL, FALSE);
    g_return_val_if_fail(node != NULL, FALSE);

    field->type = RASTA_FIELD_FILE;

    return(TRUE);
}  /* rasta_file_dialog_field_new() */


/*
 * static gboolean
 * rasta_ring_dialog_field_new(RastaContext *ctxt,
 *                             RastaDialogField *field,
 *                             xmlNodePtr node)
 */
static gboolean
rasta_ring_dialog_field_new(RastaContext *ctxt,
                            RastaDialogField *field,
                            xmlNodePtr node)
{
    RastaRingDialogField *r_field;
    RastaRingValue *item;
    xmlNodePtr cur;

    g_return_val_if_fail(field != NULL, FALSE);
    g_return_val_if_fail(node != NULL, FALSE);

    field->type = RASTA_FIELD_RING;

    r_field = RASTA_RING_DIALOG_FIELD(field);

    r_field->ring_values = NULL;
    cur = node->children;
    while (cur != NULL)
    {
        if (cur->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcmp(cur->name, "RINGVALUE") == 0)
            {
                item = rasta_ring_value_new(ctxt, cur);
                if (item != NULL)
                    r_field->ring_values =
                        g_list_append(r_field->ring_values, item);
            }
        }
        
        cur = cur->next;
    }

    return(TRUE);
}  /* rasta_ring_dialog_field_new() */


/*
 * static gboolean
 * rasta_list_dialog_field_new(RastaContext *ctxt,
 *                             RastaDialogField *field,
 *                             xmlNodePtr node)
 */
static gboolean
rasta_list_dialog_field_new(RastaContext *ctxt,
                            RastaDialogField *field,
                            xmlNodePtr node)
{
    gchar *ptr;
    RastaListDialogField *l_field;
    xmlNodePtr cur;

    g_return_val_if_fail(ctxt != NULL, FALSE);
    g_return_val_if_fail(field != NULL, FALSE);
    g_return_val_if_fail(node != NULL, FALSE);

    field->type = RASTA_FIELD_LIST;

    l_field = RASTA_ENTRY_DIALOG_FIELD(field);

    ptr = xmlGetProp(node, "MULTIPLE");
    if (ptr != NULL)
    {
        if (xmlStrcmp(ptr, "true") == 0)
            l_field->multiple = TRUE;
        else
            l_field->multiple = FALSE;
        g_free(ptr);
    }
    else
        l_field->multiple = FALSE;

    ptr = xmlGetProp(node, "SINGLECOLUMN");
    if (ptr != NULL)
    {
        if (xmlStrcmp(ptr, "true") == 0)
            l_field->single_column = TRUE;
        else
            l_field->single_column = FALSE;
        g_free(ptr);
    }
    else
        l_field->single_column = FALSE;

    cur = node->children;
    l_field->list_command = NULL;
    while ((cur != NULL) && (l_field->list_command == NULL))
    {
        if (cur->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcmp(cur->name, "LISTCOMMAND") == 0)
            {
                l_field->list_command =
                    xmlNodeListGetString(ctxt->doc, cur->children, 1);

                l_field->encoding = xmlGetProp(cur, "OUTPUTENCODING");

                ptr = xmlGetProp(cur, "ESCAPESTYLE");
                if (ptr == NULL)
                {
                    l_field->escape_style = RASTA_ESCAPE_STYLE_NONE;
                    continue;
                }

                if (strcmp(ptr, "single") == 0)
                    l_field->escape_style = RASTA_ESCAPE_STYLE_SINGLE;
                else if (strcmp(ptr, "double") == 0)
                    l_field->escape_style = RASTA_ESCAPE_STYLE_DOUBLE;
                else
                    l_field->escape_style = RASTA_ESCAPE_STYLE_NONE;

                g_free(ptr);
            }
        }
        
        cur = cur->next;
    }

    return(TRUE);
}  /* rasta_list_dialog_field_new() */


/*
 * static gboolean
 * rasta_entry_list_dialog_field_new(RastaContext *ctxt,
 *                                   RastaDialogField *field,
 *                                   xmlNodePtr node)
 */
static gboolean
rasta_entry_list_dialog_field_new(RastaContext *ctxt,
                                  RastaDialogField *field,
                                  xmlNodePtr node)
{
    g_return_val_if_fail(field != NULL, FALSE);
    g_return_val_if_fail(node != NULL, FALSE);

    if (rasta_entry_dialog_field_new(ctxt, field, node) == FALSE)
        return(FALSE);
    if (rasta_list_dialog_field_new(ctxt, field, node) == FALSE)
        return(FALSE);

    field->type = RASTA_FIELD_ENTRYLIST;

    return(TRUE);
}  /* rasta_entry_list_dialog_field_new() */


#ifdef ENABLE_DEPRECATED
/*
 * static gboolean
 * rasta_inet_addr_dialog_field_new(RastaContext *ctxt,
 *                                  RastaDialogField *field,
 *                                  xmlNodePtr node)
 */
static gboolean
rasta_inet_addr_dialog_field_new(RastaContext *ctxt,
                                 RastaDialogField *field,
                                 xmlNodePtr node)
{
    g_return_val_if_fail(field != NULL, FALSE);
    g_return_val_if_fail(node != NULL, FALSE);

    field->type = RASTA_FIELD_IPADDR;

    return(TRUE);
}  /* rasta_inet_addr_dialog_field_new() */


/*
 * static gboolean
 * rasta_volume_dialog_field_new(RastaContext *ctxt,
 *                               RastaDialogField *field,
 *                               xmlNodePtr node)
 */
static gboolean
rasta_volume_dialog_field_new(RastaContext *ctxt,
                              RastaDialogField *field,
                              xmlNodePtr node)
{
    g_return_val_if_fail(field != NULL, FALSE);
    g_return_val_if_fail(node != NULL, FALSE);

    field->type = RASTA_FIELD_VOLUME;

    return(TRUE);
}  /* rasta_volume_dialog_field_new() */
#endif  /* ENABLE_DEPRECATED */


/*
 * static RastaDialogField *rasta_dialog_field_new(RastaContext *ctxt,
 *                                                 xmlNodePtr node)
 *
 * Loads a new dialog field
 */
static RastaDialogField *rasta_dialog_field_new(RastaContext *ctxt,
                                                xmlNodePtr node)
{
    RastaDialogField *field;
    RastaAnyDialogField *any;
    gchar *ptr;
    gboolean success;

    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(node != NULL, NULL);

    field = g_new0(RastaDialogField, 1);
    if (field == NULL)
        return(NULL);

    any = RASTA_ANY_DIALOG_FIELD(field);

    any->name = xmlGetProp(node, "NAME");
    if (any->name == NULL)
        return(NULL);

    any->text = xmlGetProp(node, "TEXT");
    if (any->text == NULL)
    {
        g_free(any->name);
        g_free(field);
        return(NULL);
    }

    ptr = xmlGetProp(node, "REQUIRED");
    if (ptr != NULL)
    {
        if (xmlStrcmp(ptr, "true") == 0)
            any->required = TRUE;
        else
            any->required = FALSE;
        g_free(ptr);
    }
    else
        any->required = FALSE;

    any->help = rasta_query_help(ctxt, node);

    ptr = xmlGetProp(node, "TYPE");
    if (ptr == NULL)
    {
        g_free(any->text);
        g_free(any->name);
        g_free(field);
        return(NULL);
    }
    if (xmlStrcmp(ptr, "entry") == 0)
        success = rasta_entry_dialog_field_new(ctxt, field, node);
    else if (xmlStrcmp(ptr, "file") == 0)
        success = rasta_file_dialog_field_new(ctxt, field, node);
    else if (xmlStrcmp(ptr, "readonly") == 0)
        success = rasta_readonly_dialog_field_new(ctxt, field, node);
    else if (xmlStrcmp(ptr, "description") == 0)
        success = rasta_description_dialog_field_new(ctxt, field, node);
    else if (xmlStrcmp(ptr, "ring") == 0)
        success = rasta_ring_dialog_field_new(ctxt, field, node);
    else if (xmlStrcmp(ptr, "list") == 0)
        success = rasta_list_dialog_field_new(ctxt, field, node);
    else if (xmlStrcmp(ptr, "entrylist") == 0)
        success = rasta_entry_list_dialog_field_new(ctxt, field, node);
#ifdef ENABLE_DEPRECATED
    else if (xmlStrcmp(ptr, "ip_address") == 0)
        success = rasta_inet_addr_dialog_field_new(ctxt, field, node);
    else if (xmlStrcmp(ptr, "volume") == 0)
        success = rasta_volume_dialog_field_new(ctxt, field, node);
#endif  /* ENABLE_DEPRECATED */
    else
        success = FALSE;

    g_free(ptr);
    if (success == FALSE)
    {
        g_free(any->text);
        g_free(any->name);
        g_free(field);
        return(NULL);
    }

    return(field);
}  /* rasta_dialog_field_new() */


/*
 * void rasta_dialog_screen_load(RastaContext *ctxt,
 *                               RastaScreen *screen,
 *                               xmlNodePtr screen_node)
 *
 * Loads a dialog screen from the XML
 */
void rasta_dialog_screen_load(RastaContext *ctxt,
                              RastaScreen *screen,
                              xmlNodePtr screen_node)
{
    gchar *ptr;
    RastaDialogScreen *d_screen;
    RastaDialogField *field;
    xmlNodePtr cur;

    g_return_if_fail(screen != NULL);
    g_return_if_fail(screen_node != NULL);
    g_return_if_fail(xmlStrcmp(screen_node->name, "DIALOGSCREEN") == 0);
    
    d_screen = RASTA_DIALOG_SCREEN(screen);
    d_screen->title = xmlGetProp(screen_node, "TEXT");
    d_screen->fields = NULL;
    d_screen->init_command = NULL;
    d_screen->help = rasta_query_help(ctxt, screen_node);

    cur = screen_node->children;
    while (cur != NULL)
    {
        if (cur->type == XML_ELEMENT_NODE)
        {
            if (xmlStrcmp(cur->name, "FIELD") == 0)
            {
                field = rasta_dialog_field_new(ctxt, cur);
                if (field != NULL)
                    d_screen->fields =
                        g_list_append(d_screen->fields, field);
            }
            else if (xmlStrcmp(cur->name, "INITCOMMAND") == 0)
            {
                d_screen->init_command =
                    xmlNodeListGetString(ctxt->doc, cur->children, 1);

                d_screen->encoding = xmlGetProp(cur, "OUTPUTENCODING");

                ptr = xmlGetProp(cur, "ESCAPESTYLE");
                if (ptr == NULL)
                {
                    d_screen->escape_style = RASTA_ESCAPE_STYLE_NONE;
                    continue;
                }

                if (strcmp(ptr, "single") == 0)
                    d_screen->escape_style = RASTA_ESCAPE_STYLE_SINGLE;
                else if (strcmp(ptr, "double") == 0)
                    d_screen->escape_style = RASTA_ESCAPE_STYLE_DOUBLE;
                else
                    d_screen->escape_style = RASTA_ESCAPE_STYLE_NONE;

                g_free(ptr);
            }
        }

        cur = cur->next;
    }
}  /* rasta_dialog_screen_load() */


/*
 * void rasta_dialog_screen_init(RastaContext *ctxt,
 *                               RastaScreen *screen)
 *
 * Initializes the dialog screen for use.  It does initcommand
 * checking too
 */
void rasta_dialog_screen_init(RastaContext *ctxt,
                              RastaScreen *screen)
{
    RastaDialogScreen *d_screen;

    g_return_if_fail(ctxt != NULL);
    g_return_if_fail(screen != NULL);
    g_return_if_fail(screen->type == RASTA_SCREEN_DIALOG);

    d_screen = RASTA_DIALOG_SCREEN(screen);

    if (ctxt->state == RASTA_CONTEXT_INITIALIZED)
        ctxt->state = RASTA_CONTEXT_SCREEN;

    /* Check for the initcommand */
    if (d_screen->init_command != NULL)
    {
        if (ctxt->state == RASTA_CONTEXT_SCREEN)
        {
            ctxt->state = RASTA_CONTEXT_INITCOMMAND;
            return;
        }
        else if (ctxt->state == RASTA_CONTEXT_INITCOMMAND)
            ctxt->state = RASTA_CONTEXT_SCREEN;
        else
            g_warning("Invalid context state in INITCOMMAND\n");
    }
}  /* rasta_dialog_screen_init() */


/*
 * REnumeration *
 * rasta_dialog_screen_enumerate_fields(RastaContext *ctxt,
 *                                      RastaScreen *screen)
 *
 * Creates an enumeration object for the dialog fields
 */
REnumeration *rasta_dialog_screen_enumerate_fields(RastaContext *ctxt,
                                                   RastaScreen *screen)
{
    RastaDialogScreen *d_screen;
    REnumeration *enumer;

    g_return_val_if_fail(ctxt != NULL, NULL);
    g_return_val_if_fail(screen != NULL, NULL);
    g_return_val_if_fail(screen->type == RASTA_SCREEN_DIALOG, NULL);
    g_return_val_if_fail(ctxt->state == RASTA_CONTEXT_SCREEN, NULL);

    d_screen = RASTA_DIALOG_SCREEN(screen);

    enumer = r_enumeration_new_from_list(d_screen->fields);

    return(enumer);
}  /* rasta_dialog_screen_enumerate_fields() */


/*
 * gchar *rasta_dialog_field_get_text(RastaDialogField *field)
 *
 * Returns the text of the dialog field
 */
gchar *rasta_dialog_field_get_text(RastaDialogField *field)
{
    g_return_val_if_fail(field != NULL, NULL);

    return(g_strdup(RASTA_ANY_DIALOG_FIELD(field)->text));
}  /* rasta_dialog_field_get_text() */


/*
 * gchar *rasta_dialog_field_get_symbol_name(RastaDialogField *field)
 *
 * Returns the name of the dialog field's associated symbol
 */
gchar *rasta_dialog_field_get_symbol_name(RastaDialogField *field)
{
    g_return_val_if_fail(field != NULL, NULL);

    return(g_strdup(RASTA_ANY_DIALOG_FIELD(field)->name));
}  /* rasta_dialog_field_get_symbol_name() */


/*
 * RastaDialogFieldType
 * rasta_dialog_field_get_type(RastaDialogField *field)
 *
 * Returns the type of the dialog field
 */
RastaDialogFieldType
rasta_dialog_field_get_type(RastaDialogField *field)
{
    g_return_val_if_fail(field != NULL, RASTA_FIELD_NONE);

    return(field->type);
}  /* rasta_dialog_field_get_type() */


/*
 * gboolean rasta_dialog_field_is_required(RastaDialogField *field)
 *
 * Returns whether the field is required
 */
gboolean rasta_dialog_field_is_required(RastaDialogField *field)
{
    g_return_val_if_fail(field != NULL, FALSE);

    return(RASTA_ANY_DIALOG_FIELD(field)->required);
}  /* rasta_dialog_field_is_required() */


/*
 * gboolean rasta_entry_dialog_field_is_hidden(RastaDialogField *field)
 *
 * Returns whether the field is hidden.  Only entry fields can be
 * hidden.  (Think about it.  How do you list hidden passwords?)
 */
gboolean rasta_entry_dialog_field_is_hidden(RastaDialogField *field)
{
    g_return_val_if_fail(field != NULL, FALSE);
    g_return_val_if_fail(field->type == RASTA_FIELD_ENTRY, FALSE);

    return(RASTA_ENTRY_DIALOG_FIELD(field)->hidden);
}  /* rasta_entry_dialog_field_is_hidden() */


/*
 * gboolean rasta_entry_dialog_field_is_numeric(RastaDialogField *field)
 *
 * Returns whether the field is numeric
 */
gboolean rasta_entry_dialog_field_is_numeric(RastaDialogField *field)
{
    g_return_val_if_fail(field != NULL, FALSE);

    return(RASTA_ENTRY_DIALOG_FIELD(field)->numeric);
}  /* rasta_entry_dialog_field_is_numeric() */


/*
 * gchar *rasta_dialog_field_get_help(RastaDialogField *field)
 *
 * Returns the help text associated with the field
 */
gchar *rasta_dialog_field_get_help(RastaDialogField *field)
{
    g_return_val_if_fail(field != NULL, NULL);

    return(g_strdup(RASTA_ANY_DIALOG_FIELD(field)->help));
}  /* rasta_dialog_field_get_help() */


/*
 * gchar *rasta_entry_dialog_field_get_format(RastaDialogField *field)
 *
 * Returns the format associated with the numeric entry field
 */
gchar *rasta_entry_dialog_field_get_format(RastaDialogField *field)
{
    g_return_val_if_fail(field != NULL, NULL);

    return(g_strdup(RASTA_ENTRY_DIALOG_FIELD(field)->format));
}  /* rasta_entry_dialog_field_get_format() */


/*
 * static gint rasta_split_format_limit(const gchar *limits,
 *                                      gint format_vals[2])
 *
 * A format limit is of the form 'min,max'.  Valid forms are
 * 'min' ',max' 'min,max'.  Eg, '3' ',8' '3,8'.
 */
static gint rasta_split_format_limit(const gchar *limit,
                                     gint format_vals[2])
{
    gint rc = 0, test_res[2] = {-1, -1};
    gchar *ptr;
    gchar **test;

    g_return_val_if_fail(limit != NULL, FALSE);

    if (g_utf8_strlen(limit, -1) == 0)
        return(0);

    test = g_strsplit(limit, ",", 2);
    if (test == NULL)
        return(-ENOMEM);

    if (test[0] != NULL)
    {
        if (g_utf8_strlen(test[0], -1) >= 0)
        {
            test_res[0] = (gint)strtol(test[0], &ptr, 10);
            if (*ptr != '\0')
            {
                g_strfreev(test);
                rc = -EINVAL;
            }
            else
                rc = 0;
        }
    }
    else
        rc = -ENOMEM;

    if ((rc == 0) && 
        (test[1] != NULL) &&
        (g_utf8_strlen(test[1], -1) > 0))
    {
        test_res[1] = (gint)strtol(test[1], &ptr, 10);
        if (*ptr != '\0')
        {
            g_strfreev(test);
            rc = -EINVAL;
        }
    }

    g_strfreev(test);

    if (rc == 0)
    { 
        format_vals[0] = test_res[0];
        format_vals[1] = test_res[1];
    }
    return(rc);
}  /* rasta_split_format_limit() */


/*
 * gint
 * rasta_verify_number_format(const gchar *format,
 *                            const gchar *test_val)
 *
 * For applications that don't do number verification themselves
 * this function checks the test_val (which should be numeric) against
 * the format.  Formats are of the form 'limit.limit' for digits
 * before and after the decimal point.  Valid formats are of the forms
 * 'limit' 'limit.' '.limit' 'limit.limit'.  For example:
 * '9'     '9.2'    '.3,9'   '3,9.0,2'
 * The valid syntax for limits is described in the function
 * documentation for rasta_split_format_limit()
 *
 * NOTE that formats are specified in the C locale, with "." as the
 * decimal point and "," as the thousands separator.  The actual
 * data entered, however, is in the local locale.
 *
 * The return values are 0 for a valid number, -ERANGE for an invalid
 * number, -EINVAL for an invalid format string, and -ENOMEM if there
 * was an allocation error in the internal processing
 */
gint rasta_verify_number_format(const gchar *format,
                                const gchar *test_val)
{
    gint rc = 0, format_w[2] = {0, -1}, format_p[2] = {0, 0};
    gint rlen, llen;
    gdouble test_d;
    gchar **split;
    gchar *tmp;
    const gchar *ptr;
    struct lconv *lc;
    gunichar dp, ts, ns, ps, c;
    gboolean done, have_dp;

    g_return_val_if_fail(format != NULL, -EINVAL);
    g_return_val_if_fail(test_val != NULL, -EINVAL);
    g_return_val_if_fail(g_utf8_validate(test_val, -1, NULL) != FALSE,
                         -EINVAL);
    
    /* empty == always valid */
    if (g_utf8_strlen(format, -1) == 0)
        return(TRUE);

    lc = localeconv();

    if (lc->decimal_point)
    {
        tmp = g_locale_to_utf8(lc->decimal_point, -1, NULL, NULL, NULL);
        dp = g_utf8_get_char(tmp);
        g_free(tmp);
    }
    else
        dp = g_utf8_get_char(".");

    if (lc->thousands_sep)
    {
        tmp = g_locale_to_utf8(lc->thousands_sep, -1, NULL, NULL, NULL);
        ts = g_utf8_get_char(tmp);
        g_free(tmp);
    }
    else
        ts = g_utf8_get_char(",");

    if (lc->positive_sign)
    {
        tmp = g_locale_to_utf8(lc->positive_sign, -1, NULL, NULL, NULL);
        ps = g_utf8_get_char(tmp);
        g_free(tmp);
    }
    else
        ps = g_utf8_get_char("+");

    if (lc->negative_sign)
    {
        tmp = g_locale_to_utf8(lc->negative_sign, -1, NULL, NULL, NULL);
        ns = g_utf8_get_char(tmp);
        g_free(tmp);
    }
    else
        ns = g_utf8_get_char("-");

    split = g_strsplit(format, ".", 2);
    if (split == NULL)
        return(-ENOMEM);
    if (split[0] != NULL)
    {
        rc = rasta_split_format_limit(split[0], format_w);
        if ((rc == 0) && split[1] != NULL)
            rc = rasta_split_format_limit(split[1], format_p);
    }
    else
        rc = -ENOMEM;

    g_strfreev(split);
    
    if (rc != 0)
        return(rc);

    test_d = g_strtod(test_val, &tmp);
    if ((tmp == NULL) || (*tmp != '\0'))
        return(-ERANGE);

    done = FALSE;
    rlen = llen = 0;
    have_dp = FALSE;
    ptr = test_val;
    while (done == FALSE)
    {
        if (*ptr != '\0')
        {
            c = g_utf8_get_char(ptr);
            if ((c == ns) || (c == ps))
            {
                if (ptr != test_val)
                    g_assert_not_reached();
                /* Skip sign */
            }
            else if (c == dp)
            {
                if (have_dp == TRUE)
                    g_assert_not_reached();
                have_dp = TRUE;
            }
            else if (have_dp == FALSE)
                llen++;
            else
                rlen++;
            ptr = g_utf8_next_char(ptr);
        }
        else
            done = TRUE;
    }

    rc = 0;
    if ((llen < format_w[0]) ||
        ((format_w[1] > -1) && (llen > format_w[1])))
        rc = -ERANGE;
    if (have_dp == TRUE)
    {
        if ((rlen < format_p[0]) ||
            ((format_p[1] > -1) && (rlen > format_p[1])))
            rc = -ERANGE;
    }
    else if (format_p[0] > 0)
        rc = -ERANGE;

    return(rc);
}  /* rasta_verify_number_format() */


/*
 * guint rasta_entry_dialog_field_get_length(RastaDialogField *field)
 *
 * Returns the help text associated with the field
 */
guint rasta_entry_dialog_field_get_length(RastaDialogField *field)
{
    g_return_val_if_fail(field != NULL, 0);
    g_return_val_if_fail((field->type == RASTA_FIELD_ENTRY) ||
                         (field->type == RASTA_FIELD_ENTRYLIST),
                         0);

    return(RASTA_ENTRY_DIALOG_FIELD(field)->max_length);
}  /* rasta_entry_dialog_field_get_length() */



/*
 * void rasta_dialog_screen_next(RastaContext *ctxt)
 *
 * Selects the next screen.
 */
void rasta_dialog_screen_next(RastaContext *ctxt)
{
    xmlNodePtr node;

    g_return_if_fail(ctxt != NULL);

    node = rasta_traverse_forward(ctxt, NULL);
    rasta_scope_push(ctxt, node);
    rasta_screen_prepare(ctxt);
}  /* rasta_dialog_screen_next() */


/*
 * REnumeration *
 * rasta_dialog_field_enumerate_ring(RastaDialogField *field)
 *
 * Creates an enumeration object for the ring items
 */
REnumeration *
rasta_dialog_field_enumerate_ring(RastaDialogField *field)
{
    RastaRingDialogField *r_field;
    REnumeration *enumer;

    g_return_val_if_fail(field != NULL, NULL);
    g_return_val_if_fail(field->type == RASTA_FIELD_RING, NULL);

    r_field = RASTA_RING_DIALOG_FIELD(field);

    enumer = r_enumeration_new_from_list(r_field->ring_values);

    return(enumer);
}  /* rasta_dialog_field_enumerate_ring() */


/*
 * gchar *rasta_ring_value_get_text(RastaRingValue *value)
 *
 * Returns the text of the ring value
 */
gchar *rasta_ring_value_get_text(RastaRingValue *value)
{
    g_return_val_if_fail(value != NULL, NULL);

    return(g_strdup(value->text));
}  /* rasta_ring_value_get_text() */


/*
 * gchar *rasta_ring_value_get_value(RastaRingValue *value)
 *
 * Returns the text of the ring value
 */
gchar *rasta_ring_value_get_value(RastaRingValue *value)
{
    g_return_val_if_fail(value != NULL, NULL);

    return(g_strdup(value->value));
}  /* rasta_ring_value_get_value() */


/*
 * gchar *rasta_listcommand_get_encoding(RastaDialogField *field)
 *
 * Returns the encoding for the listcommand
 */
gchar *rasta_listcommand_get_encoding(RastaDialogField *field)
{
    const gchar *encoding;

    g_return_val_if_fail(field != NULL, NULL);
    g_return_val_if_fail((field->type == RASTA_FIELD_LIST) ||
                         (field->type == RASTA_FIELD_ENTRYLIST), NULL);

    encoding = RASTA_LIST_DIALOG_FIELD(field)->encoding;

    if (strcmp(encoding, "system") == 0)
        g_get_charset(&encoding);

    return(g_strdup(encoding));
}  /* rasta_listcommand_get_encoding() */


/*
 * gboolean rasta_listcommand_allow_multiple(RastaDialogField *field)
 *
 * Returns TRUE if the listcommand allows multiple selections
 */
gboolean rasta_listcommand_allow_multiple(RastaDialogField *field)
{
    g_return_val_if_fail(field != NULL, FALSE);
    g_return_val_if_fail((field->type == RASTA_FIELD_LIST) ||
                         (field->type == RASTA_FIELD_ENTRYLIST),
                         FALSE);

    return(RASTA_LIST_DIALOG_FIELD(field)->multiple);
}  /* rasta_listcommand_allow_multiple() */


/*
 * gboolean rasta_listcommand_is_single_column(RastaDialogField *field)
 *
 * Returns TRUE if the listcommand uses only the first column as the
 * resulting value
 */
gboolean rasta_listcommand_is_single_column(RastaDialogField *field)
{
    g_return_val_if_fail(field != NULL, FALSE);
    g_return_val_if_fail((field->type == RASTA_FIELD_LIST) ||
                         (field->type == RASTA_FIELD_ENTRYLIST),
                         FALSE);

    return(RASTA_LIST_DIALOG_FIELD(field)->single_column);
}  /* rasta_listcommand_is_single_column() */


/*
 * gint rasta_listcommand_run(RastaContext *ctxt,
 *                            RastaDialogField *field,
 *                            pid_t *pid,
 *                            gint *outfd,
 *                            gint *errfd)
 *
 * Fires off the listcommand, returning the pid and fds for the
 * calling process to handle.  The calling process is responsible for
 * converting the fd output into UTF-8 before passing any values back
 * to librasta.
 */
gint rasta_listcommand_run(RastaContext *ctxt,
                           RastaDialogField *field,
                           pid_t *pid,
                           gint *outfd,
                           gint *errfd)
{
    gint rc, l_outfd, l_errfd;
    pid_t l_pid;
    RastaListDialogField *l_field;
    gchar *utf8_cmd, *locale_cmd;
    gchar *argv[] =
    {
        "/bin/sh",
        "-c",
        NULL,
        NULL
    };

    g_return_val_if_fail(ctxt != NULL, -EINVAL);
    g_return_val_if_fail(field != NULL, -EINVAL);
    g_return_val_if_fail((field->type == RASTA_FIELD_LIST) ||
                         (field->type == RASTA_FIELD_ENTRYLIST),
                         -EINVAL);

    l_field = RASTA_LIST_DIALOG_FIELD(field);

    utf8_cmd = rasta_exec_symbol_subst(ctxt,
                                       l_field->list_command,
                                       l_field->escape_style);

    if (utf8_cmd == NULL)
        return(-ENOMEM);

    locale_cmd = g_locale_from_utf8(utf8_cmd, -1, NULL, NULL, NULL);
    g_free(utf8_cmd);
    if (locale_cmd == NULL)
        return(-ENOMEM);

    argv[2] = locale_cmd;
    rc = rasta_exec_command_v(&l_pid,
                              NULL, &l_outfd, &l_errfd,
                              RASTA_EXEC_FD_NULL,
                              RASTA_EXEC_FD_PIPE,
                              RASTA_EXEC_FD_PIPE,
                              argv);

    g_free(locale_cmd);

    if (pid != NULL)
        *pid = rc ? -1 : l_pid;
    if (outfd != NULL)
        *outfd = rc ? -1 : l_outfd;
    if (errfd != NULL)
        *errfd = rc ? -1 : l_errfd;

    return(rc);
}  /* rasta_listcommand_run() */
