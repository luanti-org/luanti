// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "util/serialize.h"

#include <dimension2d.h>
#include <ITexture.h>
#include <rect.h>
#include <vector2d.h>
#include <vector3d.h>

#include <algorithm>
#include <sstream>

namespace ui
{
	// Define some useful named colors.
	const video::SColor BLANK = 0x00000000;
	const video::SColor BLACK = 0xFF000000;
	const video::SColor WHITE = 0xFFFFFFFF;

	/* UIs deal with tons of 2D positions, sizes, rectangles, and the like, but
	 * Irrlicht's core::vector2d, core::dimension2d, and core::rect classes are
	 * inadequate for the job. For instance, vectors use the mathematical
	 * definition and hence have multiple special forms of multiplication.
	 * Notably, they don't have component-wise multiplication, which is the
	 * only one useful for UIs aside from scalar multiplication. Additionally,
	 * the distinction between a position and a dimension are blurred since
	 * vectors can perform operations that make no sense for absolute
	 * positions, e.g. position + position. Dimensions are underpowered, and
	 * rectangles are clunky to work with for multiple reasons.
	 *
	 * So, we create our own classes for use with 2D drawing and UIs. The Pos
	 * class represents an absolute position, whereas the Size class represents
	 * a relative displacement, size, or scaling factor. Similarly, the Rect
	 * class represents an absolute rectangle defined by two Pos fields,
	 * whereas the Disp class represents a double relative displacement or
	 * scaling factor defined by two Size fields.
	 *
	 * All operations are component-wise, e.g. the multiplication operation for
	 * two sizes is defined as `a * b == {a.W * b.W, a.H * b.H}`. Other useful
	 * operations exist, like component-wise minimums and maximums, unions and
	 * intersections of rectangles, offseting rectangles with displacements in
	 * multiple ways, and so on. Lastly, functions never mutate the class in
	 * place, so you can do
	 *
	 *     doThing(a.intersectWith(b));
	 *
	 * rather than being forced to use the much more clunky
	 *
	 *     core::recti c = a;
	 *     c.clipAgainst(b);
	 *     doThing(c);
	 *
	 * Implicit conversions between these classes and the corresponding
	 * Irrlicht classes are defined for seamless interoperability with code
	 * that still uses Irrlicht's types.
	 */
	template<typename E> struct Pos;
	template<typename E> struct Size;
	template<typename E> struct Rect;
	template<typename E> struct Disp;

	template<typename E>
	struct Size
	{
		E W;
		E H;

		Size() : W(), H() {}
		explicit Size(E n) : W(n), H(n) {}
		Size(E w, E h) : W(w), H(h) {}

		template<typename K>
		explicit Size(Pos<K> pos) : W(pos.X), H(pos.Y) {}
		template<typename K>
		Size(Size<K> other) : W(other.W), H(other.H) {}

		template<typename K>
		explicit Size(core::vector2d<K> vec) : W(vec.X), H(vec.Y) {}
		template<typename K>
		Size(core::dimension2d<K> dim) : W(dim.Width), H(dim.Height) {}

		template<typename K>
		explicit operator core::vector2d<K>() const { return core::vector2d<K>(W, H); }
		template<typename K>
		operator core::dimension2d<K>() const { return core::dimension2d<K>(W, H); }

		E area() const { return W * H; }
		bool empty() const { return area() == 0; }

		bool operator==(Size<E> other) const { return W == other.W && H == other.H; }
		bool operator!=(Size<E> other) const { return !(*this == other); }

		E &operator[](int index) { return index ? H : W; }
		const E &operator[](int index) const { return index ? H : W; }

		Size<E> operator+() const { return Size<E>(+W, +H); }
		Size<E> operator-() const { return Size<E>(-W, -H); }

		Size<E> operator+(Size<E> other) const { return Size<E>(W + other.W, H + other.H); }
		Size<E> operator-(Size<E> other) const { return Size<E>(W - other.W, H - other.H); }

		Size<E> &operator+=(Size<E> other) { *this = *this + other; return *this; }
		Size<E> &operator-=(Size<E> other) { *this = *this - other; return *this; }

		Size<E> operator*(Size<E> other) const { return Size<E>(W * other.W, H * other.H); }
		Size<E> operator/(Size<E> other) const { return Size<E>(W / other.W, H / other.H); }

		Size<E> &operator*=(Size<E> other) { *this = *this * other; return *this; }
		Size<E> &operator/=(Size<E> other) { *this = *this / other; return *this; }

