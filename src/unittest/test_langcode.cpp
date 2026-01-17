// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "util/langcode.h"
#include "catch.h"

TEST_CASE("test langcode")
{
	SECTION("test language list")
	{
		CHECK(expand_language_list(L"de_DE@euro.UTF-8:fr") == L"de_DE@euro:de_DE:de:fr");
		CHECK(expand_language_list(L"zh_HK:yue_HK:zh_TW") == L"zh_HK:yue_HK:yue:zh_TW:zh");
		CHECK(expand_language_list(L"de_DE:fr:de_CH:en:de:de_AT") == L"de_DE:fr:de_CH:en:de:de_AT");
		CHECK(expand_language_list(L".UTF-8:de:.ISO-8859-1:fr:.GB2312") == L"de:fr");
	}
}
