#pragma once

enum class input_type
{ 
	none = 0,
	key,
	mouse_button,
	touch
};

enum class input_state
{ 
	released	= 0,
	pressed		= 1,
	repeating	= 2
};

#ifdef USE_NATIVE_WIN32

#ifdef APP_COMPILER_MSVC
#pragma warning( push )
#pragma warning( disable : 4311 ) // pointer truncation from 'LPSTR' to 'uint32_t'
#endif

#define NOMINMAX
#include <Windows.h>

enum class mouse_button
{
	left		= VK_LBUTTON,
	right		= VK_RBUTTON,
	middle		= VK_MBUTTON,
	x1			= VK_XBUTTON1,
	x2			= VK_XBUTTON2,
	back		= x1,
	forward		= x2
};

enum class key_code
{
	space			= VK_SPACE,
	apostrophe		= VK_OEM_7, /* ' */
	comma			= VK_OEM_COMMA, /* , */
	minus			= VK_OEM_MINUS, /* - */
	plus			= VK_OEM_PLUS, /* + same as = */
	period			= VK_OEM_PERIOD, /* . */
	slash			= VK_OEM_2, /* / or ? */

	D0				= '0',
	D1				= '1',
	D2				= '2',
	D3				= '3',
	D4				= '4',
	D5				= '5',
	D6				= '6',
	D7				= '7',
	D8				= '8',
	D9				= '9',

	semicolon		= VK_OEM_1, /* ; */
	equal			= VK_OEM_PLUS, /* = same as + */

	A				= 'A',
	B				= 'B',
	C				= 'C',
	D				= 'D',
	E				= 'E',
	F				= 'F',
	G				= 'G',
	H				= 'H',
	I				= 'I',
	J				= 'J',
	K				= 'K',
	L				= 'L',
	M				= 'M',
	N				= 'N',
	O				= 'O',
	P				= 'P',
	Q				= 'Q',
	R				= 'R',
	S				= 'S',
	T				= 'T',
	U				= 'U',
	V				= 'V',
	W				= 'W',
	X				= 'X',
	Y				= 'Y',
	Z				= 'Z',

	left_bracket	= VK_OEM_4,  /* [ */
	backslash		= VK_OEM_5,  /* \ */
	separator		= backslash,
	right_bracket	= VK_OEM_6,  /* ] */
	grave_accent	= VK_OEM_3,  /* ` */

	/* Function keys */
	escape			= VK_ESCAPE,
	enter			= VK_RETURN,
	tab				= VK_TAB,
	backspace		= VK_BACK,
	insert			= VK_INSERT,
	del				= VK_DELETE,
	right			= VK_RIGHT,
	left			= VK_LEFT,
	down			= VK_DOWN,
	up				= VK_UP,
	page_up			= VK_PRIOR,
	page_down		= VK_NEXT,
	home			= VK_HOME,
	end				= VK_END,
	capslock		= VK_CAPITAL,
	scrolllock		= VK_SCROLL,
	numlock			= VK_NUMLOCK,
	print_screen	= VK_SNAPSHOT,
	pause			= VK_PAUSE,
	F1				= VK_F1,
	F2				= VK_F2,
	F3				= VK_F3,
	F4				= VK_F4,
	F5				= VK_F5,
	F6				= VK_F6,
	F7				= VK_F7,
	F8				= VK_F8,
	F9				= VK_F9,
	F10				= VK_F10,
	F11				= VK_F11,
	F12				= VK_F12,
	F13				= VK_F13,
	F14				= VK_F14,
	F15				= VK_F15,
	F16				= VK_F16,
	F17				= VK_F17,
	F18				= VK_F18,
	F19				= VK_F19,
	F20				= VK_F20,
	F21				= VK_F21,
	F22				= VK_F22,
	F23				= VK_F23,
	F24				= VK_F24,

	/* Keypad */
	kp0				= VK_NUMPAD0,
	kp1				= VK_NUMPAD1,
	kp2				= VK_NUMPAD2,
	kp3				= VK_NUMPAD3,
	kp4				= VK_NUMPAD4,
	kp5				= VK_NUMPAD5,
	kp6				= VK_NUMPAD6,
	kp7				= VK_NUMPAD7,
	kp8				= VK_NUMPAD8,
	kp9				= VK_NUMPAD9,
	kp_decimal		= VK_DECIMAL,
	kp_divide		= VK_DIVIDE,
	kp_multiply		= VK_MULTIPLY,
	kp_subtract		= VK_SUBTRACT,
	kp_add			= VK_ADD,
	kp_enter		= VK_RETURN, /* same as enter */
	kp_del			= VK_DELETE, /* same as del */

	left_shift		= VK_LSHIFT,
	left_ctrl		= VK_LCONTROL,
	left_alt		= VK_LMENU,
	left_super		= VK_LWIN,
	right_shift		= VK_RSHIFT,
	right_ctrl		= VK_RCONTROL,
	right_alt		= VK_RMENU,
	right_super		= VK_RWIN,
	apps			= VK_APPS,
	menu			= apps
};

