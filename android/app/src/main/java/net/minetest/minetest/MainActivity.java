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

import android.annotation.SuppressLint;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.RequiresApi;
import androidx.annotation.StringRes;
import androidx.appcompat.app.AppCompatActivity;

import static net.minetest.minetest.UnzipService.*;

public class MainActivity extends AppCompatActivity {
	public static final String NOTIFICATION_CHANNEL_ID = "Minetest channel";
	public static final int NOTIFICATION_ID_UNZIP = 1;
	public static final int NOTIFICATION_ID_GAME = 2;

	private final static int versionCode = BuildConfig.VERSION_CODE;
	private static final String SETTINGS = "MinetestSettings";
	private static final String TAG_VERSION_CODE = "versionCode";

	private ProgressBar mProgressBar;
	private TextView mTextView;
	private SharedPreferences sharedPreferences;

	private final BroadcastReceiver myReceiver = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			int progress = 0;
			@StringRes int message = 0;
			if (intent != null) {
				progress = intent.getIntExtra(ACTION_PROGRESS, 0);
				message = intent.getIntExtra(ACTION_PROGRESS_MESSAGE, 0);
			}

			if (progress == FAILURE) {
				Toast.makeText(MainActivity.this, intent.getStringExtra(ACTION_FAILURE), Toast.LENGTH_LONG).show();
				finish();
			} else if (progress == SUCCESS) {
				startNative();
			} else {
				if (mProgressBar != null) {
					mProgressBar.setVisibility(View.VISIBLE);
					if (progress == INDETERMINATE) {
						mProgressBar.setIndeterminate(true);
					} else {
						mProgressBar.setIndeterminate(false);
						mProgressBar.setProgress(progress);
					}
				}
				mTextView.setVisibility(View.VISIBLE);
				if (message != 0)
					mTextView.setText(message);
			}
		}
	};

	@SuppressLint("UnspecifiedRegisterReceiverFlag")
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		IntentFilter filter = new IntentFilter(ACTION_UPDATE);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
			registerReceiver(myReceiver, filter, RECEIVER_NOT_EXPORTED);
		} else {
			registerReceiver(myReceiver, filter);
		}

		mProgressBar = findViewById(R.id.progressBar);
		mTextView = findViewById(R.id.textView);
		sharedPreferences = getSharedPreferences(SETTINGS, Context.MODE_PRIVATE);

		checkAppVersion();

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
			createNotificationChannel();
	}

	private void checkAppVersion() {
		if (UnzipService.getIsRunning()) {
			mProgressBar.setVisibility(View.VISIBLE);
			mProgressBar.setIndeterminate(true);
			mTextView.setVisibility(View.VISIBLE);
		} else if (sharedPreferences.getInt(TAG_VERSION_CODE, 0) == versionCode &&
				Utils.isInstallValid(this)) {
			startNative();
		} else {
			mProgressBar.setVisibility(View.VISIBLE);
			mProgressBar.setIndeterminate(true);
			mTextView.setVisibility(View.VISIBLE);

			Intent intent = new Intent(this, UnzipService.class);
			startService(intent);
		}
	}

	private void startNative() {
		sharedPreferences.edit().putInt(TAG_VERSION_CODE, versionCode).apply();
		Intent intent = new Intent(this, GameActivity.class);
		intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_CLEAR_TASK);
		startActivity(intent);
	}

	@RequiresApi(Build.VERSION_CODES.O)
	private void createNotificationChannel() {
		NotificationManager notifyManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
		if (notifyManager == null)
			return;

		NotificationChannel notifyChannel = new NotificationChannel(
			NOTIFICATION_CHANNEL_ID,
			getString(R.string.notification_channel_name),
			NotificationManager.IMPORTANCE_LOW
		);
		notifyChannel.setDescription(getString(R.string.notification_channel_description));
		// Configure the notification channel without sound set
		notifyChannel.setSound(null, null);
		notifyChannel.enableLights(false);
		notifyChannel.enableVibration(false);

		// It is fine to always create the notification channel because creating a channel
		// with the same ID is the same as overriding it (only its name and description).
		notifyManager.createNotificationChannel(notifyChannel);
	}

	@Override
	public void onBackPressed() {
		// Prevent abrupt interruption when copy game files from assets
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
		unregisterReceiver(myReceiver);
	}
}
