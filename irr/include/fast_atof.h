// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine" and the "irrXML" project.
// For conditions of distribution and use, see copyright notice in irrlicht.h and irrXML.h

#pragma once

#include "irrTypes.h"
#include <cfloat>
#include <climits>
#include <cmath>

namespace core
{

#define IRR_ATOF_TABLE_SIZE 17
// we write [IRR_ATOF_TABLE_SIZE] here instead of [] to work around a swig bug
const float fast_atof_table[17] = {
		0.f,
		0.1f,
		0.01f,
		0.001f,
		0.0001f,
		0.00001f,
		0.000001f,
		0.0000001f,
		0.00000001f,
		0.000000001f,
		0.0000000001f,
		0.00000000001f,
		0.000000000001f,
		0.0000000000001f,
		0.00000000000001f,
		0.000000000000001f,
		0.0000000000000001f};

//! Convert a simple string of base 10 digits into an unsigned 32 bit integer.
/** \param[in] in: The string of digits to convert. No leading chars are
	allowed, only digits 0 to 9. Parsing stops at the first non-digit.
	\param[out] out: (optional) If provided, it will be set to point at the
	first character not used in the calculation.
	\return The unsigned integer value of the digits. If the string specifies
	too many digits to encode in an u32 then INT_MAX will be returned.
*/
inline u32 strtoul10(const char *in, const char **out = 0)
{
	if (!in) {
		if (out)
			*out = in;
		return 0;
	}

	bool overflow = false;
	u32 unsignedValue = 0;
	while ((*in >= '0') && (*in <= '9')) {
		const u32 tmp = (unsignedValue * 10) + (*in - '0');
		if (tmp < unsignedValue) {
			unsignedValue = (u32)0xffffffff;
			overflow = true;
		}
		if (!overflow)
			unsignedValue = tmp;
		++in;
	}

	if (out)
		*out = in;

	return unsignedValue;
}

//! Convert a simple string of base 10 digits into a signed 32 bit integer.
/** \param[in] in: The string of digits to convert. Only a leading - or +
	followed by digits 0 to 9 will be considered. Parsing stops at the first
	non-digit.
	\param[out] out: (optional) If provided, it will be set to point at the
	first character not used in the calculation.
	\return The signed integer value of the digits. If the string specifies
	too many digits to encode in an s32 then +INT_MAX or -INT_MAX will be
	returned.
*/
inline s32 strtol10(const char *in, const char **out = 0)
{
	if (!in) {
		if (out)
			*out = in;
		return 0;
	}

	const bool negative = ('-' == *in);
	if (negative || ('+' == *in))
		++in;

	const u32 unsignedValue = strtoul10(in, out);
	if (unsignedValue > (u32)INT_MAX) {
		if (negative)
			return (s32)INT_MIN;
		else
			return (s32)INT_MAX;
	} else {
		if (negative)
			return -((s32)unsignedValue);
		else
			return (s32)unsignedValue;
	}
}

//! Convert a hex-encoded character to an unsigned integer.
/** \param[in] in The digit to convert. Only digits 0 to 9 and chars A-F,a-f
	will be considered.
	\return The unsigned integer value of the digit. 0xffffffff if the input is
	not hex
*/
inline u32 ctoul16(char in)
{
	if (in >= '0' && in <= '9')
		return in - '0';
	else if (in >= 'a' && in <= 'f')
		return 10u + in - 'a';
	else if (in >= 'A' && in <= 'F')
		return 10u + in - 'A';
	else
		return 0xffffffff;
}

//! Convert a simple string of base 16 digits into an unsigned 32 bit integer.
/** \param[in] in: The string of digits to convert. No leading chars are
	allowed, only digits 0 to 9 and chars A-F,a-f are allowed. Parsing stops
	at the first illegal char.
	\param[out] out: (optional) If provided, it will be set to point at the
	first character not used in the calculation.
	\return The unsigned integer value of the digits. If the string specifies
	too many digits to encode in an u32 then INT_MAX will be returned.
*/
inline u32 strtoul16(const char *in, const char **out = 0)
{
	if (!in) {
		if (out)
			*out = in;
		return 0;
	}

	bool overflow = false;
	u32 unsignedValue = 0;
	while (true) {
		u32 tmp = 0;
		if ((*in >= '0') && (*in <= '9'))
			tmp = (unsignedValue << 4u) + (*in - '0');
		else if ((*in >= 'A') && (*in <= 'F'))
			tmp = (unsignedValue << 4u) + (*in - 'A') + 10;
		else if ((*in >= 'a') && (*in <= 'f'))
			tmp = (unsignedValue << 4u) + (*in - 'a') + 10;
		else
			break;
		if (tmp < unsignedValue) {
			unsignedValue = (u32)INT_MAX;
			overflow = true;
		}
		if (!overflow)
			unsignedValue = tmp;
		++in;
	}

	if (out)
		*out = in;

	return unsignedValue;
}

//! Convert a simple string of base 8 digits into an unsigned 32 bit integer.
/** \param[in] in The string of digits to convert. No leading chars are
	allowed, only digits 0 to 7 are allowed. Parsing stops at the first illegal
	char.
	\param[out] out (optional) If provided, it will be set to point at the
	first character not used in the calculation.
	\return The unsigned integer value of the digits. If the string specifies
	too many digits to encode in an u32 then INT_MAX will be returned.
*/
inline u32 strtoul8(const char *in, const char **out = 0)
{
	if (!in) {
		if (out)
			*out = in;
		return 0;
	}

	bool overflow = false;
	u32 unsignedValue = 0;
	while (true) {
		u32 tmp = 0;
		if ((*in >= '0') && (*in <= '7'))
			tmp = (unsignedValue << 3u) + (*in - '0');
		else
			break;
		if (tmp < unsignedValue) {
			unsignedValue = (u32)INT_MAX;
			overflow = true;
		}
		if (!overflow)
			unsignedValue = tmp;
		++in;
	}

	if (out)
		*out = in;

	return unsignedValue;
}

//! Convert a C-style prefixed string (hex, oct, integer) into an unsigned 32 bit integer.
/** \param[in] in The string of digits to convert. If string starts with 0x the
	hex parser is used, if only leading 0 is used, oct parser is used. In all
	other cases, the usual unsigned parser is used.
	\param[out] out (optional) If provided, it will be set to point at the
	first character not used in the calculation.
	\return The unsigned integer value of the digits. If the string specifies
	too many digits to encode in an u32 then INT_MAX will be returned.
*/
inline u32 strtoul_prefix(const char *in, const char **out = 0)
{
	if (!in) {
		if (out)
			*out = in;
		return 0;
	}
	if ('0' == in[0])
		return ('x' == in[1] ? strtoul16(in + 2, out) : strtoul8(in + 1, out));
	return strtoul10(in, out);
}

//! Converts a sequence of digits into a whole positive floating point value.
/** Only digits 0 to 9 are parsed.  Parsing stops at any other character,
	including sign characters or a decimal point.
	\param in: the sequence of digits to convert.
	\param out: (optional) will be set to point at the first non-converted
	character.
	\return The whole positive floating point representation of the digit
	sequence.
*/
inline f32 strtof10(const char *in, const char **out = 0)
{
	if (!in) {
		if (out)
			*out = in;
		return 0.f;
	}

	const u32 MAX_SAFE_U32_VALUE = UINT_MAX / 10 - 10;
	u32 intValue = 0;

	// Use integer arithmetic for as long as possible, for speed
	// and precision.
	while ((*in >= '0') && (*in <= '9')) {
		// If it looks like we're going to overflow, bail out
		// now and start using floating point.
		if (intValue >= MAX_SAFE_U32_VALUE)
			break;

		intValue = (intValue * 10) + (*in - '0');
		++in;
	}

	f32 floatValue = (f32)intValue;

	// If there are any digits left to parse, then we need to use
	// floating point arithmetic from here.
	while ((*in >= '0') && (*in <= '9')) {
		floatValue = (floatValue * 10.f) + (f32)(*in - '0');
		++in;
		if (floatValue > FLT_MAX) // Just give up.
			break;
	}

	if (out)
		*out = in;

	return floatValue;
}

//! Provides a fast function for converting a string into a float.
/** This is not guaranteed to be as accurate as atof(), but is
	approximately 6 to 8 times as fast.
	\param[in] in The string to convert.
	\param[out] result The resultant float will be written here.
	\return Pointer to the first character in the string that wasn't used
	to create the float value.
*/
inline const char *fast_atof_move(const char *in, f32 &result)
{
	// Please run the regression test when making any modifications to this function.

	result = 0.f;
	if (!in)
		return 0;

	const bool negative = ('-' == *in);
	if (negative || ('+' == *in))
		++in;

	f32 value = strtof10(in, &in);

	if (*in == '.') {
		const char *afterDecimal = ++in;
		const f32 decimal = strtof10(in, &afterDecimal);
		const size_t numDecimals = afterDecimal - in;
		if (numDecimals < IRR_ATOF_TABLE_SIZE) {
			value += decimal * fast_atof_table[numDecimals];
		} else {
			value += decimal * std::pow(10.f, -(float)numDecimals);
		}
		in = afterDecimal;
	}

	if ('e' == *in || 'E' == *in) {
		++in;
		// Assume that the exponent is a whole number.
		// strtol10() will deal with both + and - signs,
		// but calculate as f32 to prevent overflow at FLT_MAX
		// Using pow with float cast instead of powf as otherwise accuracy decreases.
		value *= std::pow(10.f, (f32)strtol10(in, &in));
	}

	result = negative ? -value : value;
	return in;
}

//! Convert a string to a floating point number
/** \param floatAsString The string to convert.
	\param out Optional pointer to the first character in the string that
	wasn't used to create the float value.
	\result Float value parsed from the input string
*/
inline float fast_atof(const char *floatAsString, const char **out = 0)
{
	float ret;
	if (out)
		*out = fast_atof_move(floatAsString, ret);
	else
		fast_atof_move(floatAsString, ret);
	return ret;
}

} // end namespace core
