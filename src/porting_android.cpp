// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014 celeron55, Perttu Ahola <celeron55@gmail.com>

#ifndef __ANDROID__
#error This file may only be compiled for android!
#endif

#include "util/numeric.h"
#include "porting.h"
#include "porting_android.h"
#include "threading/thread.h"
#include "config.h"
#include "filesys.h"
#include "log.h"
#include "settings.h"

#include <jni.h>
#define SDL_MAIN_HANDLED 1
#include <SDL.h>

#include <sstream>
#include <exception>
#include <cstdlib>

#ifdef GPROF
#include "prof.h"
#endif

extern int main(int argc, char *argv[]);

extern "C" JNIEXPORT void JNICALL
Java_net_minetest_minetest_GameActivity_saveSettings(JNIEnv* env, jobject /* this */) {
	if (!g_settings_path.empty())
		g_settings->updateConfigFile(g_settings_path.c_str());
}

namespace porting {
	// used here:
	void cleanupAndroid();
	std::string getLanguageAndroid();
	bool setSystemPaths(); // used in porting.cpp
}

extern "C" int SDL_Main(int _argc, char *_argv[])
{
	Thread::setName("Main");

	char *argv[] = {strdup(PROJECT_NAME), strdup("--verbose"), nullptr};
	int retval = main(ARRLEN(argv) - 1, argv);
	free(argv[0]);
	free(argv[1]);

	porting::cleanupAndroid();
	infostream << "Shutting down." << std::endl;
	exit(retval);
}

