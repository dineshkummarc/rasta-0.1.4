/**
 * rasta.h
 *
 * Common definitions for rasta
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

/*
 * NOTE -
 *
 * Some of the enum values, function names, and function prototypes
 * here do not match those in the whitepaper.  These are more
 * authoritative, as I've thought about it while doing this public
 * header.  I will update the whitepaper accordingly.
 */

#ifndef _RASTA_H
#define _RASTA_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * Defines
 */
#define         RASTA_SYSTEM_FILE       "system.rasta"
#define         RASTA_DTD               "rasta.dtd"
#define         RASTAMODIFY_DTD         "rastamodify.dtd"
#define         RASTASTATE_DTD          "rastastate.dtd"
#define         RASTA_CONTEXT(x)        ((RastaContext *)(x))
#define         RASTA_SCREEN(x)         ((RastaScreen *)(x))
#define         RASTA_ANY_SCREEN(x)     ((RastaAnyScreen *)(x))
#define         RASTA_DIALOG_SCREEN(x)  ((RastaDialogScreen *)(x))
#define         RASTA_HIDDEN_SCREEN(x)  ((RastaHiddenScreen *)(x))
#define         RASTA_MENU_SCREEN(x)    ((RastaMenuScreen *)(x))
#define         RASTA_ACTION_SCREEN(x)  ((RastaActionScreen *)(x))
#define         RASTA_MENU_ITEM(x)      ((RastaMenuItem *)(x))
#define         RASTA_DIALOG_FIELD(x)   ((RastaDialogField *)(x))
#define         RASTA_RING_VALUE(x)      ((RastaRingValue *)(x))



/*
 * Typedefs
 */
typedef struct  _RastaContext           RastaContext;
typedef union   _RastaScreen            RastaScreen;
typedef struct  _RastaMenuItem          RastaMenuItem;
typedef union   _RastaDialogField       RastaDialogField;
typedef struct  _RastaRingValue         RastaRingValue;

typedef struct _REnumeration		REnumeration;
typedef gpointer (*REnumerationFunc)	(gpointer context);



/*
 * Enums
 */
typedef enum
{
    RASTA_SCREEN_NONE,
    RASTA_SCREEN_HIDDEN,
    RASTA_SCREEN_MENU,
    RASTA_SCREEN_DIALOG,
    RASTA_SCREEN_ACTION
} RastaScreenType;

typedef enum
{
    RASTA_FIELD_NONE,           /* Error */
    RASTA_FIELD_ENTRY,          /* Text entry */
    RASTA_FIELD_FILE,           /* Filename entry */
    RASTA_FIELD_READONLY,       /* Readonly value */
    RASTA_FIELD_DESCRIPTION,    /* Description line */
    RASTA_FIELD_RING,           /* Ring of preset values */
    RASTA_FIELD_LIST,           /* Popup list of values */
#ifndef ENABLE_DEPRECATED
    RASTA_FIELD_ENTRYLIST       /* Popup list with entry */
#else
    RASTA_FIELD_ENTRYLIST,      /* Popup list with entry */
    RASTA_FIELD_IPADDR,         /* IP Address (NIC SPECIFIC) */
    RASTA_FIELD_VOLUME          /* Volume level (NIC SPECIFIC) */
#endif  /* ENABLE_DEPRECATED */
} RastaDialogFieldType;

typedef enum 
{
    RASTA_TTY_NO,               /* Action requires no TTY */
    RASTA_TTY_YES,              /* Action has TTY output */
    RASTA_TTY_SELF              /* Action should handle TTY itself */
} RastaTTYType;

typedef enum 
{
    RASTA_ESCAPE_STYLE_NONE,    /* No quote */
    RASTA_ESCAPE_STYLE_SINGLE,  /* Quote for '' enclosure */
    RASTA_ESCAPE_STYLE_DOUBLE,  /* Quote for "" enclosure */
} RastaEscapeStyleType;

typedef enum
{
    RASTA_EXEC_FD_INHERIT,      /* Child inherits fd from parent */
    RASTA_EXEC_FD_PIPE,         /* Pipe created for fd from child */
    RASTA_EXEC_FD_NULL,         /* Child fd redirected to /dev/null */
} RastaExecFDProtocol;



/*
 * Functions
 */


/* Context management */
RastaContext *rasta_context_init(const gchar *filename,
                                 const gchar *fastpath);
RastaContext *rasta_context_init_push(const gchar *filename,
                                      const gchar *fastpath);
gint rasta_context_parse_chunk(RastaContext *ctxt,
                               gpointer data_chunk,
                               gint size);  /* 0 == EOF */
gint rasta_context_load_state(RastaContext *ctxt,
                              const gchar *filename);
gint rasta_context_load_state_push(RastaContext *ctxt,
                                   const gchar *filename);
gint rasta_context_parse_state_chunk(RastaContext *ctxt,
                                     gpointer data_chunk,
                                     gint size);  /* 0 == EOF */
gint rasta_context_save_state(RastaContext *ctxt,
                              const gchar *filename);
