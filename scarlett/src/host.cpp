#include <scarlett/host.hpp>
#include <scarlett/runtime.hpp>

#include "registry.hpp"

#include <angelscript.h>
#include <scriptarray/scriptarray.h>
#include <scriptstdstring/scriptstdstring.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace asb {

namespace {

class FileStream : public asIBinaryStream {
public:
	std::span<const uint8_t> data;
	asUINT pos = 0;
	explicit FileStream(std::span<const uint8_t> bytes)
		: data(bytes) {}

	int Read(void* ptr, asUINT size) override {
		if (pos + size > data.size())
			return -1;
		std::memcpy(ptr, data.data() + pos, size);
		pos += size;
		return 0;
	}
	int Write(const void*, asUINT) override { return -1; }
};

void MessageCallback(const asSMessageInfo* msg, void*) {
	const char* sev = "?";
	switch (msg->type) {
	case asMSGTYPE_ERROR:
		sev = "error";
		break;
	case asMSGTYPE_WARNING:
		sev = "warn";
		break;
	case asMSGTYPE_INFORMATION:
		sev = "info";
		break;
	}
	std::fprintf(
		stderr, "[as %s] %s:%d:%d %s\n", sev, msg->section ? msg->section : "", msg->row, msg->col, msg->message);
}

std::string arg_to_string(asIScriptGeneric* gen, asUINT i) {
	int t = gen->GetArgTypeId(i);
	char buf[128];
	if (t == asTYPEID_BOOL || (t >= asTYPEID_INT8 && t <= asTYPEID_UINT32))
		std::snprintf(buf, sizeof(buf), "%d", gen->GetArgDWord(i));
	else if (t == asTYPEID_INT64 || t == asTYPEID_UINT64)
		std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(gen->GetArgQWord(i)));
	else if (t == asTYPEID_FLOAT)
		std::snprintf(buf, sizeof(buf), "%g", gen->GetArgFloat(i));
	else if (t == asTYPEID_DOUBLE)
		std::snprintf(buf, sizeof(buf), "%g", gen->GetArgDouble(i));
	else if (t & asTYPEID_OBJHANDLE)
		std::snprintf(buf, sizeof(buf), "@%p", gen->GetArgAddress(i));
	else {
		asITypeInfo* ti = gen->GetEngine()->GetTypeInfoById(t);
		const char* tn = ti ? ti->GetName() : "";
		std::string_view tns = tn ? tn : "";
		if (tns == "string") {
			auto* s = static_cast<const std::string*>(gen->GetArgAddress(i));
			std::snprintf(buf, sizeof(buf), "\"%s\"", s ? s->c_str() : "");
		} else if (tns == "FnVector2" || tns == "FnVector3" || tns == "FnVector4") {
			auto* f = static_cast<const float*>(gen->GetArgAddress(i));
			int n = tns == "FnVector2" ? 2 : (tns == "FnVector3" ? 3 : 4);
			std::string v = "(";
			for (int k = 0; k < n; ++k) {
				char b[32];
				std::snprintf(b, sizeof(b), k ? ",%g" : "%g", f ? f[k] : 0.0f);
				v += b;
			}
			v += ")";
			std::snprintf(buf, sizeof(buf), "%s", v.c_str());
		} else
			std::snprintf(buf, sizeof(buf), "obj@%p", gen->GetArgAddress(i));
	}
	return buf;
}

// RAII: register a type/function inside an (optional) namespace, then restore.
struct NSGuard {
	asIScriptEngine* e;
	NSGuard(asIScriptEngine* eng, const std::string& ns)
		: e(eng) {
		e->SetDefaultNamespace(ns.empty() ? "" : ns.c_str());
	}
	~NSGuard() { e->SetDefaultNamespace(""); }
	NSGuard(const NSGuard&) = delete;
};

std::function<void(const std::string&)>* g_call_log = nullptr;
Registry* g_scene_registry = nullptr;
NativeCallCallback* g_native_call = nullptr;
bool g_call_silent = false;

std::string function_name(asIScriptFunction* fn) {
	std::string name;
	if (fn && *fn->GetNamespace()) {
		name += fn->GetNamespace();
		name += "::";
	}
	if (fn)
		name += fn->GetName();
	return name;
}

std::string active_caller_name() {
	asIScriptContext* ctx = asGetActiveContext();
	if (!ctx || ctx->GetCallstackSize() == 0)
		return {};
	return function_name(ctx->GetFunction(0));
}

std::string return_type_name(asIScriptGeneric* gen, int type_id) {
	if (type_id == asTYPEID_VOID)
		return {};
	const char* declaration = gen->GetEngine()->GetTypeDeclaration(type_id, true);
	return declaration ? declaration : "?";
}

