#include "dump.hpp"

#include <criware/utf.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace dump {
namespace {

constexpr int max_utf_depth = 32;

const char* type_name(utf::type type) {
	switch (type) {
	case utf::type::UINT8:
		return "u8";
	case utf::type::INT8:
		return "s8";
	case utf::type::UINT16:
		return "u16";
	case utf::type::INT16:
		return "s16";
	case utf::type::UINT32:
		return "u32";
	case utf::type::INT32:
		return "s32";
	case utf::type::UINT64:
		return "u64";
	case utf::type::INT64:
		return "s64";
	case utf::type::FLOAT:
		return "f32";
	case utf::type::DOUBLE:
		return "f64";
	case utf::type::STRING:
		return "str";
	case utf::type::DATA:
		return "data";
	}
	return "?";
}

bool is_utf(std::span<const uint8_t> data) { return data.size() >= 4 && std::memcmp(data.data(), "@UTF", 4) == 0; }

std::string quote_text(std::string_view text) {
	std::string out;
	out.reserve(text.size() + 2);
	out += '"';
	for (char c : text) {
		switch (c) {
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			out += c;
			break;
		}
	}
	out += '"';
	return out;
}

std::string value_text(const utf::raw_table& table, const utf::value& value) {
	return std::visit(
		[&](const auto& item) -> std::string {
			using T = std::decay_t<decltype(item)>;
			if constexpr (std::is_same_v<T, std::monostate>) {
				return "(none)";
			} else if constexpr (std::is_same_v<T, uint64_t>) {
				return std::format("{}", item);
			} else if constexpr (std::is_same_v<T, int64_t>) {
				return std::format("{}", item);
			} else if constexpr (std::is_same_v<T, double>) {
				return std::format("{}", item);
			} else if constexpr (std::is_same_v<T, std::string_view>) {
				return quote_text(item);
			} else if constexpr (std::is_same_v<T, utf::blob_ref>) {
				const auto blob = table.blob(item);
				if (is_utf(blob))
					return std::format("@UTF({} bytes)", item.size);
				return std::format("data(offset={}, size={})", item.offset, item.size);
			}
		},
		value);
}

void write_json_value(json_writer& json, const utf::raw_table& table, const utf::value& value) {
	std::visit(
		[&](const auto& item) {
			using T = std::decay_t<decltype(item)>;
			if constexpr (std::is_same_v<T, std::monostate>) {
				json.null();
			} else if constexpr (
				std::is_same_v<T, uint64_t> || std::is_same_v<T, int64_t> || std::is_same_v<T, double>) {
				json.number(item);
			} else if constexpr (std::is_same_v<T, std::string_view>) {
				json.string(item);
			} else if constexpr (std::is_same_v<T, utf::blob_ref>) {
				const auto blob = table.blob(item);
				json.begin_object();
				json.field("offset", item.offset);
				json.field("size", item.size);
				json.field("utf", is_utf(blob));
				json.end_object();
			}
		},
		value);
}

struct nested_table {
	size_t row = 0;
	size_t column = 0;
	std::span<const uint8_t> data;
};

size_t row_limit(const options& opt) {
	if (opt.limit_set)
		return opt.limit;
	if (opt.full || opt.json)
		return std::numeric_limits<size_t>::max();
	return 10;
}

std::string nested_label(const std::string& label, const utf::raw_table& table, const nested_table& nested) {
	return std::format("{}.{}[{}]", label, table.column(nested.column).name, nested.row);
}

void dump_text(std::span<const uint8_t> data, const std::string& label, size_t limit, int depth) {
	if (depth > max_utf_depth) {
		std::cerr << "d: " << label << ": maximum UTF nesting depth exceeded\n";
		return;
	}

	const auto table = utf::raw_table::parse(data);
	const std::string indent(static_cast<size_t>(depth) * 2, ' ');
	std::cout << indent << label << ": rows=" << table.row_count() << " cols=" << table.column_count() << '\n';

	for (size_t i = 0; i < table.column_count(); ++i) {
		const auto& column = table.column(i);
		std::cout << indent << "  " << column.name << " (" << type_name(column.type_) << ") x"
				  << column.value_count(table.row_count());
		if (column.has_default)
			std::cout << " [default]";
		if (!column.valid)
			std::cout << " [invalid]";
		std::cout << '\n';
	}

	const size_t shown = std::min(limit, table.row_count());
	std::vector<nested_table> nested;
	for (size_t row = 0; row < shown; ++row) {
		std::cout << indent << "  [" << row << "]";
		for (size_t column = 0; column < table.column_count(); ++column) {
			const auto value = table.value_at(row, column);
			std::cout << " " << table.column(column).name << "=" << value_text(table, value);
			if (const auto* ref = std::get_if<utf::blob_ref>(&value)) {
				const auto blob = table.blob(*ref);
				if (is_utf(blob))
					nested.push_back({row, column, blob});
			}
		}
		std::cout << '\n';
	}
	if (shown < table.row_count())
		std::cout << indent << "  ... " << (table.row_count() - shown) << " more rows\n";

	for (const auto& item : nested) {
		const std::string child = nested_label(label, table, item);
		dump_text(item.data, child, limit, depth + 1);
	}
}

void write_table_json(
	json_writer& json,
	std::span<const uint8_t> data,
	const std::string& label,
	size_t limit,
	int depth) {
	if (depth > max_utf_depth) {
		json.begin_object();
		json.field("label", label);
		json.field("error", "maximum UTF nesting depth exceeded");
		json.end_object();
		return;
	}

	const auto table = utf::raw_table::parse(data);
	const size_t shown = std::min(limit, table.row_count());
	std::vector<nested_table> nested;

	for (size_t row = 0; row < shown; ++row) {
		for (size_t column = 0; column < table.column_count(); ++column) {
			const auto value = table.value_at(row, column);
			if (const auto* ref = std::get_if<utf::blob_ref>(&value)) {
				const auto blob = table.blob(*ref);
				if (is_utf(blob))
					nested.push_back({row, column, blob});
			}
		}
	}

	json.begin_object();
	json.field("label", label);
	json.field("row_count", table.row_count());
	json.field("column_count", table.column_count());
	json.begin_array("columns");
	for (size_t i = 0; i < table.column_count(); ++i) {
		const auto& column = table.column(i);
		json.begin_object();
		json.field("name", column.name);
		json.field("type", type_name(column.type_));
		json.field("value_count", column.value_count(table.row_count()));
		json.field("has_default", column.has_default);
		json.field("valid", column.valid);
		json.end_object();
	}
	json.end_array();

	json.begin_array("rows");
	for (size_t row = 0; row < shown; ++row) {
		json.begin_array();
		for (size_t column = 0; column < table.column_count(); ++column)
			write_json_value(json, table, table.value_at(row, column));
		json.end_array();
	}
	json.end_array();
	if (shown < table.row_count())
		json.field("truncated", true);

	if (!nested.empty()) {
		json.begin_array("nested");
		for (const auto& item : nested) {
			json.begin_object();
			json.field("column", table.column(item.column).name);
			json.field("row", item.row);
			json.key("table");
			write_table_json(json, item.data, nested_label(label, table, item), limit, depth + 1);
			json.end_object();
		}
		json.end_array();
	}
	json.end_object();
}

} // namespace

int dump_acb(const context& ctx, std::span<char> data) {
	utf::decipher(data);
	const std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t*>(data.data()), data.size());
	if (!is_utf(bytes)) {
		std::cerr << "d: " << ctx.path << ": not a UTF table\n";
		return 1;
	}

	const std::string type = lower_extension(ctx.path) == "acf" ? "acf" : "acb";
	const size_t limit = row_limit(ctx.opt);
	try {
		if (ctx.opt.json) {
			json_writer json(std::cout);
			json.begin_object();
			write_metadata_json(json, ctx, type);
			json.key("table");
			write_table_json(json, bytes, "root", limit, 0);
			json.end_object();
			std::cout << '\n';
		} else {
			write_metadata_text(ctx, type);
			dump_text(bytes, "root", limit, 0);
		}
	} catch (const std::exception& e) {
		std::cerr << "d: " << ctx.path << ": " << e.what() << '\n';
		return 1;
	}
	return 0;
}

} // namespace dump
