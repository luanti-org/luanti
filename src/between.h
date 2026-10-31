// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#  define assert_between_inclusive(x, min, max)	\
	assert(x >= min && x <= max);
#  define assert_between_strict(x, min, max)	\
	assert(x > min && x < max);
#  define assert_between_inc_min(x, min, max)	\
	assert(x >= min && x < max);
#  define assert_between_inc_max(x, min, max)	\
	assert(x > min && x <= max);