std::string return_value_text(asIScriptGeneric* gen, int type_id) {
	if (type_id == asTYPEID_VOID)
		return {};
	void* location = gen->GetAddressOfReturnLocation();
	if (!location)
		return "null";

	char buffer[64];
	if (type_id & asTYPEID_OBJHANDLE) {
		void* value = nullptr;
		std::memcpy(&value, location, sizeof(value));
		if (!value)
			return "null";
		std::snprintf(buffer, sizeof(buffer), "@%p", value);
		return buffer;
	}
	switch (type_id) {
	case asTYPEID_BOOL: {
		uint8_t value = 0;
		std::memcpy(&value, location, sizeof(value));
		return value ? "true" : "false";
	}
	case asTYPEID_INT8: {
		int8_t value = 0;
		std::memcpy(&value, location, sizeof(value));
		std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(value));
		return buffer;
	}
	case asTYPEID_INT16: {
		int16_t value = 0;
		std::memcpy(&value, location, sizeof(value));
		std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(value));
		return buffer;
	}
	case asTYPEID_INT32: {
		int32_t value = 0;
		std::memcpy(&value, location, sizeof(value));
		std::snprintf(buffer, sizeof(buffer), "%d", value);
		return buffer;
	}
	case asTYPEID_UINT8: {
		uint8_t value = 0;
		std::memcpy(&value, location, sizeof(value));
		std::snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned>(value));
		return buffer;
	}
	case asTYPEID_UINT16: {
		uint16_t value = 0;
		std::memcpy(&value, location, sizeof(value));
		std::snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned>(value));
		return buffer;
	}
	case asTYPEID_UINT32: {
		uint32_t value = 0;
		std::memcpy(&value, location, sizeof(value));
		std::snprintf(buffer, sizeof(buffer), "%u", value);
		return buffer;
	}
	case asTYPEID_INT64: {
		int64_t value = 0;
		std::memcpy(&value, location, sizeof(value));
		std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
		return buffer;
	}
	case asTYPEID_UINT64: {
		uint64_t value = 0;
		std::memcpy(&value, location, sizeof(value));
		std::snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
		return buffer;
	}
	case asTYPEID_FLOAT: {
		float value = 0.0f;
		std::memcpy(&value, location, sizeof(value));
		std::snprintf(buffer, sizeof(buffer), "%g", value);
		return buffer;
	}
	case asTYPEID_DOUBLE: {
		double value = 0.0;
		std::memcpy(&value, location, sizeof(value));
		std::snprintf(buffer, sizeof(buffer), "%g", value);
		return buffer;
	}
	default:
		break;
	}

	if (asITypeInfo* type = gen->GetEngine()->GetTypeInfoById(type_id)) {
		const std::string_view name = type->GetName() ? type->GetName() : "";
		if (name == "string") {
			auto* value = static_cast<const std::string*>(location);
			return value ? *value : std::string();
		}
	}
	std::snprintf(buffer, sizeof(buffer), "obj@%p", location);
	return buffer;
}

void set_default_return(asIScriptGeneric* gen, asIScriptFunction* fn) {
	const int type_id = fn->GetReturnTypeId();
	if (type_id == asTYPEID_VOID)
		return;
	if (type_id & asTYPEID_OBJHANDLE) {
		gen->SetReturnAddress(nullptr);
		return;
	}
	switch (type_id) {
	case asTYPEID_BOOL:
	case asTYPEID_INT8:
	case asTYPEID_INT16:
	case asTYPEID_INT32:
	case asTYPEID_UINT8:
	case asTYPEID_UINT16:
	case asTYPEID_UINT32:
		gen->SetReturnDWord(0);
		break;
	case asTYPEID_INT64:
	case asTYPEID_UINT64:
		gen->SetReturnQWord(0);
		break;
	case asTYPEID_FLOAT:
		gen->SetReturnFloat(0.0f);
		break;
	case asTYPEID_DOUBLE:
		gen->SetReturnDouble(0.0);
		break;
	default:
		break;
	}
}

void emit_native_call(asIScriptGeneric* gen, asIScriptFunction* fn, const std::string& call, bool handled) {
	if (!g_native_call)
		return;
	NativeCall event;
	event.frame = g_scene_registry ? g_scene_registry->frame : 0;
	event.name = function_name(fn);
	event.call = call;
	event.caller = active_caller_name();
	event.handled = handled;
	const int type_id = fn->GetReturnTypeId();
	if (type_id != asTYPEID_VOID) {
		event.return_type = return_type_name(gen, type_id);
		event.result = return_value_text(gen, type_id);
	}
	(*g_native_call)(event);
}

// True if the type already exposes a method/factory/behaviour with this
// declaration. Registering a duplicate makes the engine set its `configFailed`
// flag, which then stops it preparing the system-function interfaces (and thus
// breaks value-returning natives).
bool decl_exists(asITypeInfo* ti, const std::string& decl) {
	if (!ti)
		return true;
	if (ti->GetMethodByDecl(decl.c_str()))
		return true;
	if (ti->GetFactoryByDecl(decl.c_str()))
		return true;
	return false;
}

