/**
 * GreenPois0n Absinthe - debug.h
 * Copyright (C) 2010 Chronic-Dev Team
 * Copyright (C) 2010 Joshua Hill
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef DEBUG_H_
#define DEBUG_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>

#ifdef _DEBUG
#define info(...) fprintf(stdout, __VA_ARGS__)
#define error(...) fprintf(stderr, __VA_ARGS__)
#define debug(...) fprintf(stderr, __VA_ARGS__)
#else
#define info(...) printf(__VA_ARGS__)
#define error(...) fprintf(stderr, __VA_ARGS__)
#define debug(...)
#endif

#endif /* DEBUG_H_ */
