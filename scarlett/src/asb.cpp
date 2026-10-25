#include <scarlett/asb.hpp>

#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace asb {

namespace {

// --- AngelScript 2.35.1 opcode argument-layout table -----------------------
enum BCT {
	INFO = 0,
	NO_ARG,
	W_ARG,
	wW_ARG,
	DW_ARG,
	rW_DW_ARG,
	QW_ARG,
	DW_DW_ARG,
	wW_rW_rW_ARG,
	wW_QW_ARG,
	wW_rW_ARG,
	rW_ARG,
	wW_DW_ARG,
	wW_rW_DW_ARG,
	rW_rW_ARG,
	wW_W_ARG,
	QW_DW_ARG,
	rW_QW_ARG,
	W_DW_ARG,
	rW_W_DW_ARG,
	rW_DW_DW_ARG
};

struct OpInfo {
	const char* name;
	uint8_t type;
};

// PTR_ARG=QW_ARG, PTR_DW_ARG=QW_DW_ARG, wW_PTR_ARG=wW_QW_ARG, rW_PTR_ARG=rW_QW_ARG (64-bit)
const OpInfo OP[201] = {
	{"PopPtr", NO_ARG},
	{"PshGPtr", QW_ARG},
	{"PshC4", DW_ARG},
	{"PshV4", rW_ARG},
	{"PSF", rW_ARG},
	{"SwapPtr", NO_ARG},
	{"NOT", rW_ARG},
	{"PshG4", QW_ARG},
	{"LdGRdR4", wW_QW_ARG},
	{"CALL", DW_ARG},
	{"RET", W_ARG},
	{"JMP", DW_ARG},
	{"JZ", DW_ARG},
	{"JNZ", DW_ARG},
	{"JS", DW_ARG},
	{"JNS", DW_ARG},
	{"JP", DW_ARG},
	{"JNP", DW_ARG},
	{"TZ", NO_ARG},
	{"TNZ", NO_ARG},
	{"TS", NO_ARG},
	{"TNS", NO_ARG},
	{"TP", NO_ARG},
	{"TNP", NO_ARG},
	{"NEGi", rW_ARG},
	{"NEGf", rW_ARG},
	{"NEGd", rW_ARG},
	{"INCi16", NO_ARG},
	{"INCi8", NO_ARG},
	{"DECi16", NO_ARG},
	{"DECi8", NO_ARG},
	{"INCi", NO_ARG},
	{"DECi", NO_ARG},
	{"INCf", NO_ARG},
	{"DECf", NO_ARG},
	{"INCd", NO_ARG},
	{"DECd", NO_ARG},
	{"IncVi", rW_ARG},
	{"DecVi", rW_ARG},
	{"BNOT", rW_ARG},
	{"BAND", wW_rW_rW_ARG},
	{"BOR", wW_rW_rW_ARG},
	{"BXOR", wW_rW_rW_ARG},
	{"BSLL", wW_rW_rW_ARG},
	{"BSRL", wW_rW_rW_ARG},
	{"BSRA", wW_rW_rW_ARG},
	{"COPY", W_DW_ARG},
	{"PshC8", QW_ARG},
	{"PshVPtr", rW_ARG},
	{"RDSPtr", NO_ARG},
	{"CMPd", rW_rW_ARG},
	{"CMPu", rW_rW_ARG},
	{"CMPf", rW_rW_ARG},
	{"CMPi", rW_rW_ARG},
	{"CMPIi", rW_DW_ARG},
	{"CMPIf", rW_DW_ARG},
	{"CMPIu", rW_DW_ARG},
	{"JMPP", rW_ARG},
	{"PopRPtr", NO_ARG},
	{"PshRPtr", NO_ARG},
	{"STR", W_ARG},
	{"CALLSYS", DW_ARG},
	{"CALLBND", DW_ARG},
	{"SUSPEND", NO_ARG},
	{"ALLOC", QW_DW_ARG},
	{"FREE", wW_QW_ARG},
	{"LOADOBJ", rW_ARG},
	{"STOREOBJ", wW_ARG},
	{"GETOBJ", W_ARG},
	{"REFCPY", QW_ARG},
	{"CHKREF", NO_ARG},
	{"GETOBJREF", W_ARG},
	{"GETREF", W_ARG},
	{"PshNull", NO_ARG},
	{"ClrVPtr", wW_ARG},
	{"OBJTYPE", QW_ARG},
	{"TYPEID", DW_ARG},
	{"SetV4", wW_DW_ARG},
	{"SetV8", wW_QW_ARG},
	{"ADDSi", W_DW_ARG},
	{"CpyVtoV4", wW_rW_ARG},
	{"CpyVtoV8", wW_rW_ARG},
	{"CpyVtoR4", rW_ARG},
	{"CpyVtoR8", rW_ARG},
	{"CpyVtoG4", rW_QW_ARG},
	{"CpyRtoV4", wW_ARG},
	{"CpyRtoV8", wW_ARG},
	{"CpyGtoV4", wW_QW_ARG},
	{"WRTV1", rW_ARG},
	{"WRTV2", rW_ARG},
	{"WRTV4", rW_ARG},
	{"WRTV8", rW_ARG},
	{"RDR1", wW_ARG},
	{"RDR2", wW_ARG},
	{"RDR4", wW_ARG},
	{"RDR8", wW_ARG},
	{"LDG", QW_ARG},
	{"LDV", rW_ARG},
	{"PGA", QW_ARG},
	{"CmpPtr", rW_rW_ARG},
	{"VAR", rW_ARG},
	{"iTOf", rW_ARG},
	{"fTOi", rW_ARG},
	{"uTOf", rW_ARG},
	{"fTOu", rW_ARG},
	{"sbTOi", rW_ARG},
	{"swTOi", rW_ARG},
	{"ubTOi", rW_ARG},
	{"uwTOi", rW_ARG},
	{"dTOi", wW_rW_ARG},
	{"dTOu", wW_rW_ARG},
	{"dTOf", wW_rW_ARG},
	{"iTOd", wW_rW_ARG},
	{"uTOd", wW_rW_ARG},
	{"fTOd", wW_rW_ARG},
	{"ADDi", wW_rW_rW_ARG},
	{"SUBi", wW_rW_rW_ARG},
	{"MULi", wW_rW_rW_ARG},
	{"DIVi", wW_rW_rW_ARG},
	{"MODi", wW_rW_rW_ARG},
	{"ADDf", wW_rW_rW_ARG},
	{"SUBf", wW_rW_rW_ARG},
	{"MULf", wW_rW_rW_ARG},
	{"DIVf", wW_rW_rW_ARG},
	{"MODf", wW_rW_rW_ARG},
	{"ADDd", wW_rW_rW_ARG},
	{"SUBd", wW_rW_rW_ARG},
	{"MULd", wW_rW_rW_ARG},
	{"DIVd", wW_rW_rW_ARG},
	{"MODd", wW_rW_rW_ARG},
	{"ADDIi", wW_rW_DW_ARG},
	{"SUBIi", wW_rW_DW_ARG},
	{"MULIi", wW_rW_DW_ARG},
	{"ADDIf", wW_rW_DW_ARG},
	{"SUBIf", wW_rW_DW_ARG},
	{"MULIf", wW_rW_DW_ARG},
	{"SetG4", QW_DW_ARG},
	{"ChkRefS", NO_ARG},
	{"ChkNullV", rW_ARG},
	{"CALLINTF", DW_ARG},
	{"iTOb", rW_ARG},
	{"iTOw", rW_ARG},
	{"SetV1", wW_DW_ARG},
	{"SetV2", wW_DW_ARG},
	{"Cast", DW_ARG},
	{"i64TOi", wW_rW_ARG},
	{"uTOi64", wW_rW_ARG},
	{"iTOi64", wW_rW_ARG},
	{"fTOi64", wW_rW_ARG},
	{"dTOi64", rW_ARG},
	{"fTOu64", wW_rW_ARG},
	{"dTOu64", rW_ARG},
	{"i64TOf", wW_rW_ARG},
	{"u64TOf", wW_rW_ARG},
	{"i64TOd", rW_ARG},
	{"u64TOd", rW_ARG},
	{"NEGi64", rW_ARG},
	{"INCi64", NO_ARG},
	{"DECi64", NO_ARG},
	{"BNOT64", rW_ARG},
	{"ADDi64", wW_rW_rW_ARG},
	{"SUBi64", wW_rW_rW_ARG},
	{"MULi64", wW_rW_rW_ARG},
	{"DIVi64", wW_rW_rW_ARG},
	{"MODi64", wW_rW_rW_ARG},
	{"BAND64", wW_rW_rW_ARG},
	{"BOR64", wW_rW_rW_ARG},
	{"BXOR64", wW_rW_rW_ARG},
	{"BSLL64", wW_rW_rW_ARG},
	{"BSRL64", wW_rW_rW_ARG},
	{"BSRA64", wW_rW_rW_ARG},
	{"CMPi64", rW_rW_ARG},
	{"CMPu64", rW_rW_ARG},
	{"ChkNullS", W_ARG},
	{"ClrHi", NO_ARG},
	{"JitEntry", QW_ARG},
	{"CallPtr", rW_ARG},
	{"FuncPtr", QW_ARG},
	{"LoadThisR", W_DW_ARG},
	{"PshV8", rW_ARG},
	{"DIVu", wW_rW_rW_ARG},
	{"MODu", wW_rW_rW_ARG},
	{"DIVu64", wW_rW_rW_ARG},
	{"MODu64", wW_rW_rW_ARG},
	{"LoadRObjR", rW_W_DW_ARG},
	{"LoadVObjR", rW_W_DW_ARG},
	{"RefCpyV", wW_QW_ARG},
	{"JLowZ", DW_ARG},
	{"JLowNZ", DW_ARG},
	{"AllocMem", wW_DW_ARG},
	{"SetListSize", rW_DW_DW_ARG},
	{"PshListElmnt", rW_DW_ARG},
	{"SetListType", rW_DW_DW_ARG},
	{"POWi", wW_rW_rW_ARG},
	{"POWu", wW_rW_rW_ARG},
	{"POWf", wW_rW_rW_ARG},
	{"POWd", wW_rW_rW_ARG},
	{"POWdi", wW_rW_rW_ARG},
	{"POWi64", wW_rW_rW_ARG},
	{"POWu64", wW_rW_rW_ARG},
	{"Thiscall1", DW_ARG},
};

const OpInfo& opinfo(uint8_t op) {
	static const OpInfo unknown{"???", INFO};
	return op < 201 ? OP[op] : unknown;
}

constexpr uint32_t ASOBJ_SCRIPT_OBJECT = 1u << 21;
constexpr uint32_t ASOBJ_SHARED = 1u << 22;
constexpr uint32_t ASOBJ_FUNCDEF = 1u << 24;
constexpr uint32_t ASOBJ_ENUM = 1u << 26;
constexpr uint32_t ASOBJ_TYPEDEF = 1u << 28;

enum FuncType {
	F_DUMMY = -1,
	F_SYSTEM = 0,
	F_SCRIPT = 1,
	F_INTERFACE = 2,
	F_VIRTUAL = 3,
	F_FUNCDEF = 4,
	F_IMPORTED = 5,
	F_DELEGATE = 6
};

const char* func_type_name(int t) {
	switch (t) {
	case F_SYSTEM:
		return "system";
	case F_SCRIPT:
		return "script";
	case F_INTERFACE:
		return "interface";
	case F_VIRTUAL:
		return "virtual";
	case F_FUNCDEF:
		return "funcdef";
	case F_IMPORTED:
		return "imported";
	case F_DELEGATE:
		return "delegate";
	default:
		return "?";
	}
}

class Parser {
public:
	explicit Parser(std::span<const uint8_t> d)
		: data(d) {}

