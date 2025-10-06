// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "translation.h"
#include "filesys.h"
#include "content/subgames.h"
#include "catch.h"

#define CONTEXT L"context"
#define TEXTDOMAIN_PO L"translation_po"
#define TEST_PO_NAME "translation_po.de.po"
#define TEST_MO_NAME "translation_mo.de.mo"

static std::string read_translation_file(const std::string &filename)
{
		auto gamespec = findSubgame("devtest");
		REQUIRE(gamespec.isValid());
		auto path = gamespec.gamemods_path + (DIR_DELIM "testtranslations" DIR_DELIM "test_locale" DIR_DELIM) + filename;
		std::string content;
		REQUIRE(fs::ReadFile(path, content));
		return content;
}

TEST_CASE("test translations")
{
	SECTION("Plural-Forms function for translations")
	{
#define REQUIRE_FORM_SIZE(x) {REQUIRE(form); REQUIRE(form.size() == (x));}
		// Basic test cases
		auto form = GettextPluralForm(L"Plural-Forms: nplurals=2; plural=1;");
		REQUIRE_FORM_SIZE(2);
		CHECK(form(0) == 1);

		form = GettextPluralForm(L"");
		REQUIRE(form.size() == 0);
		CHECK(form(0) == 0);

		// Test cases from https://www.gnu.org/software/gettext/manual/html_node/Plural-forms.html
		form = GettextPluralForm(L"Plural-Forms: nplurals=2; plural=n != 1;");
		REQUIRE_FORM_SIZE(2);
		CHECK(form(0) == 1);
		CHECK(form(1) == 0);
		CHECK(form(2) == 1);

		form = GettextPluralForm(L"Plural-Forms: nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : n != 0 ? 1 : 2;");
		REQUIRE_FORM_SIZE(3);
		CHECK(form(0) == 2);
		CHECK(form(1) == 0);
		CHECK(form(102) == 1);
		CHECK(form(111) == 1);

		form = GettextPluralForm(L"Plural-Forms: nplurals=3; "
				"plural=n%10==1 && n%100!=11 ? 0 : "
				"n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2;");
		REQUIRE_FORM_SIZE(3);
		CHECK(form(0) == 2);
		CHECK(form(1) == 0);
		CHECK(form(102) == 1);
		CHECK(form(104) == 1);
		CHECK(form(111) == 2);
		CHECK(form(112) == 2);
		CHECK(form(121) == 0);
		CHECK(form(122) == 1);

		// Edge cases
		form = GettextPluralForm(L"Plural-Forms: nplurals=3; plural= (n-1+1)<=1 ? n||1?0:1 : 1?(!!2):2;");
		REQUIRE_FORM_SIZE(3);
		CHECK(form(0) == 0);
		CHECK(form(1) == 0);
		CHECK(form(2) == 1);

		form = GettextPluralForm(L"Plural-Forms: nplurals=2; plural=4/n;");
		REQUIRE_FORM_SIZE(2);
		CHECK(form(1) == 4);
		CHECK(form(0) == 0);

		form = GettextPluralForm(L"Plural-Forms: nplurals=2; plural=7%n;");
		REQUIRE_FORM_SIZE(2);
		CHECK(form(3) == 1);
		CHECK(form(0) == 0);
#undef REQUIRE_FORM_SIZE
	}

	SECTION("PO file parser")
	{
		Translations translations;
		translations.loadTranslation(TEST_PO_NAME, read_translation_file(TEST_PO_NAME));

		CHECK(translations.size() == 5);
		CHECK(translations.getTranslation(TEXTDOMAIN_PO, L"foo") == L"bar");
		CHECK(translations.getTranslation(TEXTDOMAIN_PO, L"Untranslated") == L"Untranslated");
		CHECK(translations.getTranslation(TEXTDOMAIN_PO, L"Fuzzy") == L"Fuzzy");
		CHECK(translations.getTranslation(TEXTDOMAIN_PO, L"Multi\\line\nstring") == L"Multi\\\"li\\ne\nresult");
		CHECK(translations.getTranslation(TEXTDOMAIN_PO, L"Wrong order") == L"Wrong order");
		CHECK(translations.getPluralTranslation(TEXTDOMAIN_PO, L"Plural form", 1) == L"Singular result");
		CHECK(translations.getPluralTranslation(TEXTDOMAIN_PO, L"Singular form", 0) == L"Plural result");
		CHECK(translations.getPluralTranslation(TEXTDOMAIN_PO, L"Partial translation", 1) == L"Partially translated");
		CHECK(translations.getPluralTranslation(TEXTDOMAIN_PO, L"Partial translations", 2) == L"Partial translations");
		CHECK(translations.getTranslation(CONTEXT, L"With context") == L"Has context");
	}

	SECTION("MO file parser")
	{
		Translations translations;
		translations.loadTranslation(TEST_MO_NAME, read_translation_file(TEST_MO_NAME));

		CHECK(translations.size() == 2);
		CHECK(translations.getTranslation(CONTEXT, L"With context") == L"Has context");
		CHECK(translations.getPluralTranslation(CONTEXT, L"Plural form", 1) == L"Singular result");
		CHECK(translations.getPluralTranslation(CONTEXT, L"Singular form", 0) == L"Plural result");
	}
}
