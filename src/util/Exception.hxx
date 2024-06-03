// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <exception>
#include <string>
#include <utility>

/**
 * Throws the specified exception.  There is an overload for
 * std::exception_ptr which throws the contained exception instead of
 * the std::exception_ptr itself.
 */
template<typename T>
[[noreturn]]
inline void
ThrowException(T &&t)
{
	throw std::forward<T>(t);
}

[[noreturn]]
inline void
ThrowException(std::exception_ptr ep)
{
	std::rethrow_exception(ep);
}

/**
 * Create a nested exception, wrapping #ep inside the
 * std::current_exception().
 */
template<typename T>
inline std::exception_ptr
NestCurrentException(T &&t) noexcept
{
	try {
		std::throw_with_nested(std::forward<T>(t));
	} catch (...) {
		return std::current_exception();
	}
}

/**
 * Create a nested exception, wrapping #ep inside (a copy of) #t.
 */
template<typename T>
inline std::exception_ptr
NestException(std::exception_ptr ep, T &&t) noexcept
{
	try {
		std::rethrow_exception(ep);
	} catch (...) {
		return NestCurrentException(std::forward<T>(t));
	}
}

/**
 * Find an instance of #T in the nested exception chain, and return a
 * pointer.  Returns nullptr if no such instance was found.
 */
template<typename T>
[[gnu::pure]]
inline const T *
FindNested(std::exception_ptr ep) noexcept
{
	try {
		std::rethrow_exception(ep);
	} catch (const T &t) {
		return &t;
	} catch (const std::nested_exception &ne) {
		return FindNested<T>(ne.nested_ptr());
	} catch (...) {
	}

	return nullptr;
}

/**
 * Find an instance of #T in the nested exception chain, and rethrow
 * it.  Does nothing if no such instance was found.
 */
template<typename T>
inline void
FindRetrowNested(std::exception_ptr ep)
{
	try {
		std::rethrow_exception(ep);
	} catch (const T &t) {
		throw;
	} catch (const std::nested_exception &ne) {
		FindRetrowNested<T>(ne.nested_ptr());
	} catch (...) {
	}
}

/**
 * Obtain the full concatenated message of an exception and its nested
 * chain.
 */
std::string
GetFullMessage(const std::exception &e,
	       const char *fallback="Unknown exception",
	       const char *separator="; ") noexcept;

/**
 * Obtain the full concatenated message of an exception and its nested
 * chain.
 */
std::string
GetFullMessage(std::exception_ptr ep,
	       const char *fallback="Unknown exception",
	       const char *separator="; ") noexcept;