	Module run() {
		mod.total = data.size();
		parse_module();
		mod.consumed = pos;
		if (!ok && mod.error.empty())
			mod.error = "parse error near byte " + std::to_string(pos);
		if (ok)
			annotate();
		return std::move(mod);
	}

private:
	// --- primitive reads ---------------------------------------------------
	bool read(void* dst, size_t n) {
		if (pos > data.size() || n > data.size() - pos) {
			ok = false;
			return false;
		}
		std::memcpy(dst, data.data() + pos, n);
		pos += n;
		return true;
	}
	uint8_t u8() {
		uint8_t v = 0;
		read(&v, 1);
		return v;
	}
	uint32_t u32_raw() {
		// Raw 32-bit fields (type flags, enum values) are stored big-endian.
		uint8_t b[4] = {0, 0, 0, 0};
		read(b, 4);
		return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | uint32_t(b[3]);
	}
	uint64_t enc_u64() {
		uint8_t b = 0xFF;
		if (!read(&b, 1))
			return 0;
		bool neg = (b & 0x80) != 0;
		b &= 0x7F;
		uint64_t i = 0;
		if ((b & 0x7F) == 0x7F) {
			for (int k = 0; k < 8; ++k) {
				read(&b, 1);
				i = (i << 8) | b;
			}
		} else if ((b & 0x7E) == 0x7E) {
			i = uint64_t(b & 0x01) << 48;
			for (int k = 0; k < 6; ++k) {
				read(&b, 1);
				i = (i << 8) | b;
			}
		} else if ((b & 0x7C) == 0x7C) {
			i = uint64_t(b & 0x03) << 40;
			for (int k = 0; k < 5; ++k) {
				read(&b, 1);
				i = (i << 8) | b;
			}
		} else if ((b & 0x78) == 0x78) {
			i = uint64_t(b & 0x07) << 32;
			for (int k = 0; k < 4; ++k) {
				read(&b, 1);
				i = (i << 8) | b;
			}
		} else if ((b & 0x70) == 0x70) {
			i = uint64_t(b & 0x0F) << 24;
			for (int k = 0; k < 3; ++k) {
				read(&b, 1);
				i = (i << 8) | b;
			}
		} else if ((b & 0x60) == 0x60) {
			i = uint64_t(b & 0x1F) << 16;
			for (int k = 0; k < 2; ++k) {
				read(&b, 1);
				i = (i << 8) | b;
			}
		} else if ((b & 0x40) == 0x40) {
			i = uint64_t(b & 0x3F) << 8;
			read(&b, 1);
			i |= b;
		} else {
			i = b;
		}
		if (neg)
			i = uint64_t(-int64_t(i));
		return i;
	}
	uint32_t enc_u32() { return uint32_t(enc_u64() & 0xFFFFFFFFu); }
	uint16_t enc_u16() { return uint16_t(enc_u32() & 0xFFFFu); }
	int enc_i32() { return int(int32_t(enc_u32())); }
	std::string read_str() {
		uint32_t len = enc_u32();
		if (len & 1) {
			uint32_t idx = len / 2;
			return idx < savedStrings.size() ? savedStrings[idx] : std::string();
		}
		if (len == 0)
			return {};
		len /= 2;
		if (pos > data.size() || len > data.size() - pos) {
			ok = false;
			return {};
		}
		std::string s(len, '\0');
		if (!read(s.data(), len))
			return {};
		savedStrings.push_back(s);
		return s;
	}

	// --- datatypes ---------------------------------------------------------
	DataType read_datatype() {
		uint32_t idx = enc_u32();
		if (idx != 0) {
			if (idx - 1 < savedDataTypes.size())
				return savedDataTypes[idx - 1];
			return {};
		}
		DataType dt;
		int token = int(enc_u32());
		dt.token = token;
		uint32_t slot = uint32_t(savedDataTypes.size());
		savedDataTypes.emplace_back();
		if (token == ttIdentifier)
			dt.type_info = read_typeinfo();
		uint8_t b = u8();
		dt.is_object_handle = b & 1;
		dt.handle_to_const = b & 2;
		dt.is_reference = b & 4;
		dt.is_read_only = b & 8;
		savedDataTypes[slot] = dt;
		return dt;
	}

	TypeInfoPtr read_typeinfo() {
		uint8_t ch = u8();
		if (ch == 'a') {
			auto ti = std::make_shared<TypeInfo>();
			ti->kind = TypeInfo::Template;
			ti->name = read_str();
			ti->name_space = read_str();
			uint32_t n = enc_u32();
			if (!ok || pos > data.size() || n > data.size() - pos) {
				ok = false;
				return nullptr;
			}
			for (uint32_t k = 0; k < n && ok; ++k) {
				uint8_t c2 = u8();
				if (c2 == 's')
					ti->sub_types.push_back(read_datatype());
				else {
					DataType dt;
					dt.token = int(enc_u32());
					ti->sub_types.push_back(dt);
				}
			}
			return ti;
		}
		if (ch == 'l') {
			auto ti = std::make_shared<TypeInfo>();
			ti->kind = TypeInfo::List;
			ti->list_of = read_typeinfo();
			return ti;
		}
		if (ch == 's') {
			auto ti = std::make_shared<TypeInfo>();
			ti->kind = TypeInfo::SubType;
			ti->name = read_str();
			return ti;
		}
		if (ch == 'o') {
			std::string name = read_str();
			std::string ns = read_str();
			return canonical(name, ns);
		}
		if (ch == 'c') {
			auto ti = std::make_shared<TypeInfo>();
			ti->kind = TypeInfo::Child;
			ti->name = read_str();
			ti->parent = read_typeinfo();
			return ti;
		}
		return nullptr;
	}

