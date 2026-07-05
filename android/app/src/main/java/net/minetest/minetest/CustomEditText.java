/*
Minetest
Copyright (C) 2014-2020 MoNTE48, Maksim Gamarnik <MoNTE48@mail.ua>
Copyright (C) 2014-2020 ubulem,  Bektur Mambetov <berkut87@gmail.com>
Copyright (C) 2023 srifqi, Muhammad Rifqi Priyo Susanto
		<muhammadrifqipriyosusanto@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, see <https://www.gnu.org/licenses/>.
*/

package net.minetest.minetest;

import android.content.Context;
import android.view.KeyEvent;
import android.view.inputmethod.InputMethodManager;

import androidx.appcompat.widget.AppCompatEditText;

import java.util.Objects;

public class CustomEditText extends AppCompatEditText {
	private int editType = 2; // single line text input as default
	private boolean wantsToShowKeyboard = false;

	public CustomEditText(Context context) {
		super(context);
	}

	public CustomEditText(Context context, int _editType) {
		super(context);
		editType = _editType;
	}

	@Override
	public boolean onKeyPreIme(int keyCode, KeyEvent event) {
		// For multi-line, do not close the dialog after pressing back button
		if (editType != 1 && keyCode == KeyEvent.KEYCODE_BACK) {
			InputMethodManager mgr = (InputMethodManager)
					getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
			Objects.requireNonNull(mgr).hideSoftInputFromWindow(this.getWindowToken(), 0);
		}
		return false;
	}

	@Override
	public void onWindowFocusChanged(boolean hasWindowFocus) {
		super.onWindowFocusChanged(hasWindowFocus);
		tryShowKeyboard();
	}

	public void requestFocusTryShow() {
		requestFocus();
		wantsToShowKeyboard = true;
		tryShowKeyboard();
	}

	private void tryShowKeyboard() {
		if (hasWindowFocus() && wantsToShowKeyboard) {
			if (isFocused()) {
				CustomEditText that = this;
				post(() -> {
					final InputMethodManager imm = (InputMethodManager)
							getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
					imm.showSoftInput(that, 0);
				});
			}
			wantsToShowKeyboard = false;
		}
	}
}
