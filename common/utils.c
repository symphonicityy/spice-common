/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010, 2011, 2018 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>

#include "utils.h"

/* These 2 functions come from
 * libvirt-glib/libvirt-gconfig/libvirt-gconfig-helpers.c
 * Copyright (C) 2010, 2011 Red Hat, Inc.
 * LGPLv2.1+ licensed */

const char *
spice_genum_get_nick(GType enum_type, gint value)
{
    GEnumClass *enum_class;
    GEnumValue *enum_value;

    g_return_val_if_fail(G_TYPE_IS_ENUM(enum_type), NULL);

    enum_class = g_type_class_ref(enum_type);
    enum_value = g_enum_get_value(enum_class, value);
    g_type_class_unref(enum_class);

    if (enum_value != NULL) {
        return enum_value->value_nick;
    }

    g_return_val_if_reached(NULL);
}

int
spice_genum_get_value(GType enum_type, const char *nick,
                      gint default_value)
{
    GEnumClass *enum_class;
    GEnumValue *enum_value;

    g_return_val_if_fail(G_TYPE_IS_ENUM(enum_type), default_value);
    g_return_val_if_fail(nick != NULL, default_value);

    enum_class = g_type_class_ref(enum_type);
    enum_value = g_enum_get_value_by_nick(enum_class, nick);
    g_type_class_unref(enum_class);

    if (enum_value != NULL) {
        return enum_value->value;
    }

    g_return_val_if_reached(default_value);
}
