#include "asbrun_trace.hpp"

#include "asbrun_common.hpp"

namespace asbrun {

TraceFilter::TraceFilter(std::string_view categories) {
	_all = categories.empty() || categories == "all";
	while (!categories.empty()) {
		const size_t comma = categories.find(',');
		const std::string_view item = trim(categories.substr(0, comma));
		if (!item.empty())
			_categories.emplace(item);
		if (comma == std::string_view::npos)
			break;
		categories.remove_prefix(comma + 1);
	}
	_all = _all || _categories.contains("all");
}

bool TraceFilter::contains(std::string_view kind) const { return _all || _categories.contains(std::string(kind)); }

void write_trace_event(std::ostream& out, const asb::TraceEvent& event) {
	out << "{\"frame\":" << event.frame << ",\"kind\":\"" << asb::trace::quote(event.kind) << "\",\"event\":\""
		<< asb::trace::quote(event.event) << '"';
	if (!event.fields.empty())
		out << ',' << event.fields;
	out << "}\n";
}

} // namespace asbrun
