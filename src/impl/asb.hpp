#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace asb {

enum : uint8_t {
	BC_CALL = 0x09, BC_RET = 0x0A, BC_JMP = 0x0B,
	BC_JZ = 0x0C, BC_STR = 0x3C, BC_CALLSYS = 0x3D,
	BC_CALLBND = 0x3E, BC_SUSPEND = 0x3F, BC_ALLOC = 0x40,
	BC_LDG = 0x5E, BC_LDV = 0x61, BC_PGA = 0x62, BC_VAR = 0x63,
};

constexpr const char* bc_names[] = {
	"PopPtr",   "PshGPtr",  "PshC4",    "PshV4",    "PSF",     "SwapPtr",
	"NOT",      "PshG4",    "LdGRdR4",  "CALL",     "RET",     "JMP",
	"JZ",       "JNZ",      "JS",       "JNS",      "JP",      "JNP",
	"TZ",       "TNZ",      "TS",       "TNS",      "TP",      "TNP",
	"NEGi",     "NEGf",     "NEGd",     "INCi16",   "INCi8",   "DECi16",
	"DECi8",    "INCi",     "DECi",     "INCf",     "DECf",    "INCd",
	"DECd",     "IncVi",    "DecVi",    "BNOT",     "BAND",    "BOR",
	"BXOR",     "BSLL",     "BSRL",     "BSRA",     "COPY",    "PshC8",
	"PshVPtr",  "RDSPtr",   "CMPd",     "CMPu",     "CMPf",    "CMPi",
	"CMPIi",    "CMPIf",    "CMPIu",    "JMPP",     "PopRPtr", "PshRPtr",
	"STR",      "CALLSYS",  "CALLBND",  "SUSPEND",
	"ALLOC",    "FREE",     "LOADOBJ",  "STOREOBJ",
	"GETOBJ",   "REFCPY",   "CHKREF",   "GETOBJREF",
	"GETREF",   "PshNull",  "ClrVPtr",  "OBJTYPE",
	"TYPEID",   "SetV4",    "SetV8",    "ADDSi",
	"CpyVtoV4", "CpyVtoV8", "CpyVtoR4", "CpyVtoR8",
	"CpyVtoG4", "CpyRtoV4", "CpyRtoV8", "CpyGtoV4",
	"WRTV1",    "WRTV2",    "WRTV4",    "WRTV8",
	"RDR1",     "RDR2",     "RDR4",     "RDR8",
	"LDG",      "LDV",      "PGA",      "CmpPtr",
	"VAR",      "iTOf",     "fTOi",     "uTOf",
	"fTOu",     "sbTOi",    "swTOi",    "ubTOi",
	"uwTOi",    "dTOi",     "dTOu",     "dTOf",
	"iTOd",     "uTOd",     "fTOd",     "ADDi",
	"SUBi",     "MULi",     "DIVi",     "MODi",
	"ADDf",     "SUBf",     "MULf",     "DIVf",
	"MODf",     "ADDd",     "SUBd",     "MULd",
	"DIVd",     "MODd",     "ADDIi",    "SUBIi",
	"MULIi",    "ADDIf",    "SUBIf",    "MULIf",
	"SetG4",    "ChkRefS",  "ChkNullV", "CALLINTF",
	"iTOb",     "iTOw",     "SetV1",    "SetV2",
	"Cast",     "i64TOi",   "uTOi64",   "iTOi64",
	"fTOi64",   "dTOi64",   "fTOu64",   "dTOu64",
	"i64TOf",   "u64TOf",   "i64TOd",   "u64TOd",
	"NEGi64",   "INCi64",   "DECi64",   "BNOT64",
	"ADDi64",   "SUBi64",   "MULi64",   "DIVi64",
	"MODi64",   "BAND64",   "BOR64",    "BXOR64",
	"BSLL64",   "BSRL64",   "BSRA64",   "CMPi64",
	"CMPu64",   "ChkNullS", "ClrHi",    "JitEntry",
	"CallPtr",  "FuncPtr",  "LoadThisR","PshV8",
	"DIVu",     "MODu",     "DIVu64",   "MODu64",
	"LoadRObjR","LoadVObjR","RefCpyV",  "JLowZ",
	"JLowNZ",   "AllocMem", "SetListSize","PshListElmnt",
	"SetListType","POWi",    "POWu",     "POWf",
	"POWd",     "POWdi",    "POWi64",   "POWu64",
	"Thiscall1",
};

constexpr size_t bc_name_count = sizeof(bc_names) / sizeof(bc_names[0]);

inline const char* bc_opcode_name(uint8_t op) {
	return op < bc_name_count ? bc_names[op] : "???";
}

inline int bc_instr_size(uint8_t op) {
	switch (op) {
	case 0x00: case 0x01: case 0x04: case 0x05: case 0x06: case 0x07:
	case 0x0A: case 0x3F: case 0x41:
		return 1;

	case 0x02: case 0x03: case 0x08: case 0x0B: case 0x0C: case 0x0D:
	case 0x0E: case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13:
	case 0x14: case 0x15: case 0x16: case 0x17:
	case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55:
	case 0x56: case 0x57: case 0x42: case 0x43: case 0x44:
	case 0x3C: case 0x3D: case 0x3E:
		return 5;

	case 0x09: case 0x40:
		return 5;

	case 0x5E: case 0x61: case 0x62: case 0x63:
	case 0x70:
		return 9;

	default:
		return 1;
	}
}

struct Function {
	std::string name;
	std::vector<uint8_t> bytecode;
};

struct StringRef {
	uint32_t offset;
	std::string value;
};

struct File {
	std::array<uint8_t, 8> header;
	std::vector<Function> functions;
	std::string source_path;
	std::vector<StringRef> strings;
};

auto parse(std::span<const uint8_t> data) -> File;

} // namespace asb