namespace porting {
JNIEnv      *jnienv = nullptr;
jobject      activity;
jclass       activityClass;

void osSpecificInit()
{
	jnienv = (JNIEnv*)SDL_AndroidGetJNIEnv();
	activity = (jobject)SDL_AndroidGetActivity();
	activityClass = jnienv->GetObjectClass(activity);

	// Set default language
	auto lang = getLanguageAndroid();
	unsetenv("LANGUAGE");
	setenv("LANG", lang.c_str(), 1);

#ifdef GPROF
	// in the start-up code
	warningstream << "Initializing GPROF profiler" << std::endl;
	monstartup("libluanti.so");
#endif
}

void cleanupAndroid()
{
#ifdef GPROF
	warningstream << "Shutting down GPROF profiler" << std::endl;
	setenv("CPUPROFILE", (path_user + DIR_DELIM + "gmon.out").c_str(), 1);
	moncleanup();
#endif
}

static std::string readJavaString(jstring j_str)
{
	// Get string as a UTF-8 C string
	const char *c_str = jnienv->GetStringUTFChars(j_str, nullptr);
	// Save it
	std::string str(c_str);
	// And free the C string
	jnienv->ReleaseStringUTFChars(j_str, c_str);
	return str;
}

bool setSystemPaths()
{
	// Set user and share paths
	{
		jmethodID getUserDataPath = jnienv->GetMethodID(activityClass,
				"getUserDataPath", "()Ljava/lang/String;");
		FATAL_ERROR_IF(getUserDataPath==nullptr,
				"porting::initializePathsAndroid unable to find Java getUserDataPath method");
		jobject result = jnienv->CallObjectMethod(activity, getUserDataPath);
		std::string str = readJavaString((jstring) result);
		path_user = str;
		path_share = str;
	}

	// Set cache path
	{
		jmethodID getCachePath = jnienv->GetMethodID(activityClass,
				"getCachePath", "()Ljava/lang/String;");
		FATAL_ERROR_IF(getCachePath==nullptr,
				"porting::initializePathsAndroid unable to find Java getCachePath method");
		jobject result = jnienv->CallObjectMethod(activity, getCachePath);
		path_cache = readJavaString((jstring) result);
	}

	return true;
}

void showTextInputDialog(const std::string &hint, const std::string &current, int editType)
{
	jmethodID showdialog = jnienv->GetMethodID(activityClass, "showTextInputDialog",
			"(Ljava/lang/String;Ljava/lang/String;I)V");

	FATAL_ERROR_IF(showdialog == nullptr,
			"porting::showTextInputDialog unable to find Java showTextInputDialog method");

	jstring jhint         = jnienv->NewStringUTF(hint.c_str());
	jstring jcurrent      = jnienv->NewStringUTF(current.c_str());
	jint    jeditType     = editType;

	jnienv->CallVoidMethod(activity, showdialog,
			jhint, jcurrent, jeditType);
}

void showComboBoxDialog(const std::string *optionList, s32 listSize, s32 selectedIdx)
{
	jmethodID showdialog = jnienv->GetMethodID(activityClass, "showSelectionInputDialog",
			"([Ljava/lang/String;I)V");

	FATAL_ERROR_IF(showdialog == nullptr,
			"porting::showComboBoxDialog unable to find Java showSelectionInputDialog method");

	jclass       jStringClass = jnienv->FindClass("java/lang/String");
	jobjectArray jOptionList  = jnienv->NewObjectArray(listSize, jStringClass, NULL);
	jint         jselectedIdx = selectedIdx;

	for (s32 i = 0; i < listSize; i ++) {
		jnienv->SetObjectArrayElement(jOptionList, i,
				jnienv->NewStringUTF(optionList[i].c_str()));
	}

	jnienv->CallVoidMethod(activity, showdialog, jOptionList,
			jselectedIdx);
}

void openURIAndroid(const char *url)
{
	jmethodID url_open = jnienv->GetMethodID(activityClass, "openURI",
		"(Ljava/lang/String;)V");

	FATAL_ERROR_IF(url_open == nullptr,
		"porting::openURIAndroid unable to find Java openURI method");

	jstring jurl = jnienv->NewStringUTF(url);
	jnienv->CallVoidMethod(activity, url_open, jurl);
}

void shareFileAndroid(const std::string &path)
{
	jmethodID url_open = jnienv->GetMethodID(activityClass, "shareFile",
			"(Ljava/lang/String;)V");

	FATAL_ERROR_IF(url_open == nullptr,
			"porting::shareFileAndroid unable to find Java shareFile method");

	jstring jurl = jnienv->NewStringUTF(path.c_str());
	jnienv->CallVoidMethod(activity, url_open, jurl);
}

void setPlayingNowNotification(bool show)
{
	jmethodID play_notification = jnienv->GetMethodID(activityClass,
			"setPlayingNowNotification", "(Z)V");

	FATAL_ERROR_IF(play_notification == nullptr,
			"porting::setPlayingNowNotification unable to find Java setPlayingNowNotification method");

	jboolean jshow = show;
	jnienv->CallVoidMethod(activity, play_notification, jshow);
}

AndroidDialogType getLastInputDialogType()
{
	jmethodID lastdialogtype = jnienv->GetMethodID(activityClass,
			"getLastDialogType", "()I");

	FATAL_ERROR_IF(lastdialogtype == nullptr,
			"porting::getLastInputDialogType unable to find Java getLastDialogType method");

	int dialogType = jnienv->CallIntMethod(activity, lastdialogtype);
	return static_cast<AndroidDialogType>(dialogType);
}

AndroidDialogState getInputDialogState()
{
	jmethodID inputdialogstate = jnienv->GetMethodID(activityClass,
			"getInputDialogState", "()I");

	FATAL_ERROR_IF(inputdialogstate == nullptr,
			"porting::getInputDialogState unable to find Java getInputDialogState method");

	int dialogState = jnienv->CallIntMethod(activity, inputdialogstate);
	return static_cast<AndroidDialogState>(dialogState);
}

std::string getInputDialogMessage()
{
	jmethodID dialogvalue = jnienv->GetMethodID(activityClass,
			"getDialogMessage", "()Ljava/lang/String;");

	FATAL_ERROR_IF(dialogvalue == nullptr,
			"porting::getInputDialogMessage unable to find Java getDialogMessage method");

	jobject result = jnienv->CallObjectMethod(activity,
			dialogvalue);
	return readJavaString((jstring) result);
}

int getInputDialogSelection()
{
	jmethodID dialogvalue = jnienv->GetMethodID(activityClass, "getDialogSelection", "()I");

	FATAL_ERROR_IF(dialogvalue == nullptr,
			"porting::getInputDialogSelection unable to find Java getDialogSelection method");

	return jnienv->CallIntMethod(activity, dialogvalue);
}

float getDisplayDensity()
{
	static bool firstrun = true;
	static float value = 0;

	if (firstrun) {
		jmethodID getDensity = jnienv->GetMethodID(activityClass,
				"getDensity", "()F");

		FATAL_ERROR_IF(getDensity == nullptr,
			"porting::getDisplayDensity unable to find Java getDensity method");

		value = jnienv->CallFloatMethod(activity, getDensity);
		firstrun = false;
	}

	return value;
}

v2u32 getDisplaySize()
{
	static bool firstrun = true;
	static v2u32 retval;

	if (firstrun) {
		jmethodID getDisplayWidth = jnienv->GetMethodID(activityClass,
				"getDisplayWidth", "()I");

		FATAL_ERROR_IF(getDisplayWidth == nullptr,
			"porting::getDisplayWidth unable to find Java getDisplayWidth method");

		retval.X = jnienv->CallIntMethod(activity,
				getDisplayWidth);

		jmethodID getDisplayHeight = jnienv->GetMethodID(activityClass,
				"getDisplayHeight", "()I");

		FATAL_ERROR_IF(getDisplayHeight == nullptr,
			"porting::getDisplayHeight unable to find Java getDisplayHeight method");

		retval.Y = jnienv->CallIntMethod(activity,
				getDisplayHeight);

		firstrun = false;
	}

	return retval;
}

std::string getLanguageAndroid()
{
	jmethodID getLanguage = jnienv->GetMethodID(activityClass,
			"getLanguage", "()Ljava/lang/String;");

	FATAL_ERROR_IF(getLanguage == nullptr,
		"porting::getLanguageAndroid unable to find Java getLanguage method");

	jobject result = jnienv->CallObjectMethod(activity,
			getLanguage);
	return readJavaString((jstring) result);
}

bool hasPhysicalKeyboardAndroid()
{
	jmethodID hasPhysicalKeyboard = jnienv->GetMethodID(activityClass,
			"hasPhysicalKeyboard", "()Z");

	FATAL_ERROR_IF(hasPhysicalKeyboard == nullptr,
		"porting::hasPhysicalKeyboardAndroid unable to find Java hasPhysicalKeyboard method");

	jboolean result = jnienv->CallBooleanMethod(activity,
			hasPhysicalKeyboard);
	return result;
}

}
