#include <scarlett/host.hpp>

#include <angelscript.h>
#include <scriptarray/scriptarray.h>
#include <scriptstdstring/scriptstdstring.h>

#include <cstdio>
#include <cstring>
#include <memory>
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
	else
		std::snprintf(buf, sizeof(buf), "obj@%p", gen->GetArgAddress(i));
	return buf;
}

std::function<void(const std::string&)>* g_call_log = nullptr;

void GenericStub(asIScriptGeneric* gen) {
	asIScriptFunction* fn = gen->GetFunction();

	std::string line = "call ";
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
};

Host::Host()
	: _impl(new Impl()) {
	_impl->engine = asCreateScriptEngine();
	_impl->engine->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
	// Don't run global-variable initializers at load time; they call stubbed
	// natives and would abort the load.
	_impl->engine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, 0);

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
}

void Host::register_module(const Module& mod) {
	asIScriptEngine* engine = _impl->engine;

	// Value types the engine exposes (known sizes).
	struct VT {
		const char* name;
		int size;
	};
	const VT value_types[] = {
		{"FnVector2", 12},
		{"FnVector3", 12},
		{"FnVector4", 16},
		{"FnColor", 4},
	};
	for (const auto& vt : value_types) {
		int r = engine->RegisterObjectType(vt.name, vt.size, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS);
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
	// Reference object types registered by the engine (from the exe's
	// RegisterClass_* setup); these are never value types.
	for (const char* n : {"UcStObject", "UcStChaseParam", "UcMsgBalloonParam", "UcSpriteTextParam"})
		ref_names.insert(n);
	auto mark_value = [&](const DataType& dt) {
		if (dt.token == ttIdentifier && dt.type_info) {
			std::string n = dt.type_info->format();
			if (n.empty() || n.find('<') != std::string::npos || n.find('[') != std::string::npos)
				return;
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

	for (const auto& n : value_names) {
		if (_impl->registered_types.count(n))
			continue;
		if (ref_names.count(n)) { // factory/handle evidence: reference type
			int r = engine->RegisterObjectType(n.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT);
			if (r < 0)
				std::fprintf(stderr, "[as] RegisterObjectType(%s,ref) r=%d\n", n.c_str(), r);
			_impl->registered_types[n] = true;
			continue;
		}
		int size = _impl->known_size.count(n) ? _impl->known_size[n] : 32;
		int r = engine->RegisterObjectType(n.c_str(), size, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS);
		if (r < 0)
			std::fprintf(stderr, "[as] RegisterObjectType(%s,value) r=%d\n", n.c_str(), r);
		_impl->registered_types[n] = true;
	}
	for (const auto& n : ref_names) {
		if (_impl->registered_types.count(n))
			continue;
		int r = engine->RegisterObjectType(n.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT);
		if (r < 0)
			std::fprintf(stderr, "[as] RegisterObjectType(%s,ref) r=%d\n", n.c_str(), r);
		_impl->registered_types[n] = true;
	}

	// Remaining engine object types the module refers to -> opaque references.
	for (const auto& t : mod.used_types) {
		if (!t || t->kind != TypeInfo::Object)
			continue;
		std::string name = t->format();
		if (name.empty() || name.find('<') != std::string::npos || name.find('[') != std::string::npos ||
			_impl->registered_types.count(name))
			continue;
		int r = engine->RegisterObjectType(name.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT);
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
		if ((sig.name.rfind("$beh", 0) == 0 || sig.name == "$fact") && sig.name.size() >= 4) {
			// Object behaviour the reader expects the registered type to expose.
			// Constructors/destructors target the method's object type; factories
			// target the type they return.
			TypeInfoPtr target = sig.object_type;
			if (!target && (sig.name == "$fact" || sig.name == "$beh3"))
				target = sig.return_type.type_info;
			if (!target) {
				++skipped;
				continue;
			}
			std::string obj = target->format();
			if (obj.empty() || obj[0] == '$' || obj.find('[') != std::string::npos) {
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
			int beh = sig.name == "$fact" ? asBEHAVE_FACTORY : (sig.name[4] - '0');
			r = engine->RegisterObjectBehaviour(
				obj.c_str(), (asEBehaviours)beh, decl.c_str(), asFUNCTION(GenericStub), asCALL_GENERIC);
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
			std::string obj = sig.object_type->format();
			if (obj.empty() || obj[0] == '$' || obj.find('[') != std::string::npos) {
				++skipped;
				continue;
			}
			std::string decl = sig.bare_decl(false);
			if (decl.find('[') != std::string::npos) {
				++skipped;
				continue;
			}
			r = engine->RegisterObjectMethod(obj.c_str(), decl.c_str(), asFUNCTION(GenericStub), asCALL_GENERIC);
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
	std::fprintf(
		stderr, "LoadByteCode r=%d stripped=%d consumed=%u/%zu\n", r, stripped, in.pos, bytecode.size());
	return r == 0;
}

asIScriptModule* Host::module() const {
	return _impl->module;
}

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
	ctx->Release();
	g_call_log = nullptr;
	return er;
}

} // namespace asb