std::string params_of(const std::string& d) {
	auto a = d.find('(');
	auto b = d.rfind(')');
	if (a == std::string::npos || b == std::string::npos || b < a)
		return d;
	return d.substr(a, b - a + 1);
}

// Behaviour declarations use the type name (e.g. `string(const string&in)`),
// not our `void f(...)` spelling, so match constructors/destructors/factories by
// behaviour kind + parameter list.
bool behaviour_exists(asITypeInfo* ti, asEBehaviours beh, const std::string& decl) {
	if (!ti)
		return true;
	std::string want = params_of(decl);
	for (asUINT i = 0;; ++i) {
		asEBehaviours b;
		asIScriptFunction* f = ti->GetBehaviourByIndex(i, &b);
		if (!f)
			break;
		if (b == beh && params_of(f->GetDeclaration(false)) == want)
			return true;
	}
	for (asUINT i = 0; i < ti->GetFactoryCount(); ++i) {
		asIScriptFunction* f = ti->GetFactoryByIndex(i);
		if (f && (beh == asBEHAVE_FACTORY || beh == asBEHAVE_LIST_FACTORY) &&
			params_of(f->GetDeclaration(false)) == want)
			return true;
	}
	return false;
}

void GenericStub(asIScriptGeneric* gen) {
	asIScriptFunction* fn = gen->GetFunction();

	const bool print_calls = !g_call_log && !g_call_silent;
	const bool wants_call = g_call_log || g_native_call || print_calls;
	std::string call;
	if (wants_call) {
		call = function_name(fn);
		call += "(";
		for (asUINT i = 0; i < gen->GetArgCount(); ++i) {
			if (i)
				call += ", ";
			call += arg_to_string(gen, i);
		}
		call += ")";
	}
	if (g_call_log)
		(*g_call_log)("call " + call);
	else if (print_calls)
		std::printf("call %s\n", call.c_str());

	// Let the recording shim model scene-relevant calls (and the scheduler hook)
	// before falling back to a benign no-op return.
	const bool handled = g_scene_registry && g_scene_registry->dispatch(gen);
	if (!handled) {
		set_default_return(gen, fn);

		// Value types are returned into caller-allocated storage; if we leave it
		// untouched the VM later destroys/copies uninitialised memory (e.g. a bogus
		// `string`). Default-construct a string and zero anything else.
		const int rt = fn->GetReturnTypeId();
		if (rt & asTYPEID_MASK_OBJECT) {
			asITypeInfo* ti = gen->GetEngine()->GetTypeInfoById(rt);
			void* loc = gen->GetAddressOfReturnLocation();
			if (loc && ti) {
				if (std::strcmp(ti->GetName(), "string") == 0)
					new (loc) std::string();
				else if (int size = ti->GetSize(); size > 0)
					std::memset(loc, 0, static_cast<size_t>(size));
			}
		}
	}

	emit_native_call(gen, fn, call, handled);
}

void AbortCallback(asIScriptContext* ctx, void* param) {
	unsigned long* steps = static_cast<unsigned long*>(param);
	if (++*steps > 20000000ul) {
		std::fprintf(stderr, "[as] step limit reached, aborting\n");
		ctx->Abort();
	}
}

} // namespace

struct Host::Impl {
	asIScriptEngine* engine = nullptr;
	asIScriptModule* module = nullptr;
	std::vector<std::unique_ptr<std::vector<char>>> prop_storage;
	std::unordered_map<std::string, int> known_size;
	std::unordered_map<std::string, bool> registered_types;
	std::function<void(const std::string&)> call_log;
	bool call_log_set = false;
	TraceCallback trace;
	NativeCallCallback native_call;
	std::function<long(const std::string&, int)> track_frames;
	TrackInfoProvider track_info;
	std::function<int(const std::string&, const std::string&)> resolve_track;
	std::function<std::string(const std::string&)> facility_name;
	const CharTable* chars = nullptr;
	const CharaSetTable* chara_sets = nullptr;
	const StageTable* stages = nullptr;
	const DropTables* drops = nullptr;
	std::function<int(int)> sound_length;
};

Host::Host()
	: _impl(new Impl()) {
	_impl->engine = asCreateScriptEngine();
	_impl->engine->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
	// Let the engine initialise the module's global variables after load so that
	// global string/array constants hold their real values.
	_impl->engine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, 1);

	RegisterStdString(_impl->engine);
	RegisterScriptArray(_impl->engine, true);
	// The project's bytecode was built against an add-on whose array factory
	// uses `int` for the length parameter, while this add-on uses `uint`. The
	// reader matches the stored factory stub, so register int-typed variants.
	_impl->engine->RegisterObjectBehaviour(
		"array<T>", asBEHAVE_FACTORY, "array<T>@ f(int&in, int)", asFUNCTION(GenericStub), asCALL_GENERIC);
	_impl->engine->RegisterObjectBehaviour(
		"array<T>", asBEHAVE_FACTORY, "array<T>@ f(int&in, int, const T &in)", asFUNCTION(GenericStub), asCALL_GENERIC);
}

