#pragma once

#include <angelscript.h>
#include <scriptarray/scriptarray.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace asb {

class NativeArgs {
public:
	explicit NativeArgs(asIScriptGeneric* gen)
		: _gen(gen) {}

	asIScriptGeneric* generic() const { return _gen; }

	std::string str(int index) const {
		auto* value = static_cast<const std::string*>(_gen->GetArgAddress(index));
		return value ? *value : std::string();
	}

	int i32(int index) const { return static_cast<int>(_gen->GetArgDWord(index)); }

	float f32(int index) const { return _gen->GetArgFloat(index); }

	void vec2(int index, float* out) const { read_vec(index, out, 2); }

	void vec3(int index, float* out) const { read_vec(index, out, 3); }

	void color(int index, float* out) const {
		auto* value = static_cast<const unsigned char*>(_gen->GetArgAddress(index));
		for (int i = 0; i < 4; ++i)
			out[i] = (value ? value[i] : 0.0f) / 255.0f;
	}

	std::vector<int> ints(int index) const {
		std::vector<int> out;
		auto* array = static_cast<CScriptArray*>(_gen->GetArgObject(index));
		if (!array)
			return out;
		const asUINT count = std::min<asUINT>(array->GetSize(), 64);
		out.reserve(count);
		for (asUINT i = 0; i < count; ++i)
			out.push_back(*static_cast<const int*>(array->At(i)));
		return out;
	}

private:
	void read_vec(int index, float* out, int count) const {
		auto* value = static_cast<const float*>(_gen->GetArgAddress(index));
		for (int i = 0; i < count; ++i)
			out[i] = value ? value[i] : 0.0f;
	}

	asIScriptGeneric* _gen;
};

} // namespace asb
