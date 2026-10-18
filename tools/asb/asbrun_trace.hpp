#pragma once

#include <scarlett/asb.hpp>
#include <scarlett/trace.hpp>

#include <map>
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
void write_native_call(std::ostream& out, const asb::NativeCall& call);

struct CallStat {
	size_t calls = 0;
	size_t stubs = 0;
	long first = -1;
	long last = -1;
};

class CallCensus {
public:
	void observe(const asb::NativeCall& call);

	const std::map<std::string, CallStat>& calls() const { return _calls; }

private:
	std::map<std::string, CallStat> _calls;
};

std::string call_census_text(
	const asb::Module& module,
	const CallCensus& census,
	std::string_view source,
	std::string_view function);
std::string call_census_json(
	const asb::Module& module,
	const CallCensus& census,
	std::string_view source,
	std::string_view function);

} // namespace asbrun