Host::~Host() {
	if (_impl->engine)
		_impl->engine->ShutDownAndRelease();
	delete _impl;
}

void Host::set_call_log(std::function<void(const std::string&)> cb) {
	_impl->call_log = std::move(cb);
	_impl->call_log_set = true;
}

void Host::set_trace(TraceCallback cb) { _impl->trace = std::move(cb); }

void Host::set_native_call(NativeCallCallback cb) { _impl->native_call = std::move(cb); }

void Host::register_module(const Module& mod) {
	asIScriptEngine* engine = _impl->engine;

	// Value types the engine exposes (known sizes + the register flags the game
	// used). The collection flags matter: the VM's stack layout is derived from
	// them, so they must match the module's original registration.
	struct VT {
		const char* name;
		int size;
		asDWORD flags;
	};
	constexpr asDWORD vec_flags = asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CDAK | asOBJ_APP_CLASS_ALLFLOATS;
	constexpr asDWORD color_flags = asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLINTS;
	const VT value_types[] = {
		{"FnVector2", 12, vec_flags},
		{"FnVector3", 12, vec_flags},
		{"FnVector4", 16, vec_flags},
		{"FnColor", 4, color_flags},
	};
	for (const auto& vt : value_types) {
		int r = engine->RegisterObjectType(vt.name, vt.size, vt.flags);
		if (r < 0)
			std::fprintf(stderr, "[as] RegisterObjectType(%s) r=%d\n", vt.name, r);
		_impl->known_size[vt.name] = vt.size;
		_impl->registered_types[vt.name] = true;
	}
	_impl->registered_types["string"] = true; // registered by the scriptstdstring add-on
	_impl->registered_types["array"] = true;  // registered by the scriptarray add-on

	// Classify the module's own object types by how signatures use them: a type
	// used via @ (or produced by a factory) is a reference type; one used by
	// value (or const-ref) is a value type. Reference evidence wins.
	std::unordered_set<std::string> value_names;
	std::unordered_set<std::string> ref_names;
	std::unordered_map<std::string, TypeInfoPtr> type_ptr;
	// Reference object types registered by the engine (from the exe's
	// RegisterClass_* setup); these are never value types.
	for (const char* n : {"UcStObject", "UcStChaseParam", "UcMsgBalloonParam", "UcSpriteTextParam"})
		ref_names.insert(n);
	auto mark_value = [&](const DataType& dt) {
		if (dt.token == ttIdentifier && dt.type_info) {
			std::string n = dt.type_info->format();
			if (n.empty() || n.find('<') != std::string::npos || n.find('[') != std::string::npos)
				return;
			type_ptr[n] = dt.type_info;
			if (dt.is_object_handle)
				ref_names.insert(n);
			else
				value_names.insert(n);
		}
	};
	auto scan_sig = [&](const Signature& sig) {
		mark_value(sig.return_type);
		for (const auto& p : sig.params)
			mark_value(p);
	};
	for (const auto& f : mod.used_functions)
		if (f)
			scan_sig(f->sig);
	for (const auto& f : mod.functions)
		if (f)
			scan_sig(f->sig);

	auto register_named = [&](const std::string& key, int size, asDWORD flags) {
		auto it = type_ptr.find(key);
		TypeInfoPtr t = it != type_ptr.end() ? it->second : nullptr;
		NSGuard g(engine, t ? t->name_space : std::string());
		std::string nm = (t && !t->name.empty()) ? t->name : key;
		int r = engine->RegisterObjectType(nm.c_str(), size, flags);
		if (r < 0)
			std::fprintf(stderr, "[as] RegisterObjectType(%s) r=%d\n", key.c_str(), r);
		_impl->registered_types[key] = true;
	};

	for (const auto& n : value_names) {
		if (_impl->registered_types.count(n))
			continue;
		if (ref_names.count(n))
			register_named(n, 0, asOBJ_REF | asOBJ_NOCOUNT);
		else
			register_named(
				n, _impl->known_size.count(n) ? _impl->known_size[n] : 32, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS);
	}
	for (const auto& n : ref_names) {
		if (_impl->registered_types.count(n))
			continue;
		register_named(n, 0, asOBJ_REF | asOBJ_NOCOUNT);
	}

	// Remaining engine object types the module refers to -> opaque references.
	for (const auto& t : mod.used_types) {
		if (!t || t->kind != TypeInfo::Object)
			continue;
		std::string name = t->format();
		if (name.empty() || name.find('<') != std::string::npos || name.find('[') != std::string::npos ||
			_impl->registered_types.count(name))
			continue;
		NSGuard g(engine, t->name_space);
		int r = engine->RegisterObjectType(t->name.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT);
		if (r < 0)
			std::fprintf(stderr, "[as] RegisterObjectType(%s) r=%d\n", name.c_str(), r);
		_impl->registered_types[name] = true;
	}

	// Register every native (kind 'a') the module imports, as generic stubs:
	// global functions on the engine, object methods on their (opaque) types.
	unsigned registered = 0, skipped = 0;
	for (size_t i = 0; i < mod.used_functions.size(); ++i) {
		char kind = i < mod.used_function_kind.size() ? mod.used_function_kind[i] : '?';
		if (kind != 'a' || !mod.used_functions[i])
			continue;
		const Signature& sig = mod.used_functions[i]->sig;
		int r;
		bool is_beh = sig.name.rfind("$beh", 0) == 0 || sig.name == "$fact";
		if (is_beh && sig.name.size() >= 4) {
			// The serialized behaviour digit is the asEBehaviours value: 0 =
			// constructor, 2 = destructor, 3 = factory, 4 = list factory (see
			// AngelScript as_restore.cpp). `$fact` is a factory too.
			// Constructors/destructors target the method's object type;
			// factories target the type they produce.
			asEBehaviours beh = sig.name == "$fact" ? asBEHAVE_FACTORY : static_cast<asEBehaviours>(sig.name[4] - '0');
			TypeInfoPtr target;
			if (beh == asBEHAVE_FACTORY || beh == asBEHAVE_LIST_FACTORY)
				target = sig.return_type.type_info ? sig.return_type.type_info : sig.object_type;
			else
				target = sig.object_type;
			if (!target) {
				++skipped;
				continue;
			}
			std::string obj = target->format();
			if (obj.empty() || obj[0] == '$' || obj.find('[') != std::string::npos ||
				obj.find('<') != std::string::npos) {
				++skipped;
				continue;
			}
			std::string decl = sig.return_type.as_decl() + " f(";
			for (size_t k = 0; k < sig.params.size(); ++k) {
				if (k)
					decl += ", ";
				decl += sig.params[k].as_decl();
				if (sig.params[k].is_reference && k < sig.inout_flags.size()) {
					switch (sig.inout_flags[k]) {
					case 1:
						decl += "in";
						break;
					case 2:
						decl += "out";
						break;
					case 3:
						decl += "inout";
						break;
					default:
						break;
					}
				}
			}
			decl += ")";
			if (decl.find('[') != std::string::npos) { // default-array types confuse the parser
				++skipped;
				continue;
			}
			NSGuard g(engine, target->name_space);
			if (behaviour_exists(engine->GetTypeInfoByName(target->name.c_str()), beh, decl)) {
				++skipped;
				continue;
			}
			r = engine->RegisterObjectBehaviour(
				target->name.c_str(), beh, decl.c_str(), asFUNCTION(GenericStub), asCALL_GENERIC);
			if (r < 0) {
				std::fprintf(stderr, "[as] RegisterObjectBehaviour failed r=%d: %s %s\n", r, obj.c_str(), decl.c_str());
				++skipped;
			} else {
				++registered;
			}
			continue;
		}
		if (!sig.name.empty() && sig.name[0] == '$') { // other internals ($list, ...)
			++skipped;
			continue;
		}
		if (sig.object_type) {
			TypeInfoPtr ot = sig.object_type;
			std::string obj = ot->format();
			if (obj.empty() || obj[0] == '$' || obj.find('[') != std::string::npos ||
				obj.find('<') != std::string::npos) {
				++skipped;
				continue;
			}
			std::string decl = sig.bare_decl(false);
			if (decl.find('[') != std::string::npos) {
				++skipped;
				continue;
			}
			NSGuard g(engine, ot->name_space);
			if (decl_exists(engine->GetTypeInfoByName(ot->name.c_str()), decl)) {
				++skipped;
				continue;
			}
			r = engine->RegisterObjectMethod(ot->name.c_str(), decl.c_str(), asFUNCTION(GenericStub), asCALL_GENERIC);
			if (r < 0) {
				std::fprintf(stderr, "[as] RegisterObjectMethod failed r=%d: %s::%s\n", r, obj.c_str(), decl.c_str());
				++skipped;
			} else {
				++registered;
			}
		} else {
			std::string decl = sig.bare_decl(true);
			if (decl.find('[') != std::string::npos) {
				++skipped;
				continue;
			}
			if (engine->GetGlobalFunctionByDecl(decl.c_str())) {
				++skipped;
				continue;
			}
			r = engine->RegisterGlobalFunction(decl.c_str(), asFUNCTION(GenericStub), asCALL_GENERIC);
			if (r < 0) {
				std::fprintf(stderr, "[as] RegisterGlobalFunction failed r=%d: %s\n", r, decl.c_str());
				++skipped;
			} else {
				++registered;
			}
		}
	}
	std::fprintf(stderr, "[as] registered %u native(s), skipped %u\n", registered, skipped);

	// Engine global properties the module reads (constants such as
	// FnColor::BlackA0). Storage must outlive the engine.
	for (const auto& g : mod.used_globals) {
		if (g.module_prop)
			continue; // module globals are created by the reader
		int size = 32;
		if (g.type.type_info) {
			std::string tn = g.type.type_info->format();
			if (_impl->known_size.count(tn))
				size = _impl->known_size[tn];
		}
		auto buf = std::make_unique<std::vector<char>>(static_cast<size_t>(size), 0);
		std::string decl = g.type.as_decl() + " " + g.name;
		if (decl.find('[') != std::string::npos || decl.find('<') != std::string::npos)
			continue; // array/template type properties
		if (!g.name_space.empty())
			engine->SetDefaultNamespace(g.name_space.c_str());
		int r = engine->RegisterGlobalProperty(decl.c_str(), buf->data());
		if (!g.name_space.empty())
			engine->SetDefaultNamespace("");
		if (r < 0)
			std::fprintf(
				stderr, "[as] RegisterGlobalProperty failed r=%d: %s (ns %s)\n", r, decl.c_str(), g.name_space.c_str());
		_impl->prop_storage.push_back(std::move(buf));
	}

	// Object member properties the module accesses. The reader matches these by
	// name only, so a placeholder type/offset is enough to satisfy resolution.
	unsigned obj_props = 0;
	for (const auto& op : mod.used_obj_props) {
		if (!op.obj)
			continue;
		std::string obj = op.obj->format();
		if (obj.empty() || obj[0] == '$' || obj.find('<') != std::string::npos || obj.find('[') != std::string::npos)
			continue;
		std::string decl = "int " + op.name;
		if (engine->RegisterObjectProperty(obj.c_str(), decl.c_str(), 0) >= 0)
			++obj_props;
	}
	std::fprintf(stderr, "[as] registered %u object propert(ies)\n", obj_props);
}