		Size<E> operator*(E scalar) const { return Size<E>(W * scalar, H * scalar); }
		Size<E> operator/(E scalar) const { return Size<E>(W / scalar, H / scalar); }

		Size<E> &operator*=(E scalar) { *this = *this * scalar; return *this; }
		Size<E> &operator/=(E scalar) { *this = *this / scalar; return *this; }

		Size<E> min(Size<E> other) const
			{ return Size<E>(std::min(W, other.W), std::min(H, other.H)); }
		Size<E> max(Size<E> other) const
			{ return Size<E>(std::max(W, other.W), std::max(H, other.H)); }

		Size<E> clamp(Size<E> lo, Size<E> hi) const
			{ return Size<E>(std::clamp(W, lo.W, hi.W), std::clamp(H, lo.H, hi.H)); }
		Size<E> clamp(Disp<E> disp) const
			{ return clamp(disp.TopLeft, disp.BottomRight); }

		Size<E> clip() const { return max(Size<E>()); }

		friend std::ostream &operator<<(std::ostream &os, Size<E> size)
		{
			os << "(" << size.W << ", " << size.H << ")";
			return os;
		}
	};

	using SizeI = Size<s32>;
	using SizeU = Size<u32>;
	using SizeF = Size<f32>;

	template<typename E>
	struct Pos
	{
		E X;
		E Y;

		Pos() : X(), Y() {}
		explicit Pos(E n) : X(n), Y(n) {}
		Pos(E x, E y) : X(x), Y(y) {}

		template<typename K>
		Pos(Pos<K> other) : X(other.X), Y(other.Y) {}
		template<typename K>
		explicit Pos(Size<K> size) : X(size.W), Y(size.H) {}

		template<typename K>
		Pos(core::vector2d<K> vec) : X(vec.X), Y(vec.Y) {}
		template<typename K>
		explicit Pos(core::dimension2d<K> dim) : X(dim.Width), Y(dim.Height) {}

		template<typename K>
		operator core::vector2d<K>() const { return core::vector2d<K>(X, Y); }
		template<typename K>
		explicit operator core::dimension2d<K>() const { return core::dimension2d<K>(X, Y); }

		bool operator==(Pos<E> other) const { return X == other.X && Y == other.Y; }
		bool operator!=(Pos<E> other) const { return !(*this == other); }

		E &operator[](int index) { return index ? Y : X; }
		const E &operator[](int index) const { return index ? Y : X; }

		Pos<E> operator+(Size<E> size) const { return Pos<E>(X + size.W, Y + size.H); }
		Pos<E> operator-(Size<E> size) const { return Pos<E>(X - size.W, Y - size.H); }

		Pos<E> &operator+=(Size<E> size) { *this = *this + size; return *this; }
		Pos<E> &operator-=(Size<E> size) { *this = *this - size; return *this; }

		Pos<E> operator*(Size<E> size) const { return Pos<E>(X * size.W, Y * size.H); }
		Pos<E> operator/(Size<E> size) const { return Pos<E>(X / size.W, Y / size.H); }

		Pos<E> &operator*=(Size<E> size) { *this = *this * size; return *this; }
		Pos<E> &operator/=(Size<E> size) { *this = *this / size; return *this; }

		Pos<E> operator*(E scalar) const { return Pos<E>(X * scalar, Y * scalar); }
		Pos<E> operator/(E scalar) const { return Pos<E>(X / scalar, Y / scalar); }

		Pos<E> &operator*=(E scalar) { *this = *this * scalar; return *this; }
		Pos<E> &operator/=(E scalar) { *this = *this / scalar; return *this; }

		Size<E> operator-(Pos<E> other) const { return Size<E>(X - other.X, Y - other.Y); }
		Size<E> operator/(Pos<E> other) const { return Size<E>(X / other.X, Y / other.Y); }

		Pos<E> min(Pos<E> other) const
			{ return Pos<E>(std::min(X, other.X), std::min(Y, other.Y)); }
		Pos<E> max(Pos<E> other) const
			{ return Pos<E>(std::max(X, other.X), std::max(Y, other.Y)); }

		Pos<E> clamp(Pos<E> lo, Pos<E> hi) const
			{ return Pos<E>(std::clamp(X, lo.X, hi.X), std::clamp(Y, lo.Y, hi.Y)); }
		Pos<E> clamp(Rect<E> rect) const
			{ return clamp(rect.TopLeft, rect.BottomRight); }

		friend std::ostream &operator<<(std::ostream &os, Pos<E> pos)
		{
			os << "(" << pos.X << ", " << pos.Y << ")";
			return os;
		}
	};

