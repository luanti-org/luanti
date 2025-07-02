/*
Minetest
Copyright (C) 2014-2020 MoNTE48, Maksim Gamarnik <MoNTE48@mail.ua>
Copyright (C) 2014-2020 ubulem,  Bektur Mambetov <berkut87@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

package net.minetest.minetest;

import org.libsdl.app.SDLActivity;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.ActivityNotFoundException;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.util.Log;
import android.view.KeyEvent;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.Toast;
import android.content.res.Configuration;

import androidx.annotation.Keep;
import androidx.appcompat.app.AlertDialog;
import androidx.core.content.FileProvider;

import java.io.File;
import java.util.Locale;
import java.util.Objects;

// Native code finds these methods by name (see porting_android.cpp).
// This annotation prevents the minifier/Proguard from mangling them.
@Keep
@SuppressWarnings("unused")
public class GameActivity extends SDLActivity {
	@Override
	protected String getMainSharedObject() {
		return getContext().getApplicationInfo().nativeLibraryDir + "/libluanti.so";
	}

	@Override
	protected String getMainFunction() {
		return "SDL_Main";
	}

	@Override
	protected String[] getLibraries() {
		return new String[] {
			"luanti"
		};
	}

	// Prevent SDL from changing orientation settings since we already set the
	// correct orientation in our AndroidManifest.xml
	@Override
	public void setOrientationBis(int w, int h, boolean resizable, String hint) {}

	enum DialogType { TEXT_INPUT, SELECTION_INPUT }
	enum DialogState { DIALOG_SHOWN, DIALOG_INPUTTED, DIALOG_CANCELED }

	private DialogType lastDialogType = DialogType.TEXT_INPUT;
	private DialogState inputDialogState = DialogState.DIALOG_CANCELED;
	private String messageReturnValue = "";
	private int selectionReturnValue = 0;

	private native void saveSettings();

	@Override
	protected void onStop() {
		super.onStop();
		// Avoid losing setting changes in case the app is onDestroy()ed later.
		// Saving stuff in onStop() is recommended in the Android activity
		// lifecycle documentation.
		saveSettings();
	}

	private NotificationManager mNotifyManager;
	private boolean gameNotificationShown = false;

	public void showTextInputDialog(String hint, String current, int editType) {
		runOnUiThread(() -> showTextInputDialogUI(hint, current, editType));
	}

	public void showSelectionInputDialog(String[] optionList, int selectedIdx) {
		runOnUiThread(() -> showSelectionInputDialogUI(optionList, selectedIdx));
	}

	private void showTextInputDialogUI(String hint, String current, int editType) {
		lastDialogType = DialogType.TEXT_INPUT;
		inputDialogState = DialogState.DIALOG_SHOWN;
		final AlertDialog.Builder builder = new AlertDialog.Builder(this);
		LinearLayout container = new LinearLayout(this);
		container.setOrientation(LinearLayout.VERTICAL);
		builder.setView(container);
		AlertDialog alertDialog = builder.create();
		CustomEditText editText = new CustomEditText(this, editType);
		container.addView(editText);
		editText.setMaxLines(8);
		editText.setHint(hint);
		editText.setText(current);
		if (editType == 1)
			editText.setInputType(InputType.TYPE_CLASS_TEXT |
					InputType.TYPE_TEXT_FLAG_MULTI_LINE);
		else if (editType == 3)
			editText.setInputType(InputType.TYPE_CLASS_TEXT |
					InputType.TYPE_TEXT_VARIATION_PASSWORD);
		else
			editText.setInputType(InputType.TYPE_CLASS_TEXT);
		editText.setSelection(Objects.requireNonNull(editText.getText()).length());
		final InputMethodManager imm = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
		editText.setOnKeyListener((view, keyCode, event) -> {
			// For multi-line, do not submit the text after pressing Enter key
			if (keyCode == KeyEvent.KEYCODE_ENTER && editType != 1) {
				imm.hideSoftInputFromWindow(editText.getWindowToken(), 0);
				inputDialogState = DialogState.DIALOG_INPUTTED;
				messageReturnValue = editText.getText().toString();
				alertDialog.dismiss();
				return true;
			}
			return false;
		});
		// For multi-line, add Done button since Enter key does not submit text
		if (editType == 1) {
			Button doneButton = new Button(this);
			container.addView(doneButton);
			doneButton.setText(R.string.ime_dialog_done);
			doneButton.setOnClickListener((view -> {
				imm.hideSoftInputFromWindow(editText.getWindowToken(), 0);
				inputDialogState = DialogState.DIALOG_INPUTTED;
				messageReturnValue = editText.getText().toString();
				alertDialog.dismiss();
			}));
		}
		alertDialog.setOnCancelListener(dialog -> {
			getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
			inputDialogState = DialogState.DIALOG_CANCELED;
			messageReturnValue = current;
		});
		alertDialog.show();
		editText.requestFocusTryShow();
	}

	public void showSelectionInputDialogUI(String[] optionList, int selectedIdx) {
		lastDialogType = DialogType.SELECTION_INPUT;
		inputDialogState = DialogState.DIALOG_SHOWN;
		final AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setSingleChoiceItems(optionList, selectedIdx, (dialog, selection) -> {
			inputDialogState = DialogState.DIALOG_INPUTTED;
			selectionReturnValue = selection;
			dialog.dismiss();
		});
		builder.setOnCancelListener(dialog -> {
			inputDialogState = DialogState.DIALOG_CANCELED;
			selectionReturnValue = selectedIdx;
		});
		AlertDialog alertDialog = builder.create();
		alertDialog.show();
	}

	public int getLastDialogType() {
		return lastDialogType.ordinal();
	}

	public int getInputDialogState() {
		return inputDialogState.ordinal();
	}

	public String getDialogMessage() {
		inputDialogState = DialogState.DIALOG_CANCELED;
		return messageReturnValue;
	}

	public int getDialogSelection() {
		inputDialogState = DialogState.DIALOG_CANCELED;
		return selectionReturnValue;
	}

	public float getDensity() {
		return getResources().getDisplayMetrics().density;
	}

	public int getDisplayHeight() {
		return getResources().getDisplayMetrics().heightPixels;
	}

	public int getDisplayWidth() {
		return getResources().getDisplayMetrics().widthPixels;
	}

	public void openURI(String uri) {
		Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(uri));
		try {
			startActivity(browserIntent);
		} catch (ActivityNotFoundException e) {
			runOnUiThread(() -> Toast.makeText(this, R.string.no_web_browser, Toast.LENGTH_SHORT).show());
		}
	}

	public String getUserDataPath() {
		return Utils.getUserDataDirectory(this).getAbsolutePath();
	}

	public String getCachePath() {
		return Utils.getCacheDirectory(this).getAbsolutePath();
	}

	public void shareFile(String path) {
		File file = new File(path);
		if (!file.exists()) {
			Log.e("GameActivity", "File " + file.getAbsolutePath() + " doesn't exist");
			return;
		}

		Uri fileUri = FileProvider.getUriForFile(this, "net.minetest.minetest.fileprovider", file);

		Intent intent = new Intent(Intent.ACTION_SEND, fileUri);
		intent.setDataAndType(fileUri, getContentResolver().getType(fileUri));
		intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
		intent.putExtra(Intent.EXTRA_STREAM, fileUri);

		Intent shareIntent = Intent.createChooser(intent, null);
		startActivity(shareIntent);
	}

	public String getLanguage() {
		String langCode = Locale.getDefault().getLanguage();

		// getLanguage() still uses old language codes to preserve compatibility.
		// List of code changes in ISO 639-2:
		// https://www.loc.gov/standards/iso639-2/php/code_changes.php
		switch (langCode) {
			case "in":
				langCode = "id"; // Indonesian
				break;
			case "iw":
				langCode = "he"; // Hebrew
				break;
			case "ji":
				langCode = "yi"; // Yiddish
				break;
			case "jw":
				langCode = "jv"; // Javanese
				break;
		}

		return langCode;
	}

	public boolean hasPhysicalKeyboard() {
		return getContext().getResources().getConfiguration().keyboard != Configuration.KEYBOARD_NOKEYS;
	}

	// TODO: share code with UnzipService.createNotification
	private void updateGameNotification() {
		if (mNotifyManager == null) {
			mNotifyManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
		}

		if (!gameNotificationShown) {
			mNotifyManager.cancel(MainActivity.NOTIFICATION_ID_GAME);
			return;
		}

		Notification.Builder builder;
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
			builder = new Notification.Builder(this, MainActivity.NOTIFICATION_CHANNEL_ID);
		} else {
			builder = new Notification.Builder(this);
		}

		Intent notificationIntent = new Intent(this, GameActivity.class);
		notificationIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP
			| Intent.FLAG_ACTIVITY_SINGLE_TOP);
		int pendingIntentFlag = 0;
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
			pendingIntentFlag = PendingIntent.FLAG_MUTABLE;
		}
		PendingIntent intent = PendingIntent.getActivity(this, 0,
			notificationIntent, pendingIntentFlag);

		builder.setContentTitle(getString(R.string.game_notification_title))
			.setSmallIcon(R.mipmap.ic_launcher)
			.setContentIntent(intent)
			.setOngoing(true);

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
			// This avoids a stuck notification if the app is killed while
			// in-game: (1) if the user closes the app from the "Recents" screen
			// or (2) if the system kills the app while it is in background.
			// onStop is called too early to remove the notification and
			// onDestroy is often not called at all, so there's this hack instead.
			builder.setTimeoutAfter(16000);

			// Replace the notification just before it expires as long as the app is
			// running (and we're still in-game).
			final Handler handler = new Handler(Looper.getMainLooper());
			handler.postDelayed(new Runnable() {
				@Override
				public void run() {
					if (gameNotificationShown) {
						updateGameNotification();
					}
				}
			}, 15000);
		}

		mNotifyManager.notify(MainActivity.NOTIFICATION_ID_GAME, builder.build());
	}


	public void setPlayingNowNotification(boolean show) {
		gameNotificationShown = show;
		updateGameNotification();
	}
}
