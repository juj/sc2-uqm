/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef PORT_H_INCL
#define PORT_H_INCL

#ifdef _MSC_VER
// MSVC
#	define inline  __inline

#else
// GCC, etc.
#	define inline __inline__

#endif

#ifndef __bool_true_false_are_defined
#	undef bool
#	undef false
#	undef true
typedef unsigned char bool;
#define true 1
#define false 0
#define __bool_true_false_are_defined
#endif  /* __bool_true_false_are_defined */

#endif /* PORT_H_INCL */
