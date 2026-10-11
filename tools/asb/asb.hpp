#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace asb2 {

struct TypeInfo;
using TypeInfoPtr = std::shared_ptr<TypeInfo>;

// Subset of AngelScript eTokenType, but we keep the full ordering so numeric values match.
enum eTokenType {
	ttUnrecognizedToken,
	ttEnd,
	ttWhiteSpace,
	ttOnelineComment,
	ttMultilineComment,
	ttIdentifier,
	ttIntConstant,
	ttFloatConstant,
	ttDoubleConstant,
	ttStringConstant,
	ttMultilineStringConstant,
	ttHeredocStringConstant,
	ttNonTerminatedStringConstant,
	ttBitsConstant,
	ttPlus,
	ttMinus,
	ttStar,
	ttSlash,
	ttPercent,
	ttStarStar,
	ttHandle,
	ttAddAssign,
	ttSubAssign,
	ttMulAssign,
	ttDivAssign,
	ttModAssign,
	ttPowAssign,
	ttOrAssign,
	ttAndAssign,
	ttXorAssign,
	ttShiftLeftAssign,
	ttShiftRightLAssign,
	ttShiftRightAAssign,
	ttInc,
	ttDec,
	ttDot,
	ttScope,
	ttAssignment,
	ttEndStatement,
	ttListSeparator,
	ttStartStatementBlock,
	ttEndStatementBlock,
	ttOpenParanthesis,
	ttCloseParanthesis,
	ttOpenBracket,
	ttCloseBracket,
	ttAmp,
	ttBitOr,
	ttBitNot,
	ttBitXor,
	ttBitShiftLeft,
	ttBitShiftRight,
	ttBitShiftRightArith,
	ttEqual,
	ttNotEqual,
	ttLessThan,
	ttGreaterThan,
	ttLessThanOrEqual,
	ttGreaterThanOrEqual,
	ttQuestion,
	ttColon,
	ttIf,
	ttElse,
	ttFor,
	ttWhile,
	ttBool,
	ttFuncDef,
	ttImport,
	ttInt,
	ttInt8,
	ttInt16,
	ttInt64,
	ttInterface,
	ttIs,
	ttNotIs,
	ttUInt,
	ttUInt8,
	ttUInt16,
	ttUInt64,
	ttFloat,
	ttVoid,
	ttTrue,
	ttFalse,
	ttReturn,
	ttNot,
	ttAnd,
	ttOr,
	ttXor,
	ttBreak,
	ttContinue,
	ttConst,
	ttDo,
	ttDouble,
	ttSwitch,
	ttCase,
	ttDefault,
	ttIn,
	ttOut,
	ttInOut,
	ttNull,
	ttClass,
	ttTypedef,
	ttEnum,
	ttCast,
	ttPrivate,
	ttProtected,
	ttNamespace,
	ttMixin,
	ttAuto,
	ttTry,
	ttCatch
};

std::string token_name(int token);

struct TypeInfo {
	enum Kind { Null, Object, Template, List, SubType, Child } kind = Null;
	std::string name;
	std::string name_space;
	std::vector<class DataType> sub_types;
	TypeInfoPtr list_of;
	TypeInfoPtr parent;
	uint32_t flags = 0;
	uint32_t size = 0;
	std::string format() const;
};

struct DataType {
	int token = ttVoid;
	TypeInfoPtr type_info;
	bool is_object_handle = false;
	bool handle_to_const = false;
	bool is_reference = false;
	bool is_read_only = false;
	std::string format() const;
};

struct Signature {
	std::string name;
	std::string name_space;
	DataType return_type;
	std::vector<DataType> params;
	std::vector<std::string> param_names;
	std::vector<int> inout_flags;
	std::vector<std::string> default_args;
	int func_type = 0;
	TypeInfoPtr object_type;
	bool is_readonly = false;
	bool is_private = false;
	bool is_protected = false;
	bool shared = false;
	bool external = false;
	bool is_explicit = false;
	bool dont_cleanup = false;
	std::string decl() const;
};

struct Instruction {
	uint32_t pos = 0;
	uint8_t op = 0;
	std::vector<int64_t> wargs;
	std::vector<uint64_t> dargs;
	uint64_t qarg = 0;
	bool has_qarg = false;
	std::string ann;
	int jump_target = -1; // 0-based instruction index
};

struct Variable {
	uint32_t declared_at = 0;
	int stack_offset = 0;
	std::string name;
	DataType type;
};

struct Function;
using FunctionPtr = std::shared_ptr<Function>;

struct Function {
	bool is_new = true;
	Signature sig;
	bool has_body = false;
	std::vector<Instruction> code;
	uint32_t variable_space = 0;
	std::vector<TypeInfoPtr> obj_var_types;
	std::vector<int> obj_var_pos;
	int obj_vars_on_heap = 0;
	std::vector<int> line_numbers;              // pairs (inst_pos, line)
	std::vector<std::pair<int, std::string>> sections; // (inst_pos, section name)
	std::vector<Variable> variables;
	std::string script_section;
	uint32_t declared_at = 0;
	uint32_t vf_table_idx = 0;
};

struct GlobalProp {
	std::string name;
	std::string name_space;
	DataType type;
	bool module_prop = false;
	FunctionPtr init; // module script global initializer function
};

struct ObjectProp {
	TypeInfoPtr obj;
	std::string name;
};

struct BindInfo {
	Signature sig;
	std::string import_from;
};

// --- Scene extraction (Tier 1+2) -------------------------------------------
struct SceneEvent {
	std::string call;
	std::string decl;
	char kind = '?';
	std::vector<std::string> strings;
	std::vector<int64_t> ints;
	std::vector<double> floats;
	std::vector<double> vec2;
	std::vector<double> vec3;
};

struct SceneFunction {
	std::string name;
	std::string decl;
	std::string section;
	std::vector<SceneEvent> events;
};

struct Scene {
	std::string file;
	std::vector<SceneFunction> functions;
};

struct Module {
	bool no_debug_info = false;
	std::vector<TypeInfoPtr> used_types;
	std::vector<DataType> used_type_ids;
	std::vector<FunctionPtr> used_functions;
	std::vector<char> used_function_kind; // 'm','a','s','n'
	std::vector<GlobalProp> used_globals;
	std::vector<GlobalProp> globals;
	std::vector<std::string> used_strings;
	std::vector<ObjectProp> used_obj_props;
	std::vector<FunctionPtr> functions;        // module functions (with bodies)
	std::vector<FunctionPtr> global_functions; // references into functions
	std::vector<FunctionPtr> funcdefs;
	std::vector<BindInfo> binds;
	std::vector<TypeInfoPtr> class_types;
	std::vector<TypeInfoPtr> enum_types;
	std::vector<TypeInfoPtr> typedefs;
	size_t consumed = 0;
	size_t total = 0;
	std::string error;

	const std::string& string_at(uint64_t encoded) const;
	std::string function_name(uint64_t idx) const;
	std::string function_kind(uint64_t idx) const;
	std::string type_name(uint64_t idx) const;
	std::string typeid_name(uint64_t idx) const;
	std::string objprop_name(uint64_t idx) const;
};

Module parse(std::span<const uint8_t> data);
std::string disassemble(const Module& mod, const Function& fn);
Scene extract_scene(const Module& mod);

} // namespace asb2