	using PosI = Pos<s32>;
	using PosU = Pos<u32>;
	using PosF = Pos<f32>;

	template<typename E>
	struct Disp
	{
		union {
			struct {
				E L;
				E T;
			};
			Size<E> TopLeft;
		};
		union {
			struct {
				E R;
				E B;
			};
			Size<E> BottomRight;
		};

		Disp() : L(), T(), R(), B() {}
		explicit Disp(E n) : L(n), T(n), R(n), B(n) {}
		Disp(E x, E y) : L(x), T(y), R(x), B(y) {}
		Disp(E l, E t, E r, E b) : L(l), T(t), R(r), B(b) {}

		explicit Disp(Size<E> size) : TopLeft(size), BottomRight(size) {}
		Disp(Size<E> tl, Size<E> br) : TopLeft(tl), BottomRight(br) {}

		template<typename K>
		explicit Disp(Rect<K> rect) : TopLeft(rect.TopLeft), BottomRight(rect.BottomRight) {}
		template<typename K>
		Disp(Disp<K> other) : TopLeft(other.TopLeft), BottomRight(other.BottomRight) {}

		template<typename K>
		explicit Disp(core::rect<K> rect) :
			TopLeft(rect.UpperLeftCorner), BottomRight(rect.LowerRightCorner) {}

		template<typename K>
		explicit operator core::rect<K>() const { return core::rect<K>(Rect<K>(*this)); }

		E X() const { return L + R; }
		E Y() const { return T + B; }
		Size<E> extents() const { return TopLeft + BottomRight; }

		bool operator==(Disp<E> other) const
			{ return TopLeft == other.TopLeft && BottomRight == other.BottomRight; }
		bool operator!=(Disp<E> other) const { return !(*this == other); }

		Disp<E> operator+() const { return Disp<E>(+TopLeft, +BottomRight); }
		Disp<E> operator-() const { return Disp<E>(-TopLeft, -BottomRight); }

		Disp<E> operator+(Disp<E> other) const
			{ return Disp<E>(TopLeft + other.TopLeft, BottomRight + other.BottomRight); }
		Disp<E> operator-(Disp<E> other) const
			{ return Disp<E>(TopLeft - other.TopLeft, BottomRight - other.BottomRight); }

		Disp<E> &operator+=(Disp<E> other) { *this = *this + other; return *this; }
		Disp<E> &operator-=(Disp<E> other) { *this = *this - other; return *this; }

		Disp<E> operator*(Disp<E> other) const
			{ return Disp<E>(TopLeft * other.TopLeft, BottomRight * other.BottomRight); }
		Disp<E> operator/(Disp<E> other) const
			{ return Disp<E>(TopLeft / other.TopLeft, BottomRight / other.BottomRight); }

		Disp<E> &operator*=(Disp<E> other) { *this = *this * other; return *this; }
		Disp<E> &operator/=(Disp<E> other) { *this = *this / other; return *this; }

		Disp<E> operator*(E scalar) const
			{ return Disp<E>(TopLeft * scalar, BottomRight * scalar); }
		Disp<E> operator/(E scalar) const
			{ return Disp<E>(TopLeft / scalar, BottomRight / scalar); }

		Disp<E> &operator*=(E scalar) { *this = *this * scalar; return *this; }
		Disp<E> &operator/=(E scalar) { *this = *this / scalar; return *this; }

		Disp<E> clip() const { return Disp<E>(TopLeft.clip(), BottomRight.clip()); }

		friend std::ostream &operator<<(std::ostream &os, Disp<E> disp)
		{
			os << "(" << disp.L << ", " << disp.T << ", " << disp.R << ", " << disp.B << ")";
			return os;
		}
	};

	using DispI = Disp<s32>;
	using DispU = Disp<u32>;
	using DispF = Disp<f32>;

	template<typename E>
	struct Rect
	{
		union {
			struct {
				E L;
				E T;
			};
			Pos<E> TopLeft;
		};
		union {
			struct {
				E R;
				E B;
			};
			Pos<E> BottomRight;
		};

		Rect() : L(), T(), R(), B() {}
		Rect(E l, E t, E r, E b) : L(l), T(t), R(r), B(b) {}

		explicit Rect(Pos<E> pos) : TopLeft(pos), BottomRight(pos) {}
		Rect(Pos<E> tl, Pos<E> br) : TopLeft(tl), BottomRight(br) {}

		explicit Rect(Size<E> size) : TopLeft(), BottomRight(size) {}
		Rect(Pos<E> pos, Size<E> size) : TopLeft(pos), BottomRight(pos + size) {}

