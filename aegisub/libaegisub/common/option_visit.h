// Copyright (c) 2010, Amar Takhar <verm@aegisub.org>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

/// @file option_visit.h
/// @brief Cajun JSON visitor to load config values.
/// @see option_visit.cpp
/// @ingroup libaegisub

#include "libaegisub/option.h"
#include "libaegisub/cajun/elements.h"
#include "libaegisub/cajun/visitor.h"

#include <memory>
#include <vector>

namespace agi {

DEFINE_BASE_EXCEPTION_NOINNER(OptionJsonValueError, Exception)
DEFINE_SIMPLE_EXCEPTION_NOINNER(OptionJsonValueArray, OptionJsonValueError, "options/value/array")
DEFINE_SIMPLE_EXCEPTION_NOINNER(OptionJsonValueSingle, OptionJsonValueError, "options/value")
DEFINE_SIMPLE_EXCEPTION_NOINNER(OptionJsonValueNull, OptionJsonValueError, "options/value")

class ConfigVisitor : public json::ConstVisitor {
	/// Option map being populated
	OptionValueMap &values;
	/// Option name prefix to add to read names
	std::string name;
	/// Log errors rather than throwing them, for when loading user config files
	/// (as a bad user config file shouldn't make the program fail to start)
	bool ignore_errors;
	/// Replace existing options rather than changing their value, so that the
	/// default value is changed to the new one
	bool replace;

	template<class ErrorType>
	void Error(const char *message);

	template<class OptionValueType, class ValueType>
	std::unique_ptr<OptionValue> ReadArray(json::Array const& src, std::string const& array_type, void (OptionValueType::*set_list)(const std::vector<ValueType>&));

	void AddOptionValue(std::unique_ptr<OptionValue>&& opt);
public:
	ConfigVisitor(OptionValueMap &val, const std::string &member_name, bool ignore_errors = false, bool replace = false);

	void Visit(const json::Array& array);
	void Visit(const json::Object& object);
	void Visit(const json::Integer& number);
	void Visit(const json::Double& number);
	void Visit(const json::String& string);
	void Visit(const json::Boolean& boolean);
	void Visit(const json::Null& null);
};

} // namespace agi