bool Host::load(std::span<const uint8_t> bytecode) {
	_impl->module = _impl->engine->GetModule("loaded", asGM_ALWAYS_CREATE);
	FileStream in(bytecode);
	bool stripped = false;
	int r = _impl->module->LoadByteCode(&in, &stripped);
	std::fprintf(stderr, "LoadByteCode r=%d stripped=%d consumed=%u/%zu\n", r, stripped, in.pos, bytecode.size());
	return r == 0;
}

bool Host::compile(const std::string& source) {
	_impl->module = _impl->engine->GetModule("test", asGM_ALWAYS_CREATE);
	_impl->module->AddScriptSection("test", source.c_str(), source.size());
	int r = _impl->module->Build();
	std::fprintf(stderr, "Build r=%d\n", r);
	return r >= 0;
}

asIScriptModule* Host::module() const { return _impl->module; }

asIScriptFunction* Host::find_function(const std::vector<std::string>& names) const {
	if (!_impl->module)
		return nullptr;
	for (const auto& n : names) {
		if (auto* fn = _impl->module->GetFunctionByName(n.c_str()))
			return fn;
	}
	return nullptr;
}

int Host::execute(asIScriptFunction* fn) {
	g_call_log = _impl->call_log ? &_impl->call_log : nullptr;
	g_native_call = _impl->native_call ? &_impl->native_call : nullptr;
	g_call_silent = _impl->call_log_set;
	asIScriptContext* ctx = _impl->engine->CreateContext();
	ctx->Prepare(fn);
	unsigned long steps = 0;
	ctx->SetLineCallback(asFUNCTION(AbortCallback), &steps, asCALL_CDECL);
	int er = ctx->Execute();
	std::fprintf(stderr, "[as] Execute r=%d (%s)\n", er, er == asEXECUTION_FINISHED ? "finished" : "other");
	if (er == asEXECUTION_EXCEPTION) {
		std::fprintf(stderr, "[as] exception: %s\n", ctx->GetExceptionString());
		if (asIScriptFunction* ef = ctx->GetExceptionFunction())
			std::fprintf(stderr, "[as]   in %s line %d\n", ef->GetDeclaration(), ctx->GetExceptionLineNumber());
	}
	ctx->Release();
	g_call_log = nullptr;
	g_native_call = nullptr;
	g_call_silent = false;
	return er;
}

