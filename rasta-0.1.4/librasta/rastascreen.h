/*
 * rastascreen.h
 *
 * Private header file for the Rasta screen object.
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


#ifndef _RASTA_SCREEN_H
#define _RASTA_SCREEN_H



/*
 * Typedefs
 */
typedef struct  _RastaAnyScreen         RastaAnyScreen;
typedef struct  _RastaMenuScreen        RastaMenuScreen;
typedef struct  _RastaDialogScreen      RastaDialogScreen;
typedef struct  _RastaHiddenScreen      RastaHiddenScreen;
typedef struct  _RastaActionScreen      RastaActionScreen;



/*
 * Structures
 */

struct _RastaAnyScreen
{
    RastaScreenType type;
    gchar *title;
    gchar *help;
};

struct _RastaMenuScreen
{
    RastaScreenType type;
    gchar *title;
    gchar *help;
    GList *menu_items;
};

struct _RastaDialogScreen
{
    RastaScreenType type;
    gchar *title;
    gchar *help;
    GList *fields;
    gchar *init_command;
    RastaEscapeStyleType escape_style;
    gchar *encoding;
};

struct _RastaHiddenScreen
{
    RastaScreenType type;
    gchar *title;
    gchar *help;
    gchar *init_command;
    RastaEscapeStyleType escape_style;
    gchar *encoding;
};

struct _RastaActionScreen
{
    RastaScreenType type;
    gchar *title;
    gchar *help;
    gchar *command;
    RastaTTYType tty_type;
    RastaEscapeStyleType escape_style;
    gchar *encoding;
    gboolean confirm;
};



/*
 * Unions
 */
union _RastaScreen
{
    RastaScreenType type;      /* Must not be changed; first element */
    RastaAnyScreen any;
    RastaMenuScreen menu;
    RastaDialogScreen dialog;
    RastaHiddenScreen hidden;
    RastaActionScreen action;
};



/*
 * Functions
 */
void rasta_screen_prepare(RastaContext *ctxt);

#endif /* _RASTA_SCREEN_H */
