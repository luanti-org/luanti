// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#ifndef __ANDROID__
#error This header has to be included on Android port only!
#endif

#include "irrlichttypes_bloated.h"
#include <string>

namespace porting {
/**
 * Show a text input dialog in Java
 * @param hint Hint to be shown
 * @param current Initial value to be displayed
 * @param editType Type of the text field
 * (1 = multi-line text input; 2 = single-line text input; 3 = password field)
 */
void showTextInputDialog(const std::string &hint, const std::string &current, int editType);

/**
 * Show a selection dialog in Java
 * @param optionList The list of options
 * @param listSize Size of the list
 * @param selectedIdx Selected index
 */
void showComboBoxDialog(const std::string *optionList, s32 listSize, s32 selectedIdx);

/**
 * Opens a share intent to the file at path
 *
 * @param path
 */
void shareFileAndroid(const std::string &path);

/**
 * Shows/hides notification that the game is running
 *
 * @param show whether to show/hide the notification
 */
void setPlayingNowNotification(bool show);

/*
 * Types of Android input dialog:
 * 1. Text input (single/multi-line text and password field)
 * 2. Selection input (combo box)
 */
enum AndroidDialogType { TEXT_INPUT, SELECTION_INPUT };

/*
 * WORKAROUND for not working callbacks from Java -> C++
 * Get the type of the last input dialog
 */
AndroidDialogType getLastInputDialogType();

/*
 * States of Android input dialog:
 * 1. The dialog is currently shown.
 * 2. The dialog has its input sent.
 * 3. The dialog is canceled/dismissed.
 */
enum AndroidDialogState { DIALOG_SHOWN, DIALOG_INPUTTED, DIALOG_CANCELED };

/*
 * WORKAROUND for not working callbacks from Java -> C++
 * Get the state of the input dialog
 */
AndroidDialogState getInputDialogState();

/*
 * WORKAROUND for not working callbacks from Java -> C++
 * Get the text in the current/last input dialog
 * This function clears the dialog state (set to canceled). Make sure to save
 * the dialog state before calling this function.
 */
std::string getInputDialogMessage();

/*
 * WORKAROUND for not working callbacks from Java -> C++
 * Get the selection in the current/last input dialog
 * This function clears the dialog state (set to canceled). Make sure to save
 * the dialog state before calling this function.
 */
int getInputDialogSelection();


bool hasPhysicalKeyboardAndroid();

float getDisplayDensity();
v2u32 getDisplaySize();

}