void Host::set_char_table(const CharTable& chars) { _impl->chars = &chars; }

void Host::set_chara_set_table(const CharaSetTable& sets) { _impl->chara_sets = &sets; }

void Host::set_track_frames(std::function<long(const std::string&, int)> fn) { _impl->track_frames = std::move(fn); }

void Host::set_track_info(TrackInfoProvider fn) { _impl->track_info = std::move(fn); }

void Host::set_resolve_track(std::function<int(const std::string&, const std::string&)> fn) {
	_impl->resolve_track = std::move(fn);
}

void Host::set_facility_name(std::function<std::string(const std::string&)> fn) {
	_impl->facility_name = std::move(fn);
}

void Host::set_stage_table(const StageTable& stages) { _impl->stages = &stages; }

void Host::set_drop_tables(const DropTables& drops) { _impl->drops = &drops; }

void Host::set_sound_length(std::function<int(int)> fn) { _impl->sound_length = std::move(fn); }

struct RuntimeSession::Impl {
	asIScriptEngine* engine = nullptr;
	asIScriptModule* module = nullptr;
	asIScriptContext* ctx = nullptr;
	Registry reg;
	std::function<void(const std::string&)> call_log;
	bool call_log_set = false;
	TraceCallback trace;
	NativeCallCallback native_call;
	long wake = 0;
	unsigned long steps = 0;
};