gint rasta_context_save_state_memory(RastaContext *ctxt,
                                     gchar **state_text,
                                     gint *state_size);
void rasta_context_destroy(RastaContext *ctxt);
                               
/* Symbol table */
void rasta_symbol_put(RastaContext *ctxt,
                      const gchar *sym_name,
                      const gchar *sym_value);
gchar *rasta_symbol_lookup(RastaContext *ctxt,
                           const gchar *sym_name);

/* Generic screen functions */
RastaScreen *rasta_context_get_screen(RastaContext *ctxt);
gboolean rasta_context_is_initial_screen(RastaContext *ctxt);
RastaScreenType rasta_screen_get_type(RastaScreen *screen);
gchar *rasta_screen_get_title(RastaScreen *screen);
void rasta_screen_previous(RastaContext *ctxt);

/* Menu screen functions */
REnumeration *rasta_menu_screen_enumerate_items(RastaContext *ctxt,
                                                RastaScreen *screen);
gchar *rasta_menu_item_get_text(RastaMenuItem *menu_item);
gchar *rasta_menu_item_get_id(RastaMenuItem *menu_item);
gchar *rasta_menu_item_get_help(RastaMenuItem *menu_item);
void rasta_menu_screen_next(RastaContext *ctxt, const gchar *next_id);

/* Dialog screen functions */
REnumeration *rasta_dialog_screen_enumerate_fields(RastaContext *ctxt,
                                                   RastaScreen *screen);
RastaDialogFieldType rasta_dialog_field_get_type(RastaDialogField *field);
gchar *rasta_dialog_field_get_text(RastaDialogField *field);
gchar *rasta_dialog_field_get_symbol_name(RastaDialogField *field);
gboolean rasta_dialog_field_is_required(RastaDialogField *field);
gboolean rasta_entry_dialog_field_is_hidden(RastaDialogField *field);
gboolean rasta_entry_dialog_field_is_numeric(RastaDialogField *field);
gchar *rasta_entry_dialog_field_get_format(RastaDialogField *field);
gint rasta_verify_number_format(const gchar *format,
                                const gchar *test_val);
guint rasta_entry_dialog_field_get_length(RastaDialogField *field);
gchar *rasta_dialog_field_get_help(RastaDialogField *field);
void rasta_dialog_screen_next(RastaContext *ctxt);

void rasta_hidden_screen_next(RastaContext *ctxt);

gboolean rasta_initcommand_is_required(RastaContext *ctxt,
                                       RastaScreen *screen);
gchar *rasta_initcommand_get_encoding(RastaScreen *screen);
gint rasta_initcommand_run(RastaContext *ctxt,
                           RastaScreen *screen,
                           pid_t *pid,
                           gint *outfd,
                           gint *errfd);
void rasta_initcommand_complete(RastaContext *ctxt,
                                RastaScreen *screen,
                                gchar *output_str);
void rasta_initcommand_failed(RastaContext *ctxt,
                              RastaScreen *screen);

REnumeration *rasta_dialog_field_enumerate_ring(RastaDialogField *field);
gchar *rasta_ring_value_get_text(RastaRingValue *value);
gchar *rasta_ring_value_get_value(RastaRingValue *value);

gboolean rasta_listcommand_is_single_column(RastaDialogField *field);
gchar *rasta_listcommand_get_encoding(RastaDialogField *field);
gboolean rasta_listcommand_allow_multiple(RastaDialogField *field);
gint rasta_listcommand_run(RastaContext *ctxt,
                           RastaDialogField *field,
                           pid_t *pid,
                           gint *outfd,
                           gint *errfd);

/* Action screen functions */
gchar *rasta_action_screen_get_command(RastaContext *ctxt,
                                       RastaScreen *screen);
RastaTTYType rasta_action_screen_get_tty_type(RastaScreen *screen);
gchar *rasta_action_screen_get_encoding(RastaScreen *screen);
gboolean rasta_action_screen_needs_confirm(RastaScreen *screen);

/* Execution functions */
gint rasta_exec_command_v(pid_t *pid,
                          gint *infd,
                          gint *outfd,
                          gint *errfd,
                          RastaExecFDProtocol in_prot,
                          RastaExecFDProtocol out_prot,
                          RastaExecFDProtocol err_prot,
                          gchar * args[]);
gint rasta_exec_command_l(pid_t *pid,
                          gint *infd,
                          gint *outfd,
                          gint *errfd,
                          RastaExecFDProtocol in_prot,
                          RastaExecFDProtocol out_prot,
                          RastaExecFDProtocol err_prot,
                          const gchar *command,
                          ...);

/* Enumeration functions */
REnumeration* r_enumeration_new(gpointer context,
                                REnumerationFunc has_more_func,
                                REnumerationFunc get_next_func,
                                GDestroyNotify notify_func);
REnumeration* r_enumeration_new_from_list(GList *init_list);
gboolean r_enumeration_has_more(REnumeration *enumeration);
gpointer r_enumeration_get_next(REnumeration *enumeration);
void r_enumeration_free(REnumeration *enumeration);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _RASTA_H */