		template<typename K>
		Rect(Rect<K> other) : TopLeft(other.TopLeft), BottomRight(other.BottomRight) {}
		template<typename K>
		explicit Rect(Disp<K> disp) : TopLeft(disp.TopLeft), BottomRight(disp.BottomRight) {}

		template<typename K>
		Rect(core::rect<K> rect) :
			TopLeft(rect.UpperLeftCorner), BottomRight(rect.LowerRightCorner) {}

		template<typename K>
		operator core::rect<K>() const { return core::rect<K>(TopLeft, BottomRight); }

		E W() const { return R - L; }
		E H() const { return B - T; }
		Size<E> size() const { return BottomRight - TopLeft; }

		E area() const { return size().area(); }
		bool empty() const { return size().empty(); }

		bool operator==(Rect<E> other) const
			{ return TopLeft == other.TopLeft && BottomRight == other.BottomRight; }
		bool operator!=(Rect<E> other) const { return !(*this == other); }

		Rect<E> operator+(Disp<E> disp) const
			{ return Rect<E>(TopLeft + disp.TopLeft, BottomRight + disp.BottomRight); }
		Rect<E> operator-(Disp<E> disp) const
			{ return Rect<E>(TopLeft - disp.TopLeft, BottomRight - disp.BottomRight); }

		Rect<E> &operator+=(Disp<E> disp) { *this = *this + disp; return *this; }
		Rect<E> &operator-=(Disp<E> disp) { *this = *this - disp; return *this; }

		Rect<E> operator*(Disp<E> disp) const
			{ return Rect<E>(TopLeft * disp.TopLeft, BottomRight * disp.BottomRight); }
		Rect<E> operator/(Disp<E> disp) const
			{ return Rect<E>(TopLeft / disp.TopLeft, BottomRight / disp.BottomRight); }

		Rect<E> &operator*=(Disp<E> disp) { *this = *this * disp; return *this; }
		Rect<E> &operator/=(Disp<E> disp) { *this = *this / disp; return *this; }

		Rect<E> operator*(E scalar) const
			{ return Rect<E>(TopLeft * scalar, BottomRight * scalar); }
		Rect<E> operator/(E scalar) const
			{ return Rect<E>(TopLeft / scalar, BottomRight / scalar); }

		Rect<E> &operator*=(E scalar) { *this = *this * scalar; return *this; }
		Rect<E> &operator/=(E scalar) { *this = *this / scalar; return *this; }

		Disp<E> operator-(Rect<E> other) const
			{ return Disp<E>(TopLeft - other.TopLeft, BottomRight - other.BottomRight); }
		Disp<E> operator/(Rect<E> other) const
			{ return Disp<E>(TopLeft / other.TopLeft, BottomRight / other.BottomRight); }

		Rect<E> insetBy(Disp<E> disp) const
			{ return Rect<E>(TopLeft + disp.TopLeft, BottomRight - disp.BottomRight); }
		Rect<E> outsetBy(Disp<E> disp) const
			{ return Rect<E>(TopLeft - disp.TopLeft, BottomRight + disp.BottomRight); }

		Rect<E> unionWith(Rect<E> other) const
			{ return Rect<E>(TopLeft.min(other.TopLeft), BottomRight.max(other.BottomRight)); }
		Rect<E> intersectWith(Rect<E> other) const
			{ return Rect<E>(TopLeft.max(other.TopLeft), BottomRight.min(other.BottomRight)); }

		Rect<E> clip() const { return Rect<E>(TopLeft, size().clip()); }

		bool contains(Pos<E> pos) const
			{ return pos.X >= L && pos.Y >= T && pos.X < R && pos.Y < B; }

		friend std::ostream &operator<<(std::ostream &os, Rect<E> rect)
		{
			os << "(" << rect.L << ", " << rect.T << ", " << rect.R << ", " << rect.B << ")";
			return os;
		}
	};

	using RectI = Rect<s32>;
	using RectU = Rect<u32>;
	using RectF = Rect<f32>;

	// Define a few functions that are particularly useful for UI serialization
	// and deserialization. The testShift() function shifts out and returns the
	// lower bit of an integer containing flags, which is particularly useful
	// for testing whether an optional serialized field is present or not.
	inline bool testShift(u32 &mask)
	{
		bool test = mask & 1;
		mask >>= 1;
		return test;
	}

	// Booleans are often stored directly in the flags value. However, we want
	// the bit position of each field to stay constant, so the mask needs to be
	// shifted regardless of whether the boolean is set.
	inline void testShiftBool(u32 &mask, bool &flag)
	{
		if (testShift(mask)) {
			flag = testShift(mask);
		} else {
			testShift(mask);
		}
	}