RuntimeSession::RuntimeSession(Host& host, std::string source, SaveState* save)
	: _host(host)
	, _source(std::move(source))
	, _impl(std::make_unique<Impl>()) {
	auto* h = host._impl;
	_impl->engine = h->engine;
	_impl->module = h->module;
	_impl->call_log = h->call_log;
	_impl->call_log_set = h->call_log_set;
	_impl->trace = h->trace;
	_impl->native_call = h->native_call;
	if (h->chars)
		_impl->reg.set_char_table(h->chars);
	if (h->chara_sets)
		_impl->reg.set_chara_set_table(h->chara_sets);
	if (h->track_frames)
		_impl->reg.set_track_frames(h->track_frames);
	if (h->track_info)
		_impl->reg.set_track_info(h->track_info);
	if (h->resolve_track)
		_impl->reg.set_resolve_track(h->resolve_track);
	if (h->facility_name)
		_impl->reg.set_facility_name(h->facility_name);
	if (h->stages)
		_impl->reg.set_stage_table(*h->stages);
	if (h->drops)
		_impl->reg.set_drop_tables(*h->drops);
	if (h->sound_length)
		_impl->reg.set_sound_length(h->sound_length);
	_impl->reg.set_save_state(save);
}

RuntimeSession::~RuntimeSession() {
	if (_impl && _impl->ctx)
		_impl->ctx->Release();
}

void RuntimeSession::set_scene_mode(SceneMode mode) { _impl->reg.set_scene_mode(mode); }

SceneMode RuntimeSession::scene_mode() const { return _impl->reg.scene_mode(); }

SaveState& RuntimeSession::save() { return _impl->reg.save(); }

const SaveState& RuntimeSession::save() const { return _impl->reg.save(); }

bool RuntimeSession::start(asIScriptFunction* fn, asIScriptFunction* prelude) {
	if (!fn) {
		_error = "null entry function";
		return false;
	}
	if (_impl->ctx) {
		_impl->ctx->Release();
		_impl->ctx = nullptr;
	}
	_fn = fn;
	_impl->ctx = _impl->engine->CreateContext();
	if (!_impl->ctx) {
		_error = "could not create script context";
		return false;
	}
	_impl->reg.reset(_source, fn->GetName());
	_impl->reg.set_trace(_impl->trace);
	_impl->reg.set_module(_impl->module);
	_impl->reg.set_frame(0);
	_impl->wake = 0;
	_impl->steps = 0;
	_error.clear();
	if (prelude && !run_prelude(prelude))
		return false;
	_impl->ctx->Prepare(fn);
	_impl->ctx->SetLineCallback(asFUNCTION(AbortCallback), &_impl->steps, asCALL_CDECL);
	_frame = 0;
	_started = true;
	_finished = false;
	_limit = false;
	_error.clear();
	burst();
	return _error.empty();
}

bool RuntimeSession::run_prelude(asIScriptFunction* fn) {
	if (!fn)
		return true;
	asIScriptContext* ctx = _impl->engine->CreateContext();
	if (!ctx) {
		_error = "could not create prelude context";
		return false;
	}
	_impl->steps = 0;
	ctx->Prepare(fn);
	ctx->SetLineCallback(asFUNCTION(AbortCallback), &_impl->steps, asCALL_CDECL);
	g_scene_registry = &_impl->reg;
	g_call_log = _impl->call_log ? &_impl->call_log : nullptr;
	g_native_call = _impl->native_call ? &_impl->native_call : nullptr;
	g_call_silent = _impl->call_log_set;
	const int er = ctx->Execute();
	g_scene_registry = nullptr;
	g_call_log = nullptr;
	g_native_call = nullptr;
	g_call_silent = false;
	if (er != asEXECUTION_FINISHED) {
		char buf[128];
		std::snprintf(buf, sizeof(buf), "prelude %s failed (%d)", fn->GetName(), er);
		_error = buf;
		if (er == asEXECUTION_EXCEPTION) {
			std::fprintf(stderr, "[as] prelude exception: %s\n", ctx->GetExceptionString());
			if (asIScriptFunction* ef = ctx->GetExceptionFunction())
				std::fprintf(stderr, "[as]   in %s line %d\n", ef->GetDeclaration(), ctx->GetExceptionLineNumber());
		}
	}
	ctx->Release();
	_impl->steps = 0;
	_impl->reg.pending_wait = -1;
	return _error.empty();
}

