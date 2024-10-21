#include "acb.hpp"
#include "utf.hpp"

#include <iostream>
#include <istream>

static void parse(const UTF& utf_table, std::istream& is) {
	for (const auto& entry : utf_table) {
		if (!entry.name.ends_with("Table")) {
			continue;
		}
		if (entry.type_ != UTF::field::type::DATA_ARRAY) {
			continue;
		}
		const auto data = entry.cast_at<UTF::field::data_t>(0).value_or(UTF::field::data_t{0, 0});
		is.seekg(data.offset, std::ios::beg);
		const UTF subtable(is);
		if (!subtable.empty()) {
			std::cout << "-----> Reading sub table " << entry.name << " with " << entry.values.size() << " entries:\n";
			UTF::dump(subtable);
			std::cout << std::endl;
		}
	}
}

ACB::ACB(std::istream& is) {
	auto utf_table = UTF(is);
	parse(utf_table, is);
}

ACB::ACB(std::istream&& is) {
	auto utf_table = UTF(std::move(is));
	parse(utf_table, is);
}