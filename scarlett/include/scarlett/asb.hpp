#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace asb {

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
	// AngelScript declaration syntax (const -> type -> @ / @const -> &).
	std::string as_decl() const;
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
	// Like decl() but without the object-type prefix, in the syntax
	// RegisterGlobalFunction/RegisterObjectMethod expect.
	std::string bare_decl(bool include_namespace = true) const;
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
	std::vector<int> line_numbers;					   // pairs (inst_pos, line)
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

// Scene IR: the result of folding a function's event stream into entities.
struct SceneEntity {
	std::string id;
	std::string kind; // originating create call
	std::string mbs;  // "Dir/File.mbs"
	std::string ftx;  // "Dir/File.ftx"
	std::vector<std::string> aliases;
	float pos[3] = {0.0f, 0.0f, 0.0f};
	float scale = 1.0f;
	std::string track; // animation name, if known
	int track_index = -1;
	int layer = -1;
	bool visible = true;
	int order = 0;
};

struct SceneCamera {
	float pos[3] = {0.0f, 0.0f, 0.0f};
	float zoom = 0.0f;
	bool has_zoom = false;
};

struct SceneIR {
	std::string source;
	std::string function;
	std::vector<SceneEntity> entities;
	std::vector<SceneCamera> camera;
	std::vector<int64_t> stage;
};

// Character name -> asset resolution (generated, not hand-maintained).
struct CharaInfo {
	std::string disp_name;
	std::string mbs;
	std::string ftx;
};
using CharTable = std::unordered_map<std::string, CharaInfo>;

CharTable load_char_table(const std::string& path);

// The engine's CharaSet appearance table (exe `.data` 0x2741bb8, stride 0x48)
// joined with the class -> asset table. World characters (`DrCreateChara` /
// `DrCreateNpc`) carry no asset in the call: the engine resolves the chara def
// name through `FUN_0011a100`, which strcmp's the record's Japanese label
// (record word 0), so the label is the primary key. The ASCII CharaSet `name`
// (e.g. `PL_LORD_C0`) is indexed too. First insertion wins (as the engine's
// linear scan does).
struct CharaSetTable {
	std::unordered_map<std::string, CharaInfo> by_key;
	std::vector<std::string> names; // charaset.tsv idx -> ASCII CharaSet name
	const CharaInfo* resolve(const std::string& key) const {
		auto it = by_key.find(key);
		return it == by_key.end() ? nullptr : &it->second;
	}
	std::string name_at(int idx) const {
		return idx >= 0 && static_cast<size_t>(idx) < names.size() ? names[idx] : std::string();
	}
};

// charaset.tsv: `idx name class gender variant charaImg label`
// classes.tsv:  `key jp_name mbs ftx` (mbs/ftx are `Chara/`-relative)
// charanpc.tsv: `idx name jp mbs ftx` (from the engine NPCTYPE table; optional)
CharaSetTable
load_chara_set(const std::string& charaset_tsv, const std::string& classes_tsv, const std::string& charanpc_tsv = {});

// Facility id/enum -> localized name (uc_enums.tsv rows with kind ST_FACILITYID,
// generated from Debug/_UcEnum_Facility.inc). `GetStationFacilityName` uses it.
struct FacilityNames {
	std::vector<std::string> by_index;
	std::unordered_map<std::string, std::string> by_name;
	std::string resolve(const std::string& key) const;
};
FacilityNames load_facility_names(const std::string& uc_enums_tsv);

// Stage base levels/cue ids and facility positions from the engine `.data`
// (tools/dump_stage_tables.py). Stage keys are `ST_OW_*` (case-insensitive);
// facility keys are the Japanese labels the scripts pass, plus the ASCII
// `ST_FACILITYID` names.
struct StageTable {
	struct Stage {
		int base_lv = 0;
		int cue = -1;
	};
	struct Facility {
		std::string label;
		float pos[2] = {0.0f, 0.0f};
	};
	struct CountryRegion {
		int id = 0;
		float p[8] = {0.0f}; // x0,y0, x1,y1, x2,y2, x3,y3
	};
	std::unordered_map<std::string, Stage> stages;
	std::unordered_map<std::string, Facility> facilities;
	std::vector<CountryRegion> countries;
	const Stage* stage(const std::string& name) const;
	const Facility* facility(const std::string& name) const;
	// Engine country lookup: point-in-quad over the regions, then the engine's
	// 5-entry id map (3,4,1,0,0; no region -> 2).
	int country_at(const float p[2]) const;
};
StageTable load_stage_tables(const std::string& stages_tsv, const std::string& facility_tsv,
	const std::string& country_tsv = {});

// Drop tables from the engine `.data` joined with the DROPTABLE/ITEMID enums.
struct DropTableEntry {
	std::string item;
	int weight = 0;
	int count = 0;
	int add_index = 0; // 1-based index into CreateDbItem's _aAddNum (0 = none)
};
struct DropTables {
	std::vector<std::string> names;		// drop table id -> enum name
	std::vector<std::string> item_names;	// item id -> enum name
	std::unordered_map<std::string, int> item_ids;
	std::vector<std::vector<DropTableEntry>> by_id;
	const std::vector<DropTableEntry>* resolve(int id) const;
	std::string name_at(int id) const;
	std::string item_name(int id) const;
};
DropTables load_drop_tables(const std::string& drop_tables_tsv, const std::string& uc_enums_tsv);

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
	std::vector<FunctionPtr> functions;		   // module functions (with bodies)
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
SceneIR build_scene_ir(const SceneFunction& fn, const std::string& source, const CharTable* chars = nullptr, const CharaSetTable* sets = nullptr);

// Choose the scene entry function: the named function when given, else
// scene00/MAIN, else the first scene* that creates entities, else the function
// that creates the most, else the first non-empty one. nullptr if none.
const SceneFunction* pick_scene_function(const Scene& scene, const std::string& wanted = {});

// Call-name classification, shared by the scene fold, the harvest tool and the viewer.
bool is_sprite_create_call(std::string_view call);
bool is_chara_create_call(std::string_view call);
bool is_entity_create_call(std::string_view call);

} // namespace asb
