// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <exception>
#include <string>


class BaseException : public std::exception
{
public:
	BaseException(const std::string &s) noexcept: m_s(s) {}
	~BaseException() throw() = default;

	virtual const char * what() const noexcept
	{
		return m_s.c_str();
	}
protected:
	std::string m_s;
};

class AlreadyExistsException : public BaseException {
public:
	AlreadyExistsException(const std::string &s): BaseException(s) {}
};

class VersionMismatchException : public BaseException {
public:
	VersionMismatchException(const std::string &s): BaseException(s) {}
};

class FileNotGoodException : public BaseException {
public:
	FileNotGoodException(const std::string &s): BaseException(s) {}
};

class DatabaseException : public BaseException {
public:
	DatabaseException(const std::string &s): BaseException(s) {}
};

class SerializationError : public BaseException {
public:
	SerializationError(const std::string &s): BaseException(s) {}
};

class PacketError : public BaseException {
public:
	PacketError(const std::string &s): BaseException(s) {}
};

class SettingNotFoundException : public BaseException {
public:
	SettingNotFoundException(const std::string &s): BaseException(s) {}
};

class ItemNotFoundException : public BaseException {
public:
	ItemNotFoundException(const std::string &s): BaseException(s) {}
};

class ServerError : public BaseException {
public:
	ServerError(const std::string &s): BaseException(s) {}
};

class ClientStateError : public BaseException {
public:
	ClientStateError(const std::string &s): BaseException(s) {}
};

class PrngException : public BaseException {
public:
	PrngException(const std::string &s): BaseException(s) {}
};

class ShaderException : public BaseException {
public:
	ShaderException(const std::string &s): BaseException(s) {}
};

class ModError : public BaseException {
public:
	ModError(const std::string &s): BaseException(s) {}
};


/*
	Some "old-style" interrupts:
*/

class InvalidNoiseParamsException : public BaseException {
public:
	InvalidNoiseParamsException():
		BaseException("One or more noise parameters were invalid or require "
			"too much memory")
	{}

	InvalidNoiseParamsException(const std::string &s):
		BaseException(s)
	{}
};

class InvalidPositionException : public BaseException
{
public:
	InvalidPositionException():
		BaseException("Somebody tried to get/set something in a nonexistent position.")
	{}
	InvalidPositionException(const std::string &s):
		BaseException(s)
	{}
};
