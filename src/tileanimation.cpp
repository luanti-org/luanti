// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2016 sfan5 <sfan5@live.de>
#include "tileanimation.h"
#include "util/serialize.h"

void TileAnimationParams::serialize(std::ostream &os, u16 protocol_ver) const
{
	// The particle code overloads the length parameter so that negative numbers
	// indicate an extra feature which old clients don't understand (crash).
	// In hindsight it would have been better to use an extra parameter for this
	// but we're stuck with this now.
	const bool need_abs = protocol_ver < 40;

	writeU8(os, type);
	if (type == TAT_VERTICAL_FRAMES) {
		writeU16(os, vertical_frames.aspect_w);
		writeU16(os, vertical_frames.aspect_h);
		writeF32(os, need_abs ? std::abs(vertical_frames.length) : vertical_frames.length);
	} else if (type == TAT_SHEET_2D) {
		writeU8(os, sheet_2d.frames_w);
		writeU8(os, sheet_2d.frames_h);
		writeF32(os, need_abs ? std::abs(sheet_2d.frame_length) : sheet_2d.frame_length);
	}
}

void TileAnimationParams::deSerialize(std::istream &is, u16 protocol_ver)
{
	type = static_cast<TileAnimationType>(readU8(is));
	switch(type) {
		case TAT_NONE:
			break;
		case TAT_VERTICAL_FRAMES:
			vertical_frames.aspect_w = readU16(is);
			vertical_frames.aspect_h = readU16(is);
			vertical_frames.length = readF32(is);
			break;
		case TAT_SHEET_2D:
			sheet_2d.frames_w = readU8(is);
			sheet_2d.frames_h = readU8(is);
			sheet_2d.frame_length = readF32(is);
			break;
		default:
			type = TAT_NONE;
			break;
	}
}

void TileAnimationParams::determineParams(v2u32 texture_size, int *frame_count,
		int *frame_length_ms, v2u32 *frame_size) const
{
	if (type == TAT_VERTICAL_FRAMES) {
		int frame_height = (float)texture_size.X /
				(float)vertical_frames.aspect_w *
				(float)vertical_frames.aspect_h;
		int _frame_count = texture_size.Y / frame_height;
		if (frame_count)
			*frame_count = _frame_count;
		if (frame_length_ms)
			*frame_length_ms = 1000 * vertical_frames.length / _frame_count;
		if (frame_size)
			*frame_size = v2u32(texture_size.X, frame_height);
	} else if (type == TAT_SHEET_2D) {
		if (frame_count)
			*frame_count = sheet_2d.frames_w * sheet_2d.frames_h;
		if (frame_length_ms)
			*frame_length_ms = 1000 * sheet_2d.frame_length;
		if (frame_size)
			*frame_size = v2u32(texture_size.X / sheet_2d.frames_w, texture_size.Y / sheet_2d.frames_h);
	}
	// caller should check for TAT_NONE
}

void TileAnimationParams::getTextureModifer(std::ostream &os, v2u32 texture_size, int frame) const
{
	if (type == TAT_NONE)
		return;
	if (type == TAT_VERTICAL_FRAMES) {
		int frame_count;
		determineParams(texture_size, &frame_count, NULL, NULL);
		os << "^[verticalframe:" << frame_count << ":" << frame;
	} else if (type == TAT_SHEET_2D) {
		int q, r;
		q = frame / sheet_2d.frames_w;
		r = frame % sheet_2d.frames_w;
		os << "^[sheet:" << sheet_2d.frames_w << "x" << sheet_2d.frames_h
			<< ":" << r << "," << q;
	}
}

v2f TileAnimationParams::getTextureCoords(v2u32 texture_size, int frame) const
{
	v2u32 ret(0, 0);
	if (type == TAT_VERTICAL_FRAMES) {
		int frame_height = (float)texture_size.X /
				(float)vertical_frames.aspect_w *
				(float)vertical_frames.aspect_h;
		ret = v2u32(0, frame_height * frame);
	} else if (type == TAT_SHEET_2D) {
		v2u32 frame_size;
		determineParams(texture_size, NULL, NULL, &frame_size);
		int q, r;
		q = frame / sheet_2d.frames_w;
		r = frame % sheet_2d.frames_w;
		ret = v2u32(r * frame_size.X, q * frame_size.Y);
	}
	return v2f::from(ret) / v2f::from(texture_size);
}