void RuntimeSession::burst() {
	_impl->reg.set_frame(_frame);
	_impl->steps = 0;
	g_scene_registry = &_impl->reg;
	g_call_log = _impl->call_log ? &_impl->call_log : nullptr;
	g_native_call = _impl->native_call ? &_impl->native_call : nullptr;
	g_call_silent = _impl->call_log_set;
	const int er = _impl->ctx->Execute();
	if (er == asEXECUTION_SUSPENDED) {
		_impl->wake = _frame + std::max(1L, _impl->reg.pending_wait);
	} else {
		_finished = true;
		if (er != asEXECUTION_FINISHED) {
			char buf[128];
			std::snprintf(buf, sizeof(buf), "script execution failed (%d)", er);
			_error = buf;
			std::fprintf(stderr, "[as] run r=%d\n", er);
			if (er == asEXECUTION_EXCEPTION) {
				std::fprintf(stderr, "[as] exception: %s\n", _impl->ctx->GetExceptionString());
				if (asIScriptFunction* ef = _impl->ctx->GetExceptionFunction())
					std::fprintf(
						stderr, "[as]   in %s line %d\n", ef->GetDeclaration(), _impl->ctx->GetExceptionLineNumber());
			}
		}
	}
	_impl->reg.pending_wait = -1;
	if (_record_keyframes)
		_impl->reg.flush_keyframe();
	g_scene_registry = nullptr;
	g_call_log = nullptr;
	g_native_call = nullptr;
	g_call_silent = false;
}

RuntimeSession::StepResult RuntimeSession::advance() {
	if (!_started) {
		_error = "runtime session not started";
		return StepResult::Error;
	}
	if (_finished)
		return StepResult::Finished;
	if (_limit)
		return StepResult::Limit;
	if (_frame >= _max_frames) {
		_limit = true;
		return StepResult::Limit;
	}

	++_frame;
	_impl->reg.set_frame(_frame);
	_impl->reg.advance();
	if (_frame >= _impl->wake && !_finished)
		burst();

	if (_finished)
		return StepResult::Finished;
	if (_limit)
		return StepResult::Limit;
	return StepResult::Advanced;
}

const SceneState& RuntimeSession::scene() const { return _impl->reg.state(); }

Timeline RuntimeSession::take_timeline() { return std::move(_impl->reg.timeline()); }

asIScriptFunction* RuntimeSession::resolve_callback(std::string_view name) const {
	return _impl->reg.resolve_callback(name);
}

Timeline Host::run(asIScriptFunction* fn, std::string source, long max_frames) {
	RuntimeSession session(*this, std::move(source));
	session.set_max_frames(max_frames);
	session.set_record_keyframes(true);
	if (!session.start(fn)) {
		std::fprintf(stderr, "[as] run failed: %s\n", session.error().c_str());
		return Timeline{};
	}
	while (!session.finished() && !session.limit_reached()) {
		if (session.advance() == RuntimeSession::StepResult::Error)
			break;
	}

	Timeline tl = session.take_timeline();
	std::fprintf(
		stderr,
		"[as] run %s: %zu keyframe(s), %zu entit(ies), last frame %ld\n",
		fn ? fn->GetName() : "?",
		tl.keyframes.size(),
		tl.keyframes.empty() ? 0 : tl.keyframes.back().entities.size(),
		session.frame());

	if (std::getenv("ASB_CALLBACKS")) {
		for (const auto& cb : tl.callbacks) {
			asIScriptFunction* cbf = session.resolve_callback(cb.function);
			std::fprintf(
				stderr,
				"[callback] %s key=\"%s\"%s%s -> %s (%s; dispatch deferred)\n",
				cb.event.c_str(),
				cb.key.c_str(),
				cb.slot.empty() ? "" : " slot=",
				cb.slot.c_str(),
				cb.function.c_str(),
				cbf ? "resolved" : "MISSING");
		}
	}

	g_scene_registry = nullptr;
	g_call_log = nullptr;
	g_native_call = nullptr;
	g_call_silent = false;
	return tl;
}

Timeline run_module(
	Host& host,
	std::span<const uint8_t> bytecode,
	const std::string& source,
	const std::string& func,
	long max_frames) {
	Module mod = parse(bytecode);
	host.register_module(mod);
	if (!host.load(bytecode))
		return Timeline{};

	Scene scene = extract_scene(mod);
	const SceneFunction* picked = pick_scene_function(scene, func);
	asIScriptFunction* fn = picked ? host.find_function({picked->name}) : nullptr;
	if (!fn)
		fn = host.find_function({"scene00", "MAIN"});
	if (!fn)
		return Timeline{};
	return host.run(fn, source, max_frames);
}

} // namespace asb