	TypeInfoPtr canonical(const std::string& name, const std::string& ns) {
		std::string key = ns + "\x1f" + name;
		auto it = registry.find(key);
		if (it != registry.end())
			return it->second;
		auto ti = std::make_shared<TypeInfo>();
		ti->kind = TypeInfo::Object;
		ti->name = name;
		ti->name_space = ns;
		registry[key] = ti;
		return ti;
	}

	// --- signatures / functions -------------------------------------------
	Signature read_signature() {
		Signature s;
		s.name = read_str();
		if (s.name == "$dlgte")
			return s;
		s.return_type = read_datatype();
		uint32_t count = enc_u32();
		if (!ok || pos > data.size() || count > data.size() - pos) {
			ok = false;
			return s;
		}
		for (uint32_t k = 0; k < count && ok; ++k)
			s.params.push_back(read_datatype());
		if (count > 0) {
			uint32_t ic = enc_u32();
			if (!ok || ic > count) {
				ok = false;
				return s;
			}
			s.inout_flags.assign(count, 0);
			for (uint32_t k = 0; k < ic && ok; ++k)
				s.inout_flags[k] = enc_i32();
		}
		s.func_type = int(enc_u32());
		if (count > 0) {
			uint32_t dc = enc_u32();
			if (!ok || dc > count || pos > data.size() || dc > data.size() - pos) {
				ok = false;
				return s;
			}
			s.default_args.resize(dc);
			for (uint32_t k = 0; k < dc && ok; ++k)
				s.default_args[dc - 1 - k] = read_str();
		}
		s.object_type = read_typeinfo();
		if (s.object_type) {
			uint8_t b = u8();
			s.is_readonly = b & 1;
			s.is_private = b & 2;
			s.is_protected = b & 4;
			s.name_space = s.object_type->name_space;
		} else if (s.func_type == F_FUNCDEF) {
			uint8_t b = u8();
			if (b == 'n')
				s.name_space = read_str();
			else if (b == 'o')
				read_typeinfo();
		} else {
			s.name_space = read_str();
		}
		return s;
	}

	void read_bytecode(Function& fn) {
		uint32_t n = enc_u32();
		for (uint32_t k = 0; k < n && ok; ++k) {
			Instruction in;
			in.pos = uint32_t(fn.code.size());
			in.op = u8();
			const OpInfo& oi = opinfo(in.op);
			switch (oi.type) {
			case NO_ARG:
				break;
			case W_ARG:
			case wW_ARG:
			case rW_ARG:
				in.wargs.push_back(int16_t(enc_u16()));
				break;
			case W_DW_ARG:
			case rW_DW_ARG:
			case wW_DW_ARG:
				in.wargs.push_back(int16_t(enc_u16()));
				in.dargs.push_back(enc_u32());
				break;
			case DW_ARG:
				in.dargs.push_back(enc_u32());
				break;
			case DW_DW_ARG:
				in.dargs.push_back(enc_u32());
				in.dargs.push_back(enc_u32());
				break;
			case wW_rW_rW_ARG:
				in.wargs.push_back(int16_t(enc_u16()));
				in.wargs.push_back(int16_t(enc_u16()));
				in.wargs.push_back(int16_t(enc_u16()));
				break;
			case wW_rW_ARG:
			case rW_rW_ARG:
			case wW_W_ARG:
				in.wargs.push_back(int16_t(enc_u16()));
				in.wargs.push_back(int16_t(enc_u16()));
				break;
			case wW_rW_DW_ARG:
			case rW_W_DW_ARG:
				in.wargs.push_back(int16_t(enc_u16()));
				in.wargs.push_back(int16_t(enc_u16()));
				in.dargs.push_back(enc_u32());
				break;
			case rW_DW_DW_ARG:
				in.wargs.push_back(int16_t(enc_u16()));
				in.dargs.push_back(enc_u32());
				in.dargs.push_back(enc_u32());
				break;
			case QW_ARG:
				in.qarg = enc_u64();
				in.has_qarg = true;
				break;
			case QW_DW_ARG:
				in.qarg = enc_u64();
				in.has_qarg = true;
				in.dargs.push_back(enc_u32());
				break;
			case rW_QW_ARG:
			case wW_QW_ARG:
				in.wargs.push_back(int16_t(enc_u16()));
				in.qarg = enc_u64();
				in.has_qarg = true;
				break;
			default:
				ok = false;
				break;
			}
			fn.code.push_back(std::move(in));
		}
	}

	FunctionPtr read_function(bool addToModule) {
		uint8_t c = u8();
		if (c == '\0')
			return nullptr;
		if (c == 'r') {
			uint32_t idx = enc_u32();
			return idx < savedFunctions.size() ? savedFunctions[idx] : nullptr;
		}
		auto fn = std::make_shared<Function>();
		savedFunctions.push_back(fn);
		fn->sig = read_signature();
		if (fn->sig.func_type == F_SCRIPT) {
			TypeInfo* ot = fn->sig.object_type.get();
			if (ot && externalTypes.count(ot)) {
				fn->is_new = false;
				return fn;
			}
			uint8_t bits = u8();
			fn->sig.shared = bits & 1;
			fn->sig.is_explicit = bits & 32;
			fn->sig.dont_cleanup = bits & 2;
			if (bits & 4) {
				fn->sig.external = true;
			} else {
				read_bytecode(*fn);
				fn->has_body = true;
				fn->variable_space = enc_u32();
				if (bits & 8) {
					uint32_t count = enc_u32();
					for (uint32_t k = 0; k < count && ok; ++k) {
						fn->obj_var_types.push_back(read_typeinfo());
						fn->obj_var_pos.push_back(enc_i32());
					}
					if (count > 0)
						fn->obj_vars_on_heap = enc_i32();
					uint32_t info = enc_u32();
					for (uint32_t k = 0; k < info && ok; ++k) {
						enc_u32();
						enc_i32();
						enc_u32();
					}
				}
				if (bits & 16) {
					uint32_t tc = enc_u32();
					for (uint32_t k = 0; k < tc && ok; ++k) {
						enc_u32();
						enc_u32();
					}
				}
				if (!mod.no_debug_info) {
					uint32_t lc = enc_u32();
					for (uint32_t k = 0; k < lc && ok; ++k)
						fn->line_numbers.push_back(enc_i32());
					uint32_t sc = enc_u32();
					int pending = 0;
					for (uint32_t k = 0; k < sc && ok; ++k) {
						if ((k & 1) == 0)
							pending = enc_i32();
						else
							fn->sections.emplace_back(pending, read_str());
					}
					uint32_t vc = enc_u32();
					for (uint32_t k = 0; k < vc && ok; ++k) {
						Variable v;
						v.declared_at = enc_u32();
						v.stack_offset = enc_i32();
						v.name = read_str();
						v.type = read_datatype();
						fn->variables.push_back(std::move(v));
					}
					fn->script_section = read_str();
					fn->declared_at = enc_u32();
					uint32_t pc = uint32_t(enc_u64());
					for (uint32_t k = 0; k < pc && ok; ++k)
						fn->sig.param_names.push_back(read_str());
				}
			}
		} else if (fn->sig.func_type == F_VIRTUAL || fn->sig.func_type == F_INTERFACE) {
			fn->vf_table_idx = enc_u32();
		} else if (fn->sig.func_type == F_FUNCDEF) {
			uint8_t bits = u8();
			fn->sig.shared = bits & 1;
			fn->sig.external = bits & 2;
		}
		if (addToModule)
			mod.functions.push_back(fn);
		return fn;
	}

