#include <impl/asb/asb.hpp>

#include <angelscript.h>
#include <criware/cpk.hpp>
#include <scriptarray/scriptarray.h>
#include <scriptstdstring/scriptstdstring.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

class FileStream : public asIBinaryStream {
public:
	std::vector<unsigned char> data;
	asUINT pos = 0;
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

struct Stub {
	unsigned long calls = 0;
};

void GenericStub(asIScriptGeneric* gen) {
	asIScriptFunction* fn = gen->GetFunction();

	std::printf("call %s(", fn->GetName());
	for (asUINT i = 0; i < gen->GetArgCount(); ++i) {
		if (i)
			std::printf(", ");
		std::string a = arg_to_string(gen, i);
		std::printf("%s", a.c_str());
	}
	std::printf(")\n");

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

std::vector<uint8_t> read_file(const std::string& path) {
	std::ifstream f(path, std::ios::binary);
	if (!f)
		throw std::runtime_error("cannot open " + path);
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

std::vector<uint8_t> extract_entry(const std::string& cpk, const std::string& entry) {
	auto pos = entry.find_last_of('/');
	std::string dir = pos == std::string::npos ? "" : entry.substr(0, pos);
	std::string name = pos == std::string::npos ? entry : entry.substr(pos + 1);
	CPKTable table = TopLevelCpk(std::filesystem::absolute(cpk)).getTableOfContents();
	std::vector<char> buf;
	if (auto e = table.find_file(dir, name); e == table.end())
		throw std::runtime_error("not found in CPK: " + entry);
	else
		table.extract(*e, buf);
	return std::vector<uint8_t>(buf.begin(), buf.end());
}

} // namespace

int main(int argc, char** argv) try {
	std::string input, cpk, entry, func;
	unsigned max_calls = 200;
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		auto next = [&]() { return (i + 1 < argc) ? argv[++i] : ""; };
		if (a == "--input")
			input = next();
		else if (a == "--cpk")
			cpk = next();
		else if (a == "--asb")
			entry = next();
		else if (a == "--func")
			func = next();
		else if (a == "--max-calls")
			max_calls = std::stoul(next());
		else {
			std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
			return 2;
		}
	}

	std::vector<uint8_t> bytes = !input.empty() ? read_file(input) : extract_entry(cpk, entry);
	if (bytes.empty())
		throw std::runtime_error("no input");

	asb::Module mod = asb::parse(std::span<const uint8_t>(bytes.data(), bytes.size()));
	std::fprintf(
		stderr,
		"asb decode: consumed=%zu/%zu funcs=%zu used_fns=%zu used_types=%zu error=%s\n",
		mod.consumed,
		mod.total,
		mod.functions.size(),
		mod.used_functions.size(),
		mod.used_types.size(),
		mod.error.c_str());
	for (const auto& b : mod.binds)
		std::fprintf(stderr, "[as] bind from '%s': %s\n", b.import_from.c_str(), b.sig.decl().c_str());

	asIScriptEngine* engine = asCreateScriptEngine();
	engine->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
	engine->SetEngineProperty(asEP_EXPAND_DEF_ARRAY_TO_TMPL, 1);

	RegisterStdString(engine);
	RegisterScriptArray(engine, true);

	// Value types the engine exposes (known sizes).
	struct VT {
		const char* name;
		int size;
	};
	const VT value_types[] = {
		{"FnVector2", 8},
		{"FnVector3", 12},
		{"FnVector4", 16},
		{"FnColor", 4},
	};
	std::unordered_map<std::string, int> known_size;
	std::unordered_map<std::string, bool> registered_types;
	for (const auto& vt : value_types) {
		int r = engine->RegisterObjectType(vt.name, vt.size, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS);
		if (r < 0)
			std::fprintf(stderr, "[as] RegisterObjectType(%s) r=%d\n", vt.name, r);
		known_size[vt.name] = vt.size;
		registered_types[vt.name] = true;
	}
	registered_types["string"] = true; // registered by the scriptstdstring add-on
	registered_types["array"] = true;  // registered by the scriptarray add-on

	// Classify the module's own object types by how signatures use them: a type
	// used via @ (or produced by a factory) is a reference type; one used by
	// value (or const-ref) is a value type. Reference evidence wins.
	std::unordered_set<std::string> value_names;
	std::unordered_set<std::string> ref_names;
	auto mark_value = [&](const asb::DataType& dt) {
		if (dt.token == asb::ttIdentifier && dt.type_info) {
			std::string n = dt.type_info->format();
			if (n.empty() || n.find('<') != std::string::npos || n.find('[') != std::string::npos)
				return;
			if (dt.is_object_handle)
				ref_names.insert(n);
			else
				value_names.insert(n);
		}
	};
	auto scan_sig = [&](const asb::Signature& sig) {
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
		if (registered_types.count(n))
			continue;
		if (ref_names.count(n)) { // factory/handle evidence: reference type
			int r = engine->RegisterObjectType(n.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT);
			if (r < 0)
				std::fprintf(stderr, "[as] RegisterObjectType(%s,ref) r=%d\n", n.c_str(), r);
			registered_types[n] = true;
			continue;
		}
		int size = known_size.count(n) ? known_size[n] : 32;
		int r = engine->RegisterObjectType(n.c_str(), size, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS);
		if (r < 0)
			std::fprintf(stderr, "[as] RegisterObjectType(%s,value) r=%d\n", n.c_str(), r);
		registered_types[n] = true;
	}
	for (const auto& n : ref_names) {
		if (registered_types.count(n))
			continue;
		int r = engine->RegisterObjectType(n.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT);
		if (r < 0)
			std::fprintf(stderr, "[as] RegisterObjectType(%s,ref) r=%d\n", n.c_str(), r);
		registered_types[n] = true;
	}

	// Remaining engine object types the module refers to -> opaque references.
	for (const auto& t : mod.used_types) {
		if (!t)
			continue;
		if (t->kind != asb::TypeInfo::Object)
			continue;
		std::string name = t->format();
		if (name.empty() || name.find('<') != std::string::npos || name.find('[') != std::string::npos ||
			registered_types.count(name))
			continue;
		int r = engine->RegisterObjectType(name.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT);
		if (r < 0)
			std::fprintf(stderr, "[as] RegisterObjectType(%s) r=%d\n", name.c_str(), r);
		registered_types[name] = true;
	}

	// Register every native (kind 'a') the module imports, as generic stubs:
	// global functions on the engine, object methods on their (opaque) types.
	unsigned registered = 0, skipped = 0;
	for (size_t i = 0; i < mod.used_functions.size(); ++i) {
		char kind = i < mod.used_function_kind.size() ? mod.used_function_kind[i] : '?';
		if (kind != 'a' || !mod.used_functions[i])
			continue;
		const asb::Signature& sig = mod.used_functions[i]->sig;
		int r;
		if ((sig.name.rfind("$beh", 0) == 0 || sig.name == "$fact") && sig.name.size() >= 4) {
			// Object behaviour the reader expects the registered type to expose.
			// Constructors/destructors target the method's object type; factories
			// target the type they return.
			asb::TypeInfoPtr target = sig.object_type;
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
	std::vector<std::unique_ptr<std::vector<char>>> prop_storage;
	for (const auto& g : mod.used_globals) {
		if (g.module_prop)
			continue; // module globals are created by the reader
		int size = 32;
		if (g.type.type_info) {
			std::string tn = g.type.type_info->format();
			if (known_size.count(tn))
				size = known_size[tn];
		}
		auto buf = std::make_unique<std::vector<char>>(static_cast<size_t>(size), 0);
		std::string decl = g.type.as_decl() + " " + g.name;
		if (decl.find('[') != std::string::npos)
			continue; // default-array type properties
		if (!g.name_space.empty())
			engine->SetDefaultNamespace(g.name_space.c_str());
		int r = engine->RegisterGlobalProperty(decl.c_str(), buf->data());
		if (!g.name_space.empty())
			engine->SetDefaultNamespace("");
		if (r < 0)
			std::fprintf(
				stderr, "[as] RegisterGlobalProperty failed r=%d: %s (ns %s)\n", r, decl.c_str(), g.name_space.c_str());
		prop_storage.push_back(std::move(buf));
	}

	asIScriptModule* m = engine->GetModule("loaded", asGM_ALWAYS_CREATE);
	FileStream in;
	in.data = bytes;
	bool stripped = false;
	int r = m->LoadByteCode(&in, &stripped);
	std::fprintf(stderr, "LoadByteCode r=%d stripped=%d consumed=%u/%zu\n", r, stripped, in.pos, bytes.size());
	if (r < 0)
		return 3;

	asUINT cnt = m->GetFunctionCount();
	std::fprintf(stderr, "functions=%u\n", cnt);
	for (asUINT i = 0; i < cnt; ++i)
		std::fprintf(stderr, "  func[%u] %s\n", i, m->GetFunctionByIndex(i)->GetDeclaration());

	asIScriptFunction* target = nullptr;
	if (!func.empty())
		target = m->GetFunctionByName(func.c_str());
	for (const char* cand : {"scene00", "MAIN"}) {
		if (target)
			break;
		target = m->GetFunctionByName(cand);
	}
	if (!target) {
		std::fprintf(stderr, "[as] no entry function found\n");
		return 4;
	}
	std::fprintf(stderr, "[as] running %s\n", target->GetDeclaration());

	asIScriptContext* ctx = engine->CreateContext();
	ctx->Prepare(target);
	unsigned long steps = 0;
	ctx->SetLineCallback(asFUNCTION(AbortCallback), &steps, asCALL_CDECL);
	int er = ctx->Execute();
	std::fprintf(stderr, "[as] Execute r=%d (%s)\n", er, er == asEXECUTION_FINISHED ? "finished" : "other");
	ctx->Release();
	engine->ShutDownAndRelease();
	return 0;
} catch (const std::exception& e) {
	std::fprintf(stderr, "error: %s\n", e.what());
	return 1;
}