enum class cursor_type
{
	arrow						= (uint32_t)IDC_ARROW,
	default						= arrow,
	cross						= (uint32_t)IDC_CROSS,
	hand						= (uint32_t)IDC_HAND,
	help						= (uint32_t)IDC_HELP,
	ibeam						= (uint32_t)IDC_IBEAM,
	text						= ibeam,
	no							= (uint32_t)IDC_NO,
	size_all					= (uint32_t)IDC_SIZEALL,
	size_nesw					= (uint32_t)IDC_SIZENESW,
	arrow_northeast_southwest	= size_nesw,
	size_ns						= (uint32_t)IDC_SIZENS,
	arrow_north_south			= size_ns,
	size_we						= (uint32_t)IDC_SIZEWE,
	arrow_west_east				= size_we,
	arrow_northwest_southeast	= size_nesw,
	up_arrow					= (uint32_t)IDC_UPARROW,
	hourglass					= (uint32_t)IDC_WAIT,
	wait						= hourglass
};

#ifdef APP_COMPILER_MSVC
#pragma warning( pop )
#endif

#else

enum class mouse_button
{ 
	left	= 0,
	right	= 1,
	middle	= 2,
	back	= 3,
	forward	= 4,
	x1		= 5,
	x2		= 6,
};

enum class key_code
{
	// From glfw3.h
	space               = 32,
	apostrophe          = 39, /* ' */
	comma               = 44, /* , */
	minus               = 45, /* - */
	period              = 46, /* . */
	slash               = 47, /* / */

	D0                  = 48, /* 0 */
	D1                  = 49, /* 1 */
	D2                  = 50, /* 2 */
	D3                  = 51, /* 3 */
	D4                  = 52, /* 4 */
	D5                  = 53, /* 5 */
	D6                  = 54, /* 6 */
	D7                  = 55, /* 7 */
	D8                  = 56, /* 8 */
	D9                  = 57, /* 9 */

	semicolon           = 59, /* ; */
	equal               = 61, /* = */

	A                   = 65,
	B                   = 66,
	C                   = 67,
	D                   = 68,
	E                   = 69,
	F                   = 70,
	G                   = 71,
	H                   = 72,
	I                   = 73,
	J                   = 74,
	K                   = 75,
	L                   = 76,
	M                   = 77,
	N                   = 78,
	O                   = 79,
	P                   = 80,
	Q                   = 81,
	R                   = 82,
	S                   = 83,
	T                   = 84,
	U                   = 85,
	V                   = 86,
	W                   = 87,
	X                   = 88,
	Y                   = 89,
	Z                   = 90,

	left_bracket        = 91,  /* [ */
	backslash           = 92,  /* \ */
	right_bracket       = 93,  /* ] */
	separator			= backslash,
	grave_accent        = 96,  /* ` */

	/* Function keys */
	escape              = 256,
	enter               = 257,
	tab                 = 258,
	Backspace           = 259,
	insert              = 260,
	del              	= 261,
	right               = 262,
	left                = 263,
	down                = 264,
	up                  = 265,
	page_up             = 266,
	page_down           = 267,
	home                = 268,
	end                 = 269,
	capslock            = 280,
	scrolllock          = 281,
	numlock             = 282,
	print_screen        = 283,
	pause               = 284,
	F1                  = 290,
	F2                  = 291,
	F3                  = 292,
	F4                  = 293,
	F5                  = 294,
	F6                  = 295,
	F7                  = 296,
	F8                  = 297,
	F9                  = 298,
	F10                 = 299,
	F11                 = 300,
	F12                 = 301,
	F13                 = 302,
	F14                 = 303,
	F15                 = 304,
	F16                 = 305,
	F17                 = 306,
	F18                 = 307,
	F19                 = 308,
	F20                 = 309,
	F21                 = 310,
	F22                 = 311,
	F23                 = 312,
	F24                 = 313,
	F25                 = 314,

	/* Keypad */
	kp0					= 320,
	kp1					= 321,
	kp2					= 322,
	kp3					= 323,
	kp4					= 324,
	kp5					= 325,
	kp6					= 326,
	kp7					= 327,
	kp8					= 328,
	kp9					= 329,
	kp_decimal          = 330,
	kp_divide           = 331,
	kp_multiply         = 332,
	kp_subtract         = 333,
	kp_add              = 334,
	kp_enter            = 335,
	kp_equal            = 336,

	left_shift          = 340,
	left_ctrl			= 341,
	left_alt            = 342,
	left_super          = 343,
	right_shift         = 344,
	right_ctrl			= 345,
	right_alt			= 346,
	right_super         = 347,
	apps				= 348,
	menu				= apps
};

enum class cursor_type
{
	arrow = 0,
	cross,
	hand,
	help,
	ibeam, text = ibeam,
	no,
	size_all,
	size_nesw,
	arrow_northeast_southwest = size_nesw,
	size_ns,
	arrow_north_south = size_ns,
	size_we,
	arrow_west_east = size_we,
	arrow_northwest_southeast,
	up_arrow,
	hourglass, wait = hourglass
};

#endif