	void read_type_decl(const TypeInfoPtr& type, int phase, bool* isExternal) {
		if (!type)
			return;
		bool is_interface = (type->flags & ASOBJ_SCRIPT_OBJECT) && type->size == 0;
		if (phase == 1) {
			type->name = read_str();
			type->flags = u32_raw();
			type->size = enc_u32();
			type->name_space = read_str();
			registry[type->name_space + "\x1f" + type->name] = type;
			if (type->flags & ASOBJ_SHARED) {
				uint8_t c = u8();
				if (c == 'e') {
					if (isExternal)
						*isExternal = true;
				}
			}
		} else if (phase == 2) {
			if ((type->flags & ASOBJ_SHARED) && externalTypes.count(type.get()))
				return;
			if (type->flags & ASOBJ_ENUM) {
				uint32_t count = enc_u32();
				for (uint32_t k = 0; k < count && ok; ++k) {
					read_str();
					u32_raw();
				}
			} else if (type->flags & ASOBJ_TYPEDEF) {
				enc_u32();
			} else {
				read_typeinfo();
				uint32_t isize = enc_u32();
				for (uint32_t k = 0; k < isize && ok; ++k) {
					read_typeinfo();
					if (!is_interface)
						enc_u32();
				}
				if (!is_interface) {
					read_function(false);
					uint32_t count = enc_u32();
					for (uint32_t k = 0; k < count && ok; ++k) {
						read_function(false);
						read_function(false);
					}
				}
				uint32_t msize = enc_u32();
				for (uint32_t k = 0; k < msize && ok; ++k)
					read_function(false);
				uint32_t vsize = enc_u32();
				for (uint32_t k = 0; k < vsize && ok; ++k)
					read_function(false);
			}
		} else if (phase == 3) {
			if ((type->flags & ASOBJ_SHARED) && externalTypes.count(type.get()))
				return;
			uint32_t count = enc_u32();
			for (uint32_t k = 0; k < count && ok; ++k) {
				read_str();
				read_datatype();
				enc_u32();
			}
		}
	}

	void parse_module() {
		mod.no_debug_info = enc_u32() != 0;

		// enums
		uint32_t count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i) {
			auto et = std::make_shared<TypeInfo>();
			et->kind = TypeInfo::Object;
			bool ext = false;
			read_type_decl(et, 1, &ext);
			if (ext)
				externalTypes.insert(et.get());
			mod.enum_types.push_back(et);
			read_type_decl(et, 2, nullptr);
		}

		// class types (phase 1)
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i) {
			auto ot = std::make_shared<TypeInfo>();
			ot->kind = TypeInfo::Object;
			bool ext = false;
			read_type_decl(ot, 1, &ext);
			if (ext)
				externalTypes.insert(ot.get());
			mod.class_types.push_back(ot);
		}

		// func defs
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i) {
			auto f = read_function(false);
			if (f)
				mod.funcdefs.push_back(f);
		}

		// interface methods (phase 2)
		for (const auto& t : mod.class_types) {
			if (!ok)
				break;
			bool is_interface = (t->flags & ASOBJ_SCRIPT_OBJECT) && t->size == 0;
			if (is_interface)
				read_type_decl(t, 2, nullptr);
		}
		// class methods (phase 2)
		for (const auto& t : mod.class_types) {
			if (!ok)
				break;
			bool is_interface = (t->flags & ASOBJ_SCRIPT_OBJECT) && t->size == 0;
			if (!is_interface)
				read_type_decl(t, 2, nullptr);
		}
		// class properties (phase 3)
		for (const auto& t : mod.class_types) {
			if (!ok)
				break;
			bool is_interface = (t->flags & ASOBJ_SCRIPT_OBJECT) && t->size == 0;
			if (!is_interface)
				read_type_decl(t, 3, nullptr);
		}

		// typedefs
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i) {
			auto td = std::make_shared<TypeInfo>();
			td->kind = TypeInfo::Object;
			bool ext = false;
			read_type_decl(td, 1, &ext);
			if (ext)
				externalTypes.insert(td.get());
			mod.typedefs.push_back(td);
			read_type_decl(td, 2, nullptr);
		}

		// script globals
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i) {
			GlobalProp g;
			g.name = read_str();
			g.name_space = read_str();
			g.type = read_datatype();
			g.init = read_function(false);
			mod.globals.push_back(std::move(g));
		}

		// script functions
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i)
			read_function(true);

		// global functions (references)
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i) {
			auto f = read_function(false);
			if (f)
				mod.global_functions.push_back(f);
		}

		// bind infos
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i) {
			BindInfo b;
			read_function(false);
			b.import_from = read_str();
			mod.binds.push_back(std::move(b));
		}

		// used types
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i)
			mod.used_types.push_back(read_typeinfo());

		// used type ids
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i)
			mod.used_type_ids.push_back(read_datatype());

		// used functions
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i) {
			uint8_t c = u8();
			if (c == 'n') {
				mod.used_functions.push_back(nullptr);
				mod.used_function_kind.push_back('n');
			} else {
				auto f = std::make_shared<Function>();
				f->is_new = false;
				f->sig = read_signature();
				mod.used_functions.push_back(f);
				mod.used_function_kind.push_back(char(c));
			}
		}

		// used global props
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i) {
			GlobalProp g;
			g.name = read_str();
			g.name_space = read_str();
			g.type = read_datatype();
			g.module_prop = u8() != 0;
			mod.used_globals.push_back(std::move(g));
		}

		// used string constants
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i)
			mod.used_strings.push_back(read_str());

		// used object props
		count = enc_u32();
		for (uint32_t i = 0; i < count && ok; ++i) {
			ObjectProp p;
			p.obj = read_typeinfo();
			p.name = read_str();
			mod.used_obj_props.push_back(std::move(p));
		}
	}

	// --- reference resolution ---------------------------------------------
	void annotate() {
		for (auto& fn : mod.functions)
			annotate_function(*fn);
	}

	void annotate_function(Function& fn) {
		for (size_t i = 0; i < fn.code.size(); ++i) {
			Instruction& in = fn.code[i];
			switch (in.op) {
			case 9:	  // CALL
			case 139: // CALLINTF
			case 61:  // CALLSYS
			case 200: // Thiscall1
				in.ann = call_ann(in.dargs.empty() ? 0 : in.dargs[0]);
				break;
			case 176: // FuncPtr
				in.ann = "&" + call_ann(uint64_t(in.qarg));
				break;
			case 62: // CALLBND
				if (!in.dargs.empty() && in.dargs[0] < mod.binds.size())
					in.ann = "bind:" + mod.binds[in.dargs[0]].sig.decl();
				break;
			case 64: { // ALLOC
				std::string s = mod.type_name(in.qarg);
				if (!in.dargs.empty() && in.dargs[0] != 0)
					s += " ctor=" + call_ann(in.dargs[0] - 1);
				in.ann = s;
				break;
			}
			case 69:  // REFCPY
			case 75:  // OBJTYPE
			case 65:  // FREE
			case 186: // RefCpyV
				in.ann = mod.type_name(in.qarg);
				break;
			case 76:  // TYPEID
			case 144: // Cast
			case 46:  // COPY
			case 192: // SetListType
				if (!in.dargs.empty())
					in.ann = mod.typeid_name(in.dargs[0]);
				break;
			case 79:  // ADDSi
			case 178: // LoadThisR
				if (!in.wargs.empty())
					in.ann = mod.objprop_name(uint64_t(in.wargs[0]));
				break;
			case 184: // LoadRObjR
			case 185: // LoadVObjR
				if (in.wargs.size() >= 2)
					in.ann = mod.objprop_name(uint64_t(in.wargs[1]));
				break;
			case 1:	  // PshGPtr
			case 7:	  // PshG4
			case 8:	  // LdGRdR4
			case 84:  // CpyVtoG4
			case 87:  // CpyGtoV4
			case 96:  // LDG
			case 98:  // PGA
			case 136: // SetG4
				in.ann = global_or_string(in.qarg);
				break;
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
			case 16:
			case 17:
			case 187: // JLowZ
			case 188: // JLowNZ
				if (!in.dargs.empty()) {
					int delta = int(int32_t(in.dargs[0]));
					in.jump_target = int(i) + 1 + delta;
				}
				break;
			default:
				break;
			}
		}
	}

	std::string call_ann(uint64_t idx) const {
		if (idx >= mod.used_functions.size())
			return "func#" + std::to_string(idx);
		char kind = idx < mod.used_function_kind.size() ? mod.used_function_kind[idx] : '?';
		const auto& f = mod.used_functions[idx];
		std::string decl = f ? f->sig.decl() : "null";
		return std::string("[") + kind + "] " + decl;
	}

	std::string global_or_string(uint64_t v) const {
		uint64_t idx = v >> 1;
		if (v & 1) {
			if (idx < mod.used_globals.size())
				return "g:" + mod.used_globals[idx].name;
			return "g#" + std::to_string(idx);
		}
		if (idx < mod.used_strings.size())
			return "\"" + mod.used_strings[idx] + "\"";
		return "str#" + std::to_string(idx);
	}

	std::span<const uint8_t> data;
	size_t pos = 0;
	bool ok = true;
	Module mod;
	std::vector<std::string> savedStrings;
	std::vector<DataType> savedDataTypes;
	std::vector<FunctionPtr> savedFunctions;
	std::unordered_map<std::string, TypeInfoPtr> registry;
	std::unordered_set<TypeInfo*> externalTypes;
};

} // namespace

