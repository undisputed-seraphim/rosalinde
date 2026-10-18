#pragma once

#include <scarlett/trace.hpp>

#include <ostream>
#include <string>
#include <string_view>
#include <unordered_set>

namespace asbrun {

class TraceFilter {
public:
	explicit TraceFilter(std::string_view categories);

	bool contains(std::string_view kind) const;

private:
	bool _all = true;
	std::unordered_set<std::string> _categories;
};

void write_trace_event(std::ostream& out, const asb::TraceEvent& event);

} // namespace asbrun