	// Convenience functions for creating new binary streams.
	inline std::istringstream newIs(const std::string &str)
	{
		return std::istringstream(str, std::ios_base::binary);
	}

	inline std::ostringstream newOs()
	{
		return std::ostringstream(std::ios_base::binary);
	}

	// The UI purposefully avoids dealing with SerializationError, so it always
	// uses string functions that truncate gracefully. Hence, we make
	// convenience wrappers around the string functions in "serialize.h".
	inline std::string readStr16(std::istream &is) { return deSerializeString16(is, true); }
	inline std::string readStr32(std::istream &is) { return deSerializeString32(is, true); }

	inline void writeStr16(std::ostream &os, std::string_view str)
		{ os << serializeString16(str, true); }
	inline void writeStr32(std::ostream &os, std::string_view str)
		{ os << serializeString32(str, true); }

	// The UI also uses null-terminated strings for certain fields as well, so
	// define functions to work with them.
	inline std::string readNullStr(std::istream &is)
	{
		std::string str;
		std::getline(is, str, '\0');
		return str;
	}

	inline void writeNullStr(std::ostream &os, std::string_view str)
	{
		os << std::string_view(str.data(), std::min(str.find('\0'), str.size())) << '\0';
	}

	// Define serialization and deserialization functions that work with the
	// positioning types above. Brace initializer lists are used for the
	// constructors because they guarantee left-to-right argument evaluation.
	inline PosI readPosI(std::istream &is) { return PosI{readS32(is), readS32(is)}; }
	inline PosU readPosU(std::istream &is) { return PosU{readU32(is), readU32(is)}; }
	inline PosF readPosF(std::istream &is) { return PosF{readF32(is), readF32(is)}; }

	inline void writePosI(std::ostream &os, PosI pos)
		{ writeS32(os, pos.X); writeS32(os, pos.Y); }
	inline void writePosU(std::ostream &os, PosU pos)
		{ writeU32(os, pos.X); writeU32(os, pos.Y); }
	inline void writePosF(std::ostream &os, PosF pos)
		{ writeF32(os, pos.X); writeF32(os, pos.Y); }

	inline SizeI readSizeI(std::istream &is) { return SizeI{readS32(is), readS32(is)}; }
	inline SizeU readSizeU(std::istream &is) { return SizeU{readU32(is), readU32(is)}; }
	inline SizeF readSizeF(std::istream &is) { return SizeF{readF32(is), readF32(is)}; }

	inline void writeSizeI(std::ostream &os, SizeI size)
		{ writeS32(os, size.W); writeS32(os, size.H); }
	inline void writeSizeU(std::ostream &os, SizeU size)
		{ writeU32(os, size.W); writeU32(os, size.H); }
	inline void writeSizeF(std::ostream &os, SizeF size)
		{ writeF32(os, size.W); writeF32(os, size.H); }

	inline RectI readRectI(std::istream &is) { return RectI{readPosI(is), readPosI(is)}; }
	inline RectU readRectU(std::istream &is) { return RectU{readPosU(is), readPosU(is)}; }
	inline RectF readRectF(std::istream &is) { return RectF{readPosF(is), readPosF(is)}; }

	inline void writeRectI(std::ostream &os, RectI rect)
		{ writePosI(os, rect.TopLeft); writePosI(os, rect.BottomRight); }
	inline void writeRectU(std::ostream &os, RectU rect)
		{ writePosU(os, rect.TopLeft); writePosU(os, rect.BottomRight); }
	inline void writeRectF(std::ostream &os, RectF rect)
		{ writePosF(os, rect.TopLeft); writePosF(os, rect.BottomRight); }

	inline DispI readDispI(std::istream &is) { return DispI{readSizeI(is), readSizeI(is)}; }
	inline DispU readDispU(std::istream &is) { return DispU{readSizeU(is), readSizeU(is)}; }
	inline DispF readDispF(std::istream &is) { return DispF{readSizeF(is), readSizeF(is)}; }

	inline void writeDispI(std::ostream &os, DispI disp)
		{ writeSizeI(os, disp.TopLeft); writeSizeI(os, disp.BottomRight); }
	inline void writeDispU(std::ostream &os, DispU disp)
		{ writeSizeU(os, disp.TopLeft); writeSizeU(os, disp.BottomRight); }
	inline void writeDispF(std::ostream &os, DispF disp)
		{ writeSizeF(os, disp.TopLeft); writeSizeF(os, disp.BottomRight); }
}