// --- Module helpers ---------------------------------------------------------
const std::string& Module::string_at(uint64_t encoded) const {
	static const std::string empty;
	uint64_t idx = encoded >> 1;
	return idx < used_strings.size() ? used_strings[idx] : empty;
}
std::string Module::function_name(uint64_t idx) const {
	if (idx >= used_functions.size() || !used_functions[idx])
		return "?";
	return used_functions[idx]->sig.decl();
}
std::string Module::function_kind(uint64_t idx) const {
	if (idx >= used_function_kind.size())
		return "";
	return std::string(1, used_function_kind[idx]);
}
std::string Module::type_name(uint64_t idx) const {
	return idx < used_types.size() && used_types[idx] ? used_types[idx]->format() : "?";
}
std::string Module::typeid_name(uint64_t idx) const {
	return idx < used_type_ids.size() ? used_type_ids[idx].format() : "?";
}
std::string Module::objprop_name(uint64_t idx) const {
	if (idx >= used_obj_props.size())
		return "prop#" + std::to_string(idx);
	const auto& p = used_obj_props[idx];
	return (p.obj ? p.obj->format() : "?") + "." + p.name;
}

Module parse(std::span<const uint8_t> data) { return Parser(data).run(); }

CharTable load_char_table(const std::string& path) {
	CharTable t;
	std::ifstream f(path);
	std::string line;
	while (std::getline(f, line)) {
		if (line.empty() || line[0] == '#')
			continue;
		std::vector<std::string> cols;
		size_t s = 0;
		for (size_t i = 0; i <= line.size(); ++i)
			if (i == line.size() || line[i] == '\t') {
				cols.push_back(line.substr(s, i - s));
				s = i + 1;
			}
		if (cols.size() >= 4)
			t[cols[0]] = {cols[1], cols[2], cols[3]};
	}
	return t;
}

std::string FacilityNames::resolve(const std::string& key) const {
	if (!key.empty() && key.find_first_not_of("0123456789") == std::string::npos) {
		const unsigned long idx = std::stoul(key);
		if (idx < by_index.size())
			return by_index[idx];
	}
	auto it = by_name.find(key);
	if (it != by_name.end())
		return it->second;
	return {};
}

FacilityNames load_facility_names(const std::string& uc_enums_tsv) {
	FacilityNames out;
	std::ifstream f{uc_enums_tsv};
	std::string line;
	while (std::getline(f, line)) {
		if (line.empty() || line[0] == '#')
			continue;
		const size_t a = line.find('\t');
		if (a == std::string::npos || line.substr(0, a) != "ST_FACILITYID")
			continue;
		const size_t b = line.find('\t', a + 1);
		if (b == std::string::npos)
			continue;
		const std::string name = line.substr(a + 1, b - a - 1);
		const std::string label = line.substr(b + 1);
		if (name.empty())
			continue;
		out.by_name.emplace(name, label);
		out.by_index.push_back(label);
	}
	return out;
}

const StageTable::Stage* StageTable::stage(const std::string& name) const {
	auto it = stages.find(name);
	if (it != stages.end())
		return &it->second;
	std::string up;
	up.reserve(name.size());
	for (char c : name)
		up.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
	it = stages.find(up);
	return it == stages.end() ? nullptr : &it->second;
}

const StageTable::Facility* StageTable::facility(const std::string& name) const {
	auto it = facilities.find(name);
	return it == facilities.end() ? nullptr : &it->second;
}

int StageTable::country_at(const float p[2]) const {
	static const int map[5] = {3, 4, 1, 0, 0};
	for (const auto& r : countries) {
		bool inside = true;
		int sign = 0;
		for (int i = 0; i < 4 && inside; ++i) {
			const float ax = r.p[2 * i], ay = r.p[2 * i + 1];
			const float bx = r.p[(2 * i + 2) % 8], by = r.p[(2 * i + 3) % 8];
			const float cross = (bx - ax) * (p[1] - ay) - (by - ay) * (p[0] - ax);
			const int s = cross > 1e-4f ? 1 : cross < -1e-4f ? -1 : 0;
			if (s) {
				if (sign && s != sign)
					inside = false;
				sign = s;
			}
		}
		if (inside)
			return r.id >= 0 && r.id < 5 ? map[r.id] : r.id;
	}
	return 2;
}

