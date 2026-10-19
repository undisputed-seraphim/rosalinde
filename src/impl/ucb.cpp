#include "ucb.hpp"

#include <cstring>

namespace ucb {

std::optional<file> parse(std::span<const uint8_t> data) {
	if (data.empty() || data.size() % sizeof(cell) != 0)
		return std::nullopt;

	const size_t count = data.size() / sizeof(cell);
	file f;
	f.cells.resize(count);
	std::memcpy(f.cells.data(), data.data(), data.size());

	for (const auto& cell : f.cells) {
		if (cell.y != 0)
			break;
		f.width = cell.x + 1;
	}
	if (f.width == 0 || count % f.width != 0)
		return std::nullopt;
	f.height = static_cast<uint32_t>(count / f.width);

	for (size_t i = 0; i < f.cells.size(); ++i) {
		const auto& cell = f.cells[i];
		if (cell.x != i % f.width || cell.y != i / f.width)
			return std::nullopt;
	}

	return f;
}

std::optional<file> parse(std::istream& is) {
	std::vector<uint8_t> buffer;
	char c;
	while (is.get(c))
		buffer.push_back(static_cast<uint8_t>(c));
	return parse(std::span<const uint8_t>(buffer.data(), buffer.size()));
}

std::optional<file> parse(std::istream&& is) { return parse(is); }

} // namespace ucb
