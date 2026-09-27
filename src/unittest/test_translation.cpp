// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "translation.h"

class TestTranslation : public TestBase
{
public:
	TestTranslation() {
		TestManager::registerTestModule(this);
	}

	const char *getName() { return "TestTranslation"; }

	void runTests(IGameDef *gamedef);

	void testGetFileLanguage();
	void testGetScriptLanguage();
};

static TestTranslation g_test_instance;

void TestTranslation::runTests(IGameDef *gamedef)
{
	TEST(testGetFileLanguage);
	TEST(testGetScriptLanguage);
}

void TestTranslation::testGetFileLanguage()
{
	UASSERTEQ(std::string, std::string(Translations::getFileLanguage("mymod.zh_Hans.po")), "zh_Hans");
	UASSERTEQ(std::string, std::string(Translations::getFileLanguage("mymod.zh_CN.tr")), "zh_CN");
	UASSERTEQ(std::string, std::string(Translations::getFileLanguage("mymod.fr.mo")), "fr");
	UASSERTEQ(std::string, std::string(Translations::getFileLanguage("mymod.po")), "");
}

void TestTranslation::testGetScriptLanguage()
{
	// Region-based codes imply a script-based code they can fall back to
	UASSERTEQ(std::string, Translations::getScriptLanguage("zh_CN"), "zh_Hans");
	UASSERTEQ(std::string, Translations::getScriptLanguage("zh_TW"), "zh_Hant");
	// Everything else passes through unchanged
	UASSERTEQ(std::string, Translations::getScriptLanguage("zh_Hans"), "zh_Hans");
	UASSERTEQ(std::string, Translations::getScriptLanguage("de"), "de");
	UASSERTEQ(std::string, Translations::getScriptLanguage("pt_BR"), "pt_BR");
	UASSERTEQ(std::string, Translations::getScriptLanguage(""), "");
}
