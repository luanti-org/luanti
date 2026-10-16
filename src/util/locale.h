// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

/* This is a wrapper around ICU's locale class because:
 * We cannot use ICU's C++ API (which is horrible anyway) due to a lack of ABI compatibility
 * ICU's C API is unpleasant to work with
 */

#include <string>

class Locale
{
public:
	// ICU expects the string to be null-terminated so we cannot use string_view here
	Locale(const std::string &id);

	// Gets the full name
	const std::string &getName() const
	{
		return name;
	}

	// Gets the display name
	std::string getDisplayName(const Locale &in_locale) const;
	std::string getDisplayName() const
	{
		return getDisplayName(*this);
	}
private:
	std::string name;
};
