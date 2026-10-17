#include <scarlett/host.hpp>

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

	std::string line = "call ";
	if (*fn->GetNamespace()) {
		line += fn->GetNamespace();
		line += "::";
	}
	line += fn->GetName();
	line += "(";
	for (asUINT i = 0; i < gen->GetArgCount(); ++i) {
		if (i)
			line += ", ";
		line += arg_to_string(gen, i);
	}
	line += ")";
	if (g_call_log)
		(*g_call_log)(line);
	else
		std::printf("%s\n", line.c_str());

	// Let the recording shim model scene-relevant calls (and the scheduler hook)
	// before falling back to a benign no-op return.
	if (g_scene_registry && g_scene_registry->dispatch(gen))
		return;

	int rt = fn->GetReturnTypeId();
	if (rt == asTYPEID_VOID)
		return;
	if (rt & asTYPEID_OBJHANDLE) {
		gen->SetReturnAddress(nullptr);
		return;
	}
	switch (rt) {
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

	// Value types are returned into caller-allocated storage; if we leave it
	// untouched the VM later destroys/copies uninitialised memory (e.g. a bogus
	// `string`). Default-construct a string and zero anything else.
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
	std::function<long(const std::string&, int)> track_frames;
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

void Host::set_call_log(std::function<void(const std::string&)> cb) { _impl->call_log = std::move(cb); }

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
	asIScriptContext* ctx = _impl->engine->CreateContext();
	ctx->Prepare(fn);
	unsigned long steps = 0;
	ctx->SetLineCallback(asFUNCTION(AbortCallback), &steps, asCALL_CDECL);
	int er = ctx->Execute();
	std::fprintf(stderr, "[as] Execute r=%d (%s)\n", er, er == asEXECUTION_FINISHED ? "finished" : "other");
	if (er == asEXECUTION_EXCEPTION) {
		std::fprintf(stderr, "[as] exception: %s\n", ctx->GetExceptionString());
		if (asIScriptFunction* ef = ctx->GetExceptionFunction())
			std::fprintf(
				stderr, "[as]   in %s line %d\n", ef->GetDeclaration(), ctx->GetExceptionLineNumber());
	}
	ctx->Release();
	g_call_log = nullptr;
	return er;
}

void Host::set_char_table(const CharTable& chars) { _impl->chars = &chars; }

void Host::set_chara_set_table(const CharaSetTable& sets) { _impl->chara_sets = &sets; }

void Host::set_track_frames(std::function<long(const std::string&, int)> fn) { _impl->track_frames = std::move(fn); }

void Host::set_resolve_track(std::function<int(const std::string&, const std::string&)> fn) {
	_impl->resolve_track = std::move(fn);
}

void Host::set_facility_name(std::function<std::string(const std::string&)> fn) {
	_impl->facility_name = std::move(fn);
}

void Host::set_stage_table(const StageTable& stages) { _impl->stages = &stages; }

void Host::set_drop_tables(const DropTables& drops) { _impl->drops = &drops; }

void Host::set_sound_length(std::function<int(int)> fn) { _impl->sound_length = std::move(fn); }

Timeline Host::run(asIScriptFunction* fn, std::string source, long max_frames) {
	g_call_log = _impl->call_log ? &_impl->call_log : nullptr;

	Registry reg;
	reg.set_char_table(_impl->chars);
	if (_impl->chara_sets)
		reg.set_chara_set_table(_impl->chara_sets);
	if (_impl->track_frames)
		reg.set_track_frames(_impl->track_frames);
	if (_impl->resolve_track)
		reg.set_resolve_track(_impl->resolve_track);
	if (_impl->facility_name)
		reg.set_facility_name(_impl->facility_name);
	if (_impl->stages)
		reg.set_stage_table(*_impl->stages);
	if (_impl->drops)
		reg.set_drop_tables(*_impl->drops);
	if (_impl->sound_length)
		reg.set_sound_length(_impl->sound_length);
	reg.reset(std::move(source), fn ? fn->GetName() : "");
	g_scene_registry = &reg;

	asIScriptContext* ctx = _impl->engine->CreateContext();
	ctx->Prepare(fn);

	unsigned long steps = 0;
	ctx->SetLineCallback(asFUNCTION(AbortCallback), &steps, asCALL_CDECL);

	long frame = 0;
	long wake = 0;
	bool done = false;

	auto burst = [&]() {
		reg.frame = frame;
		steps = 0;
		int er = ctx->Execute();
		if (er == asEXECUTION_SUSPENDED) {
			wake = frame + std::max(1L, reg.pending_wait);
		} else {
			done = true;
			if (er != asEXECUTION_FINISHED) {
				std::fprintf(stderr, "[as] run r=%d\n", er);
				if (er == asEXECUTION_EXCEPTION) {
					std::fprintf(stderr, "[as] exception: %s\n", ctx->GetExceptionString());
					if (asIScriptFunction* ef = ctx->GetExceptionFunction())
						std::fprintf(
							stderr,
							"[as]   in %s line %d\n",
							ef->GetDeclaration(),
							ctx->GetExceptionLineNumber());
				}
			}
		}
		reg.pending_wait = -1;
		reg.flush_keyframe();
	};

	burst();
	while (!done) {
		long next = std::max(wake, frame + 1);
		if (next > max_frames)
			break;
		frame = next;
		burst();
	}

	std::fprintf(
		stderr,
		"[as] run %s: %zu keyframe(s), %zu entit(ies), last frame %ld\n",
		fn ? fn->GetName() : "?",
		reg.timeline().keyframes.size(),
		reg.timeline().keyframes.empty() ? 0 : reg.timeline().keyframes.back().entities.size(),
		frame);

	g_scene_registry = nullptr;
	g_call_log = nullptr;
	ctx->Release();
	return std::move(reg.timeline());
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
