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

#ifndef H_SPICE_COMMON_UTILS
#define H_SPICE_COMMON_UTILS

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

const char *spice_genum_get_nick(GType enum_type, gint value);
int spice_genum_get_value(GType enum_type, const char *nick,
                          gint default_value);

G_END_DECLS

#endif //H_SPICE_COMMON_UTILS