StageTable
load_stage_tables(const std::string& stages_tsv, const std::string& facility_tsv, const std::string& country_tsv) {
	auto split = [](const std::string& line) {
		std::vector<std::string> c;
		size_t s = 0;
		for (size_t i = 0; i <= line.size(); ++i)
			if (i == line.size() || line[i] == '\t') {
				c.push_back(line.substr(s, i - s));
				s = i + 1;
			}
		return c;
	};

	StageTable out;
	if (std::ifstream f{stages_tsv}) {
		std::string line;
		while (std::getline(f, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			auto c = split(line);
			if (c.size() < 2 || c[0].empty())
				continue;
			StageTable::Stage s;
			s.base_lv = std::atoi(c[1].c_str());
			s.cue = c.size() > 2 ? std::atoi(c[2].c_str()) : -1;
			out.stages.emplace(c[0], s);
		}
	}
	if (std::ifstream f{facility_tsv}) {
		std::string line;
		while (std::getline(f, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			auto c = split(line);
			if (c.size() < 4)
				continue;
			StageTable::Facility fac;
			fac.label = c[1];
			fac.pos[0] = static_cast<float>(std::atof(c[2].c_str()));
			fac.pos[1] = static_cast<float>(std::atof(c[3].c_str()));
			if (!c[0].empty())
				out.facilities.emplace(c[0], fac);
			if (!fac.label.empty())
				out.facilities.emplace(fac.label, fac);
		}
	}
	if (std::ifstream f{country_tsv}; !country_tsv.empty() && f) {
		std::string line;
		while (std::getline(f, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			auto c = split(line);
			if (c.size() < 9)
				continue;
			StageTable::CountryRegion r;
			r.id = std::atoi(c[0].c_str());
			for (int k = 0; k < 8; ++k)
				r.p[k] = static_cast<float>(std::atof(c[static_cast<size_t>(k) + 1].c_str()));
			out.countries.push_back(r);
		}
	}
	return out;
}

const std::vector<DropTableEntry>* DropTables::resolve(int id) const {
	return id >= 0 && static_cast<size_t>(id) < by_id.size() ? &by_id[static_cast<size_t>(id)] : nullptr;
}

std::string DropTables::name_at(int id) const {
	return id >= 0 && static_cast<size_t>(id) < names.size() ? names[static_cast<size_t>(id)] : std::string();
}

std::string DropTables::item_name(int id) const {
	return id >= 0 && static_cast<size_t>(id) < item_names.size() ? item_names[static_cast<size_t>(id)] : std::string();
}

DropTables load_drop_tables(const std::string& drop_tables_tsv, const std::string& uc_enums_tsv) {
	DropTables out;
	if (std::ifstream f{uc_enums_tsv}) {
		std::string line;
		while (std::getline(f, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			const size_t a = line.find('\t');
			if (a == std::string::npos)
				continue;
			const size_t b = line.find('\t', a + 1);
			if (b == std::string::npos)
				continue;
			const std::string kind = line.substr(0, a);
			const std::string name = line.substr(a + 1, b - a - 1);
			if (kind == "DROPTABLE") {
				out.names.push_back(name);
			} else if (kind == "ITEMID") {
				out.item_ids.emplace(name, static_cast<int>(out.item_names.size()));
				out.item_names.push_back(name);
			}
		}
	}
	out.by_id.resize(out.names.size());

	std::unordered_map<std::string, int> drop_ids;
	for (int i = 0; i < static_cast<int>(out.names.size()); ++i)
		drop_ids.emplace(out.names[static_cast<size_t>(i)], i);

	if (std::ifstream f{drop_tables_tsv}) {
		std::string line;
		while (std::getline(f, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			auto split = [&](const std::string& s) {
				std::vector<std::string> c;
				size_t p = 0;
				for (size_t i = 0; i <= s.size(); ++i)
					if (i == s.size() || s[i] == '\t') {
						c.push_back(s.substr(p, i - p));
						p = i + 1;
					}
				return c;
			};
			auto c = split(line);
			if (c.size() < 2)
				continue;
			auto it = drop_ids.find(c[0]);
			if (it == drop_ids.end())
				continue;
			DropTableEntry e;
			e.item = c[1];
			e.weight = c.size() > 2 ? std::atoi(c[2].c_str()) : 0;
			e.count = c.size() > 3 ? std::atoi(c[3].c_str()) : 0;
			e.add_index = c.size() > 4 ? std::atoi(c[4].c_str()) : 0;
			out.by_id[static_cast<size_t>(it->second)].push_back(std::move(e));
		}
	}
	return out;
}

CharaSetTable load_chara_set(
	const std::string& charaset_tsv,
	const std::string& classes_tsv,
	const std::string& charanpc_tsv,
	const std::string& chara_colors_tsv) {
	auto split = [](const std::string& line) {
		std::vector<std::string> c;
		size_t s = 0;
		for (size_t i = 0; i <= line.size(); ++i)
			if (i == line.size() || line[i] == '\t') {
				c.push_back(line.substr(s, i - s));
				s = i + 1;
			}
		return c;
	};

	std::unordered_map<std::string, CharaInfo> classes;
	if (std::ifstream f{classes_tsv}) {
		std::string line;
		while (std::getline(f, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			auto c = split(line);
			if (c.size() < 4)
				continue;
			CharaInfo ci;
			ci.disp_name = c[1];
			ci.mbs = c[2].find('/') == std::string::npos ? "Chara/" + c[2] : c[2];
			ci.ftx = c[3].find('/') == std::string::npos ? "Chara/" + c[3] : c[3];
			classes.emplace(c[0], std::move(ci));
		}
	}

	std::vector<std::array<uint32_t, 4>> color_presets;
	std::vector<std::array<uint8_t, 8>> color_appearance;
	if (!chara_colors_tsv.empty()) {
		if (std::ifstream f{chara_colors_tsv}) {
			std::string line;
			while (std::getline(f, line)) {
				if (line.empty() || line[0] == '#')
					continue;
				auto c = split(line);
				if (c.size() < 5)
					continue;
				const int idx = std::atoi(c[0].c_str());
				if (idx < 0)
					continue;
				if (static_cast<size_t>(idx) >= color_presets.size()) {
					color_presets.resize(static_cast<size_t>(idx) + 1);
					color_appearance.resize(static_cast<size_t>(idx) + 1);
				}
				for (int i = 0; i < 4; ++i)
					color_presets[static_cast<size_t>(idx)][i] =
						static_cast<uint32_t>(std::strtoul(c[static_cast<size_t>(i) + 1].c_str(), nullptr, 0));
				for (int i = 0; i < 8 && static_cast<size_t>(i) + 5 < c.size(); ++i)
					color_appearance[static_cast<size_t>(idx)][static_cast<size_t>(i)] =
						static_cast<uint8_t>(std::atoi(c[static_cast<size_t>(i) + 5].c_str()));
			}
		}
	}

	CharaSetTable out;
	if (std::ifstream f{charaset_tsv}) {
		std::string line;
		while (std::getline(f, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			auto c = split(line);
			if (c.size() < 7)
				continue;
			const std::string& name = c[1];
			const int set_idx = std::atoi(c[0].c_str());
			if (set_idx >= 0) {
				if (static_cast<size_t>(set_idx) >= out.names.size())
					out.names.resize(static_cast<size_t>(set_idx) + 1);
				out.names[static_cast<size_t>(set_idx)] = name;
			}
			const std::string& cls = c[2];
			const std::string& gender = c[3];
			const std::string& label = c[6];
			if (cls.empty() || cls == "UNKNOWN")
				continue;
			auto it = classes.find(cls + "_" + gender);
			if (it == classes.end())
				continue;
			CharaInfo ci = it->second;
			ci.appearance.variant = static_cast<uint32_t>(std::atoi(c[4].c_str()));
			if (c.size() > 7 && !c[7].empty())
				ci.appearance.mask = static_cast<uint32_t>(std::strtoul(c[7].c_str(), nullptr, 0));
			if (c.size() > 8 && !c[8].empty()) {
				ci.appearance.preset = static_cast<uint32_t>(std::atoi(c[8].c_str()));
				if (ci.appearance.preset < color_presets.size()) {
					for (int i = 0; i < 4; ++i)
						ci.appearance.palette[i] = color_presets[ci.appearance.preset][static_cast<size_t>(i)];
					if (ci.appearance.preset < color_appearance.size())
						for (int i = 0; i < 8; ++i)
							ci.appearance.parts[i] = color_appearance[ci.appearance.preset][static_cast<size_t>(i)];
					ci.appearance.color_mask = 1u << 1 | 1u << 2 | 1u << 3;
				}
			}
			if (!label.empty())
				out.by_key.emplace(label, ci);
			if (!name.empty())
				out.by_key.emplace(name, std::move(ci));
		}
	}
	// CharaNpc (NPCTYPE) table: key both the JP display name (what
	// `FUN_0011aac0` searches) and the enum name to the full CharaNpc/ asset.
	// Resolves DrCreateNpc's def name.
	if (!charanpc_tsv.empty()) {
		if (std::ifstream f{charanpc_tsv}) {
			std::string line;
			while (std::getline(f, line)) {
				if (line.empty() || line[0] == '#')
					continue;
				auto c = split(line);
				if (c.size() < 5)
					continue;
				CharaInfo ci;
				ci.disp_name = c[2];
				ci.mbs = c[3];
				ci.ftx = c[4];
				if (!c[2].empty())
					out.by_key.emplace(c[2], ci);
				if (!c[1].empty())
					out.by_key.emplace(c[1], ci);
			}
		}
	}
	return out;
}

// --- Formatting -------------------------------------------------------------
std::string token_name(int token) {
	switch (token) {
	case ttVoid:
		return "void";
	case ttBool:
		return "bool";
	case ttInt:
		return "int";
	case ttInt8:
		return "int8";
	case ttInt16:
		return "int16";
	case ttInt64:
		return "int64";
	case ttUInt:
		return "uint";
	case ttUInt8:
		return "uint8";
	case ttUInt16:
		return "uint16";
	case ttUInt64:
		return "uint64";
	case ttFloat:
		return "float";
	case ttDouble:
		return "double";
	case ttAuto:
		return "auto";
	case ttNull:
		return "null";
	default:
		return "tt" + std::to_string(int(token));
	}
}

std::string TypeInfo::format() const {
	switch (kind) {
	case Template: {
		std::string s = name + "<";
		for (size_t i = 0; i < sub_types.size(); ++i) {
			if (i)
				s += ",";
			s += sub_types[i].format();
		}
		return s + ">";
	}
	case List:
		return (list_of ? list_of->format() : std::string("?")) + "{}";
	case SubType:
		return name;
	case Child:
		return (parent ? parent->format() : std::string("?")) + "::" + name;
	case Object:
		return name_space.empty() ? name : name_space + "::" + name;
	default:
		return {};
	}
}

std::string DataType::format() const {
	std::string s;
	if (is_read_only)
		s += "const ";
	if (token == ttIdentifier)
		s += type_info ? type_info->format() : "?";
	else
		s += token_name(token);
	if (is_reference)
		s += "&";
	if (is_object_handle)
		s += "@";
	return s;
}

std::string DataType::as_decl() const {
	std::string s;
	if (is_read_only)
		s += "const ";
	if (token == ttIdentifier)
		s += type_info ? type_info->format() : "?";
	else
		s += token_name(token);
	if (is_object_handle) {
		s += "@";
		if (handle_to_const)
			s += "const";
	}
	if (is_reference)
		s += "&";
	return s;
}

std::string Signature::decl() const {
	std::string s = return_type.format();
	s += " ";
	if (object_type)
		s += object_type->format() + "::";
	else if (!name_space.empty())
		s += name_space + "::";
	s += name;
	s += "(";
	for (size_t i = 0; i < params.size(); ++i) {
		if (i)
			s += ", ";
		if (i < inout_flags.size() && inout_flags[i] == 1)
			s += "in ";
		else if (i < inout_flags.size() && inout_flags[i] == 2)
			s += "out ";
		else if (i < inout_flags.size() && inout_flags[i] == 3)
			s += "inout ";
		s += params[i].format();
	}
	s += ")";
	return s;
}

std::string Signature::bare_decl(bool include_namespace) const {
	std::string s = return_type.as_decl();
	s += " ";
	if (include_namespace && !name_space.empty())
		s += name_space + "::";
	s += name;
	s += "(";
	for (size_t i = 0; i < params.size(); ++i) {
		if (i)
			s += ", ";
		s += params[i].as_decl();
		if (params[i].is_reference && i < inout_flags.size()) {
			switch (inout_flags[i]) {
			case 1:
				s += "in";
				break;
			case 2:
				s += "out";
				break;
			case 3:
				s += "inout";
				break;
			default:
				break;
			}
		}
	}
	s += ")";
	if (is_readonly)
		s += " const";
	return s;
}

// --- Disassembly ------------------------------------------------------------
std::string disassemble(const Module&, const Function& fn) {
	std::string out;
	out += "function " + fn.sig.decl() + "  [" + func_type_name(fn.sig.func_type) + "]";
	if (fn.sig.object_type)
		out += " method-of=" + fn.sig.object_type->format();
	if (fn.sig.shared)
		out += " shared";
	if (fn.sig.external)
		out += " external";
	out += "\n";
	out += "  section=" + (fn.script_section.empty() ? std::string("?") : fn.script_section) +
		   "  line=" + std::to_string(fn.declared_at) + "  variableSpace=" + std::to_string(fn.variable_space) + "\n";
	if (!fn.has_body)
		return out + "  (no body)\n";

	for (size_t i = 0; i < fn.code.size(); ++i) {
		const Instruction& in = fn.code[i];
		char head[64];
		std::snprintf(head, sizeof(head), "%5zu  %5x  %-11s ", i, in.pos, opinfo(in.op).name);
		out += head;
		for (auto w : in.wargs)
			out += std::to_string(w) + " ";
		for (auto d : in.dargs)
			out += std::to_string(int32_t(d)) + " ";
		if (in.has_qarg) {
			char t[32];
			std::snprintf(t, sizeof(t), "@0x%llx ", static_cast<unsigned long long>(in.qarg));
			out += t;
		}
		if (in.jump_target >= 0)
			out += "-> " + std::to_string(in.jump_target) + " ";
		if (!in.ann.empty())
			out += " ; " + in.ann;
		out += "\n";
	}
	return out;
}

// --- Scene extraction (Tier 1: argument lifting, Tier 2: structuring) -------
namespace {

struct Val {
	enum K { N32, N64, S, O } k = O;
	uint64_t raw = 0;
	std::string s;
};

float as_f32(uint32_t u) {
	float f;
	std::memcpy(&f, &u, 4);
	return f;
}
double as_f64(uint64_t u) {
	double d;
	std::memcpy(&d, &u, 8);
	return d;
}

} // namespace

Scene extract_scene(const Module& mod) {
	Scene scene;

	// Resolve module globals that are initialized from string constants.
	std::unordered_map<std::string, std::vector<std::string>> global_strings;
	for (const auto& g : mod.globals) {
		if (!g.init)
			continue;
		for (const auto& in : g.init->code) {
			if ((in.op == 98 || in.op == 1) && in.has_qarg && !(in.qarg & 1)) {
				uint64_t idx = in.qarg >> 1;
				if (idx < mod.used_strings.size())
					global_strings[g.name].push_back(mod.used_strings[idx]);
			}
		}
	}

	auto call_name = [&](uint64_t idx, std::string& decl, char& kind) -> std::string {
		decl.clear();
		kind = '?';
		if (idx < mod.used_functions.size() && mod.used_functions[idx]) {
			decl = mod.used_functions[idx]->sig.decl();
			if (idx < mod.used_function_kind.size())
				kind = mod.used_function_kind[idx];
			return mod.used_functions[idx]->sig.name;
		}
		return {};
	};

	for (const auto& fnp : mod.functions) {
		const Function& fn = *fnp;
		if (!fn.has_body)
			continue;
		SceneFunction sf;
		sf.name = fn.sig.name;
		sf.decl = fn.sig.decl();
		sf.section = fn.script_section;

		std::vector<Val> pending;
		std::vector<double> lastvec;
		std::string pending_import;
		bool has_vec = false;

		auto flush = [&](const std::string& cname, const std::string& cdecl_str, char ckind, bool is_ctor) {
			std::vector<double> floats;
			std::vector<int64_t> ints;
			for (const Val& v : pending) {
				if (v.k == Val::S)
					continue;
				if (v.k == Val::N32) {
					int32_t i;
					std::memcpy(&i, &v.raw, 4);
					ints.push_back(i);
					floats.push_back(as_f32(uint32_t(v.raw)));
				} else if (v.k == Val::N64) {
					ints.push_back(int64_t(v.raw));
					floats.push_back(as_f64(v.raw));
				}
			}
			if (is_ctor) {
				lastvec.clear();
				const size_t n = std::min<size_t>(floats.size(), cname == "FnVector3" ? 3 : 2);
				for (size_t k = floats.size() - n; k < floats.size(); ++k)
					lastvec.push_back(floats[k]);
				has_vec = !lastvec.empty();
				size_t remaining = n;
				for (size_t i = pending.size(); i > 0 && remaining > 0;) {
					--i;
					if (pending[i].k == Val::N32 || pending[i].k == Val::N64) {
						pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(i));
						--remaining;
					}
				}
				return;
			}
			SceneEvent ev;
			ev.call = cname;
			ev.decl = cdecl_str;
			ev.import_from = pending_import;
			ev.kind = ckind;
			for (const Val& v : pending)
				if (v.k == Val::S)
					ev.strings.push_back(v.s);
			ev.ints = ints;
			ev.floats = floats;
			if (has_vec) {
				if (lastvec.size() >= 3) {
					ev.vec3 = lastvec;
					if (lastvec.size() == 3)
						ev.vec2 = {lastvec[0], lastvec[1]};
				} else
					ev.vec2 = lastvec;
			}
			sf.events.push_back(std::move(ev));
			pending.clear();
			lastvec.clear();
			pending_import.clear();
			has_vec = false;
		};

		for (const auto& in : fn.code) {
			switch (in.op) {
			case 2: // PshC4
				pending.push_back({Val::N32, in.dargs.empty() ? 0 : in.dargs[0], {}});
				break;
			case 47: // PshC8
				pending.push_back({Val::N64, in.qarg, {}});
				break;
			case 1:	  // PshGPtr
			case 7:	  // PshG4
			case 8:	  // LdGRdR4
			case 84:  // CpyVtoG4
			case 96:  // LDG
			case 98:  // PGA
			case 136: // SetG4
				if (in.has_qarg) {
					uint64_t v = in.qarg;
					uint64_t idx = v >> 1;
					if (v & 1) {
						if (idx < mod.used_globals.size()) {
							const auto& g = mod.used_globals[idx];
							auto it = global_strings.find(g.name);
							if (it != global_strings.end())
								for (const auto& s : it->second)
									pending.push_back({Val::S, 0, s});
							else
								pending.push_back({Val::O, 0, "g:" + g.name});
						}
					} else if (idx < mod.used_strings.size()) {
						pending.push_back({Val::S, 0, mod.used_strings[idx]});
					}
				}
				break;
			case 9:	   // CALL
			case 139:  // CALLINTF
			case 61:   // CALLSYS
			case 200:  // Thiscall1
			case 62: { // CALLBND
				std::string cname, cdecl_str;
				char ckind = '?';
				if (in.op == 62) {
					if (!in.dargs.empty() && in.dargs[0] < mod.binds.size()) {
						cdecl_str = mod.binds[in.dargs[0]].sig.decl();
						cname = mod.binds[in.dargs[0]].sig.name;
						pending_import = mod.binds[in.dargs[0]].import_from;
					}
				} else if (!in.dargs.empty()) {
					cname = call_name(in.dargs[0], cdecl_str, ckind);
				}
				bool ctor =
					(cname == "$beh0" &&
					 (cdecl_str.rfind("void FnVector2::", 0) == 0 || cdecl_str.rfind("void FnVector3::", 0) == 0));
				std::string vecname = cdecl_str.rfind("void FnVector3::", 0) == 0 ? "FnVector3" : "FnVector2";
				if (ctor)
					flush(vecname, cdecl_str, ckind, true);
				else
					flush(cname, cdecl_str, ckind, false);
				break;
			}
			default:
				break;
			}
		}
		if (!sf.events.empty())
			scene.functions.push_back(std::move(sf));
	}
	return scene;
}

// --- Scene IR: fold the event stream into entities (Tier 3 state pass) -------
namespace {

bool ends_with(const std::string& s, const char* suf) {
	size_t n = std::strlen(suf);
	return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}
} // namespace

bool is_sprite_create_call(std::string_view c) {
	return c == "CreateSpriteMone" || c == "CreateSpriteCoA" || c == "CreateSpriteEffect" ||
		   c == "CreateSpriteMoneWorld" || c == "CreateSpriteMoneStageStart" || c == "DrUiCreateChara";
}

bool is_chara_create_call(std::string_view c) {
	return c == "DrCreateChara" || c == "DrCreateNpc" || c == "DrUiCreateChara";
}

bool is_entity_create_call(std::string_view c) { return is_sprite_create_call(c) || is_chara_create_call(c); }

SceneIR
build_scene_ir(const SceneFunction& fn, const std::string& source, const CharTable* chars, const CharaSetTable* sets) {
	SceneIR ir;
	ir.source = source;
	ir.function = fn.name;
	std::unordered_map<std::string, int> by_id;

	auto find = [&](const std::vector<std::string>& ss) -> int {
		for (const auto& s : ss) {
			auto it = by_id.find(s);
			if (it != by_id.end())
				return it->second;
		}
		return -1;
	};
	auto plausible_scale = [](double d) { return std::isfinite(d) && d > 1e-4 && d < 1e4; };

	for (const auto& e : fn.events) {
		const std::string& c = e.call;

		if (is_sprite_create_call(c)) {
			SceneEntity ent;
			ent.kind = c;
			for (const auto& s : e.strings) {
				if (ends_with(s, ".mbs"))
					ent.mbs = s;
				else if (ends_with(s, ".ftx"))
					ent.ftx = s;
				else if (!s.empty())
					ent.aliases.push_back(s);
			}
			if (ent.mbs.empty() && ent.ftx.empty())
				continue;
			ent.id = ent.aliases.empty() ? ent.mbs : ent.aliases[0];
			if (e.vec3.size() >= 3) {
				ent.pos[0] = float(e.vec3[0]);
				ent.pos[1] = float(e.vec3[1]);
				ent.pos[2] = float(e.vec3[2]);
			} else if (e.vec2.size() >= 2) {
				ent.pos[0] = float(e.vec2[0]);
				ent.pos[1] = float(e.vec2[1]);
			}
			ent.order = int(ir.entities.size());
			ir.entities.push_back(ent);
			int idx = int(ir.entities.size()) - 1;
			by_id[ent.id] = idx;
			for (const auto& a : ent.aliases)
				by_id[a] = idx;
			continue;
		}

		if (is_chara_create_call(c)) {
			SceneEntity ent;
			ent.kind = c;
			ent.aliases = e.strings;
			ent.id = e.strings.size() >= 2 ? e.strings[1] : (e.strings.empty() ? std::string() : e.strings[0]);
			// World characters carry no asset in the call; resolve the engine's
			// CharaSet table (keyed by the Japanese label) before the hand table.
			if (sets) {
				for (const auto& s : e.strings) {
					if (s.empty())
						continue;
					if (const CharaInfo* ci = sets->resolve(s)) {
						ent.mbs = ci->mbs;
						ent.ftx = ci->ftx;
						break;
					}
				}
			}
			if (ent.mbs.empty() && chars) {
				auto it = chars->find(ent.id);
				if (it == chars->end() && !e.strings.empty())
					it = chars->find(e.strings[0]);
				if (it != chars->end()) {
					ent.mbs = it->second.mbs;
					ent.ftx = it->second.ftx;
				}
			}
			if (e.vec3.size() >= 3) {
				ent.pos[0] = float(e.vec3[0]);
				ent.pos[1] = float(e.vec3[1]);
				ent.pos[2] = float(e.vec3[2]);
			} else if (e.vec2.size() >= 2) {
				ent.pos[0] = float(e.vec2[0]);
				ent.pos[1] = float(e.vec2[1]);
			}
			ent.order = int(ir.entities.size());
			ir.entities.push_back(ent);
			int idx = int(ir.entities.size()) - 1;
			by_id[ent.id] = idx;
			for (const auto& a : ent.aliases)
				by_id[a] = idx;
			continue;
		}

		if (c == "DeleteSprite" || c == "DrUiDeleteChara" || c == "DrDeleteEffect") {
			int i = find(e.strings);
			if (i >= 0)
				ir.entities[i].visible = false;
			continue;
		}

		if (c == "DrSetPosV" || c == "SetSpritePosV" || c == "DrUiSetPosV" || c == "DrSetOffsetPosV" ||
			c == "DrSetEffectPosV") {
			int i = find(e.strings);
			if (i < 0)
				continue;
			if (e.vec3.size() >= 3) {
				ir.entities[i].pos[0] = float(e.vec3[0]);
				ir.entities[i].pos[1] = float(e.vec3[1]);
				ir.entities[i].pos[2] = float(e.vec3[2]);
			} else if (e.vec2.size() >= 2) {
				ir.entities[i].pos[0] = float(e.vec2[0]);
				ir.entities[i].pos[1] = float(e.vec2[1]);
			}
			continue;
		}

		if (c == "DrSetScale" || c == "SetSpriteScale" || c == "DrUiSetScale" || c == "SetSpriteMoneScale") {
			int i = find(e.strings);
			if (i < 0)
				continue;
			if (!e.floats.empty() && plausible_scale(e.floats[0]))
				ir.entities[i].scale = float(e.floats[0]);
			continue;
		}

		if (c == "DrSetIdleAnimation" || c == "DrAnimation" || c == "DrChangeAnimation") {
			int i = find(e.strings);
			if (i < 0)
				continue;
			for (const auto& s : e.strings) {
				if (by_id.find(s) == by_id.end() && !ends_with(s, ".mbs") && !ends_with(s, ".ftx")) {
					ir.entities[i].track = s;
					break;
				}
			}
			continue;
		}

		if (c == "SetSpriteMoneAnimation" || c == "SetSpriteMoneAnimationPaused") {
			int i = find(e.strings);
			if (i >= 0 && !e.ints.empty())
				ir.entities[i].track_index = int(e.ints[0]);
			continue;
		}

		if (c == "DrSetCameraPosV" || c == "SetCameraPosV" || c == "DrSetOffsetPosV") {
			SceneCamera key;
			if (e.vec3.size() >= 3) {
				key.pos[0] = float(e.vec3[0]);
				key.pos[1] = float(e.vec3[1]);
				key.pos[2] = float(e.vec3[2]);
			} else if (e.vec2.size() >= 2) {
				key.pos[0] = float(e.vec2[0]);
				key.pos[1] = float(e.vec2[1]);
			} else {
				continue;
			}
			ir.camera.push_back(key);
			continue;
		}

		if (c == "DrSetCameraZoom" || c == "DrSetCameraParam" || c == "SetCameraParam") {
			if (!ir.camera.empty() && !e.floats.empty() && plausible_scale(e.floats[0])) {
				ir.camera.back().zoom = float(e.floats[0]);
				ir.camera.back().has_zoom = true;
			}
			continue;
		}

		if (c == "DrCreateStage" || c == "FlCreateStage" || c == "StSetBattleScene") {
			for (int v : e.ints)
				ir.stage.push_back(v);
			continue;
		}
	}

	return ir;
}

namespace {

size_t count_creates(const SceneFunction& f) {
	size_t c = 0;
	for (const auto& e : f.events)
		if (is_entity_create_call(e.call))
			++c;
	return c;
}

} // namespace

const SceneFunction* pick_scene_function(const Scene& scene, const std::string& wanted) {
	if (!wanted.empty()) {
		for (const auto& f : scene.functions)
			if (f.name == wanted)
				return &f;
	}
	auto named = [&](const char* n) -> const SceneFunction* {
		for (const auto& f : scene.functions)
			if (f.name == n)
				return &f;
		return nullptr;
	};
	if (const SceneFunction* c = named("MAIN"); c && c->decl == "void MAIN()")
		return c;
	if (const SceneFunction* c = named("scene00"); c && count_creates(*c) > 0)
		return c;
	for (const auto& f : scene.functions)
		if (f.name.rfind("scene", 0) == 0 && count_creates(f) > 0)
			return &f;
	const SceneFunction* best = nullptr;
	size_t best_n = 0;
	for (const auto& f : scene.functions) {
		size_t c = count_creates(f);
		if (c > best_n) {
			best_n = c;
			best = &f;
		}
	}
	if (best)
		return best;
	for (const auto& f : scene.functions)
		if (!f.events.empty())
			return &f;
	return nullptr;
}

} // namespace asb
