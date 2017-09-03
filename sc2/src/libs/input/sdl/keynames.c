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

#include "port.h"
#include SDL_INCLUDE(SDL.h)
#include <string.h>
#include "keynames.h"

/* This code is adapted from the code in SDL_keysym.h.  Though this
 * would almost certainly be fast if we were to use a direct char *
 * array, this technique permits us to be independent of the actual
 * character encoding to keysyms. */

/* These names are case-insensitive when compared, but we format 
 * them to look pretty when output */

/* This version of Virtual Controller does not support SDL_SCANCODE_WORLD_*
 * keysyms or the Num/Caps/ScrollLock keys.  SDL treats locking keys
 * specially, and we cannot treat them as normal keys.  Pain, 
 * tragedy. */

typedef struct vcontrol_keyname {
	const char *name;
	int code;
} keyname;

static keyname keynames[] = {
	{"Backspace", SDL_SCANCODE_BACKSPACE},
	{"Tab", SDL_SCANCODE_TAB},
	{"Clear", SDL_SCANCODE_CLEAR},
	{"Return", SDL_SCANCODE_RETURN},
	{"Pause", SDL_SCANCODE_PAUSE},
	{"Escape", SDL_SCANCODE_ESCAPE},
	{"Space", SDL_SCANCODE_SPACE},
	{"!", SDL_SCANCODE_KP_EXCLAM},
	{"\"", SDL_SCANCODE_BACKSLASH},
	{"Hash", SDL_SCANCODE_NONUSHASH},
//	{"$", SDL_SCANCODE_DOLLAR},
//	{"&", SDL_SCANCODE_AMPERSAND},
	{"'", SDL_SCANCODE_APOSTROPHE},
//	{"(", SDL_SCANCODE_LEFTPAREN},
//	{")", SDL_SCANCODE_RIGHTPAREN},
//	{"*", SDL_SCANCODE_ASTERISK},
//	{"+", SDL_SCANCODE_PLUS},
	{",", SDL_SCANCODE_COMMA},
	{"-", SDL_SCANCODE_MINUS},
	{".", SDL_SCANCODE_PERIOD},
	{"/", SDL_SCANCODE_SLASH},
	{"0", SDL_SCANCODE_0},
	{"1", SDL_SCANCODE_1},
	{"2", SDL_SCANCODE_2},
	{"3", SDL_SCANCODE_3},
	{"4", SDL_SCANCODE_4},
	{"5", SDL_SCANCODE_5},
	{"6", SDL_SCANCODE_6},
	{"7", SDL_SCANCODE_7},
	{"8", SDL_SCANCODE_8},
	{"9", SDL_SCANCODE_9},
//	{":", SDL_SCANCODE_COLON},
	{";", SDL_SCANCODE_SEMICOLON},
	{"<", SDL_SCANCODE_NONUSBACKSLASH},
	{"=", SDL_SCANCODE_EQUALS},
//	{">", SDL_SCANCODE_GREATER},
//	{"?", SDL_SCANCODE_QUESTION},
//	{"@", SDL_SCANCODE_AT},
	{"[", SDL_SCANCODE_LEFTBRACKET},
	{"\\", SDL_SCANCODE_BACKSLASH},
	{"]", SDL_SCANCODE_RIGHTBRACKET},
//	{"^", SDL_SCANCODE_CARET},
//	{"_", SDL_SCANCODE_UNDERSCORE},
	{"`", SDL_SCANCODE_GRAVE},
	{"a", SDL_SCANCODE_A},
	{"b", SDL_SCANCODE_B},
	{"c", SDL_SCANCODE_C},
	{"d", SDL_SCANCODE_D},
	{"e", SDL_SCANCODE_E},
	{"f", SDL_SCANCODE_F},
	{"g", SDL_SCANCODE_G},
	{"h", SDL_SCANCODE_H},
	{"i", SDL_SCANCODE_I},
	{"j", SDL_SCANCODE_J},
	{"k", SDL_SCANCODE_K},
	{"l", SDL_SCANCODE_L},
	{"m", SDL_SCANCODE_M},
	{"n", SDL_SCANCODE_N},
	{"o", SDL_SCANCODE_O},
	{"p", SDL_SCANCODE_P},
	{"q", SDL_SCANCODE_Q},
	{"r", SDL_SCANCODE_R},
	{"s", SDL_SCANCODE_S},
	{"t", SDL_SCANCODE_T},
	{"u", SDL_SCANCODE_U},
	{"v", SDL_SCANCODE_V},
	{"w", SDL_SCANCODE_W},
	{"x", SDL_SCANCODE_X},
	{"y", SDL_SCANCODE_Y},
	{"z", SDL_SCANCODE_Z},
	{"Delete", SDL_SCANCODE_DELETE},
	{"Keypad-0", SDL_SCANCODE_KP_0},
	{"Keypad-1", SDL_SCANCODE_KP_1},
	{"Keypad-2", SDL_SCANCODE_KP_2},
	{"Keypad-3", SDL_SCANCODE_KP_3},
	{"Keypad-4", SDL_SCANCODE_KP_4},
	{"Keypad-5", SDL_SCANCODE_KP_5},
	{"Keypad-6", SDL_SCANCODE_KP_6},
	{"Keypad-7", SDL_SCANCODE_KP_7},
	{"Keypad-8", SDL_SCANCODE_KP_8},
	{"Keypad-9", SDL_SCANCODE_KP_9},
	{"Keypad-.", SDL_SCANCODE_KP_PERIOD},
	{"Keypad-/", SDL_SCANCODE_KP_DIVIDE},
	{"Keypad-*", SDL_SCANCODE_KP_MULTIPLY},
	{"Keypad--", SDL_SCANCODE_KP_MINUS},
	{"Keypad-+", SDL_SCANCODE_KP_PLUS},
	{"Keypad-Enter", SDL_SCANCODE_KP_ENTER},
	{"Keypad-=", SDL_SCANCODE_KP_EQUALS},
	{"Up", SDL_SCANCODE_UP},
	{"Down", SDL_SCANCODE_DOWN},
	{"Right", SDL_SCANCODE_RIGHT},
	{"Left", SDL_SCANCODE_LEFT},
	{"Insert", SDL_SCANCODE_INSERT},
	{"Home", SDL_SCANCODE_HOME},
	{"End", SDL_SCANCODE_END},
	{"PageUp", SDL_SCANCODE_PAGEUP},
	{"PageDown", SDL_SCANCODE_PAGEDOWN},
	{"F1", SDL_SCANCODE_F1},
	{"F2", SDL_SCANCODE_F2},
	{"F3", SDL_SCANCODE_F3},
	{"F4", SDL_SCANCODE_F4},
	{"F5", SDL_SCANCODE_F5},
	{"F6", SDL_SCANCODE_F6},
	{"F7", SDL_SCANCODE_F7},
	{"F8", SDL_SCANCODE_F8},
	{"F9", SDL_SCANCODE_F9},
	{"F10", SDL_SCANCODE_F10},
	{"F11", SDL_SCANCODE_F11},
	{"F12", SDL_SCANCODE_F12},
	{"F13", SDL_SCANCODE_F13},
	{"F14", SDL_SCANCODE_F14},
	{"F15", SDL_SCANCODE_F15},
	{"RightShift", SDL_SCANCODE_RSHIFT},
	{"LeftShift", SDL_SCANCODE_LSHIFT},
	{"RightControl", SDL_SCANCODE_RCTRL},
	{"LeftControl", SDL_SCANCODE_LCTRL},
	{"RightAlt", SDL_SCANCODE_RALT},
	{"LeftAlt", SDL_SCANCODE_LALT},
	{"RightMeta", SDL_SCANCODE_RGUI},
	{"LeftMeta", SDL_SCANCODE_LGUI},
	{"RightSuper", SDL_SCANCODE_RGUI},
	{"LeftSuper", SDL_SCANCODE_LGUI},
	{"AltGr", SDL_SCANCODE_MODE},
	{"Help", SDL_SCANCODE_HELP},
	{"Print", SDL_SCANCODE_PRINTSCREEN},
	{"SysReq", SDL_SCANCODE_SYSREQ},
	{"Menu", SDL_SCANCODE_MENU},
	{"Power", SDL_SCANCODE_POWER},
	{"Undo", SDL_SCANCODE_UNDO},
#ifdef _WIN32_WCE
	{"App1", SDL_SCANCODE_APP1},
	{"App2", SDL_SCANCODE_APP2},
	{"App3", SDL_SCANCODE_APP3},
	{"App4", SDL_SCANCODE_APP4},
	{"App5", SDL_SCANCODE_APP5},
	{"App6", SDL_SCANCODE_APP6},
#endif

	{"Unknown", 0}};
/* Last element must have code zero */

const char *
VControl_code2name (int code)
{
	int i = 0;
	while (1)
	{
		int test = keynames[i].code;
		if (test == code || !test)
		{
			return keynames[i].name;
		}
		++i;
	}
}

int
VControl_name2code (const char *name)
{
	int i = 0;
	while (1)
	{
		const char *test = keynames[i].name;
		int code = keynames[i].code;
		if (!strcasecmp(test, name) || !code)
		{
			return code;
		}
		++i;
	}
}
