#include "acb.hpp"
#include "utf.hpp"

#include <iostream>
#include <istream>

void parse(const UTF& utf_table) {
	for (const auto& entry : utf_table) {
		if (!entry.name.ends_with("Table")) {
			continue;
		}
		if (entry.type_ != UTF::field::type::DATA_ARRAY) {
			continue;
		}
		auto data = entry.cast_at<std::vector<char>>(0).value_or(std::vector<char>{});
		const UTF subtable(data);
		if (!subtable.empty()) {
			std::cout << "-----> Reading sub table " << entry.name << " with " << entry.values.size() << " entries:\n";
			UTF::dump(subtable);
			std::cout << std::endl;
		}
	}
}

ACB::ACB(std::istream& is) {
	auto utf_table = UTF(is);
	// UTF::dump(utf_table);
	parse(utf_table);
}

ACB::ACB(std::istream&& is) {
	auto utf_table = UTF(std::move(is));
	// UTF::dump(utf_table);
	parse(utf_table);
}