//
//
// Ur-Quan Masters Voice-over / Subtitle Synchronization tool
// Copyright 2003 by Geoffrey Hausheer
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//
// contact tcpaiqgvnmbf28f001@sneakemail.com for bugs/issues
//
typedef struct {
	unsigned long samp_freq;
	unsigned long bits_per_sample;
	unsigned long channels;
	char *data;
	unsigned long data_len;
} AUDIOHDR;
