#include "state.hpp"

#include <glad/glad.h>
#include <spanstream>

using Job = State::Job;

// clang-format off
static const std::unordered_map<std::string, Job> Characters = {
	{"Fighter", Job{{"Chara/Fighter_M.mbs"}, {"Chara/Fighter_M00.ftx"},                                { {"Lex", 0}, {"Colm", 0}, {"Generic", 0} }}},
	{"Vanguard", Job{{"Chara/Fighter_HG_M.mbs"}, {"Chara/Fighter_HG_M00.ftx"},                         { {"Lex", 0}, {"Colm", 0}, {"Generic", 0} }}},
	{"Soldier_F",  Job{{"Chara/Soldier_F.mbs"}, {"Chara/Soldier_F00.ftx"},                             { {"Chloe", 0}, {"Generic", 0} }}},
	{"Sergeant_F", Job{{"Chara/Soldier_HG_F.mbs"}, {"Chara/Soldier_HG_F00.ftx"},                       { {"Chloe", 0}, {"Generic", 0} }}},
	{"Soldier_M",  Job{{"Chara/Soldier_M.mbs"}, {"Chara/Soldier_M00.ftx"},                             { {"Gailey", 0}, {"Generic", 0} }}},
	{"Sergeant_M", Job{{"Chara/Soldier_HG_M.mbs"}, {"Chara/Soldier_HG_M00.ftx"},                       { {"Gailey", 0}, {"Generic", 0} }}},
	{"Housecarl", Job{{"Chara/Huscarl_M.mbs"}, {"Chara/Huscarl_M00.ftx"},                              { {"Aubin", 0}, {"Generic", 0} }}},
	{"Viking", Job{{"Chara/Huscarl_HG_M.mbs"}, {"Chara/Huscarl_HG_M00.ftx"},                           { {"Aubin", 0}, {"Generic", 0} }}},
	{"Swordfighter_F", Job{{"Chara/Swordsman_F.mbs"}, {"Chara/Swordsman_F00.ftx"},                     { {"Melisandre", 0x2 + 0x2000}, {"Leah", 0x1}, {"Generic", 0x1 + 0x2000} }}},
	{"Swordmaster_F",  Job{{"Chara/Swordsman_HG_F.mbs"}, {"Chara/Swordsman_HG_F00.ftx"},               { {"Melisandre", 0x2 + 0x2000}, {"Leah", 0x1}, {"Generic", 0x1 + 0x2000} }}},
	{"Swordfighter_M", Job{{"Chara/Swordsman_M.mbs"}, {"Chara/Swordsman_M00.ftx"},                     { {"Aramis", 0}, {"Generic", 0} }}},
	{"Swordmaster_M",  Job{{"Chara/Swordsman_HG_M.mbs"}, {"Chara/Swordsman_HG_M00.ftx"},               { {"Aramis", 0}, {"Generic", 0} }}},
	{"Sellsword_F",   Job{{"Chara/Mercenary_F.mbs"}, {"Chara/Mercenary_F00.ftx"},                      { {"Berenice", 0}, {"Berengaria", 0}, {"Generic", 0} }}},
	{"Landsknecht_F", Job{{"Chara/Mercenary_HG_F.mbs"}, {"Chara/Mercenary_HG_F00.ftx"},                { {"Berenice", 0}, {"Berengaria", 0}, {"Generic", 0} }}},
	{"Sellsword_M",   Job{{"Chara/Mercenary_M.mbs"}, {"Chara/Mercenary_M00.ftx"},                      { {"Magellan", 0}, {"Jeremy", 0}, {"Generic", 0} }}},
	{"Landsknecht_M", Job{{"Chara/Mercenary_HG_M.mbs"}, {"Chara/Mercenary_HG_M00.ftx"},                { {"Magellan", 0}, {"Jeremy", 0}, {"Generic", 0} }}},
	{"Hoplite", Job{{"Chara/Hoplite_M.mbs"}, {"Chara/Hoplite_M00.ftx"},                                { {"Hodrick", 0}, {"Bryce", 0}, {"Beaumont", 0}, {"Generic", 0} }}},
	{"Legionnaire", Job{{"Chara/Hoplite_HG_M.mbs"}, {"Chara/Hoplite_HG_M00.ftx"},                      { {"Hodrick", 0}, {"Bryce", 0}, {"Beaumont", 0}, {"Generic", 0} }}},
	{"Gladiator", Job{{"Chara/Gladiator_HG_M.mbs"}, {"Chara/Gladiator_HG_M00.ftx"},                    { {"Bruno", 0}, {"Generic", 0} }}},
	{"Berserker", Job{{"Chara/Gladiator_M.mbs"}, {"Chara/Gladiator_M00.ftx"},                          { {"Bruno", 0}, {"Generic", 0} }}},
	{"Warrior_F", Job{{"Chara/Warrior_F.mbs"}, {"Chara/Warrior_F00.ftx"},                              { {"Mordon", 0}, {"Generic", 0} }}},
	{"Breaker_F", Job{{"Chara/Warrior_HG_F.mbs"}, {"Chara/Warrior_HG_F00.ftx"},                        { {"Mordon", 0}, {"Generic", 0} }}},
	{"Warrior_M", Job{{"Chara/Warrior_M.mbs"}, {"Chara/Warrior_M00.ftx"},                              { {"Nina", 0}, {"Mille", 0}, {"Kitra", 0}, {"Generic", 0} }}},
	{"Breaker_M", Job{{"Chara/Warrior_HG_M.mbs"}, {"Chara/Warrior_HG_M00.ftx"},                        { {"Nina", 0}, {"Mille", 0}, {"Kitra", 0}, {"Generic", 0} }}},
	{"Hunter", Job{{"Chara/Hunter_M.mbs"}, {"Chara/Hunter_M00.ftx"},                                   { {"Rolf", 0}, {"Mandrin", 0}, {"Generic", 0} }}},
	{"Sniper", Job{{"Chara/Hunter_HG_M.mbs"}, {"Chara/Hunter_HG_M00.ftx"},                             { {"Rolf", 0}, {"Mandrin", 0}, {"Generic", 0} }}},
	{"Arbalist", Job{{"Chara/Ranger_F.mbs"}, {"Chara/Ranger_F00.ftx"},                                 { {"Liza", 0}, {"Generic", 0} }}},
	{"Shieldshooter", Job{{"Chara/Ranger_HG_F.mbs"}, {"Chara/Ranger_HG_F00.ftx"},                      { {"Liza", 0}, {"Generic", 0} }}},
	{"Thief", Job{{"Chara/Thief_M.mbs"}, {"Chara/Thief_M00.ftx"},                                      { {"Travis", 0}, {"Gammel", 0}, {"Generic", 0} }}},
	{"Rogue", Job{{"Chara/Thief_HG_M.mbs"}, {"Chara/Thief_HG_M00.ftx"},                                { {"Travis", 0}, {"Gammel", 0}, {"Generic", 0} }}},
	{"Knight", Job{{"Chara/Knight_M.mbs"}, {"Chara/Knight_M00.ftx"},                                   { {"Clive", 0}, {"Adel", 0}, {"Renault", 0}, {"Jerome", 0}, {"Generic", 0} }}},
	{"GreatKnight", Job{{"Chara/Knight_HG_M.mbs"}, {"Chara/Knight_HG_M00.ftx"},                        { {"Clive", 0}, {"Adel", 0}, {"Renault", 0}, {"Jerome", 0}, {"Generic", 0} }}},
	{"RadiantKnight", Job{{"Chara/WhiteKnight_F.mbs"}, {"Chara/WhiteKnight_F00.ftx"},                  { {"Monica", 0}, {"Miriam", 0}, {"Generic", 0} }}},
	{"SaintedKnight", Job{{"Chara/WhiteKnight_HG_F.mbs"}, {"Chara/WhiteKnight_HG_F00.ftx"},            { {"Monica", 0}, {"Miriam", 0}, {"Generic", 0} }}},
	{"DarkKnight", Job{{"Chara/BlackKnight_M.mbs"}, {"Chara/BlackKnight_M00.ftx"},                     { {"Gloucester", 0}, {"Generic", 0} }}},
	{"DoomKnight", Job{{"Chara/BlackKnight_HG_M.mbs"}, {"Chara/BlackKnight_HG_M00.ftx"},               { {"Gloucester", 0}, {"Generic", 0} }}},
	{"Cleric", Job{{"Chara/Cleric_F.mbs"}, {"Chara/Cleric_F00.ftx"},                                   { {"Sharon", 0x4 + 0x10000}, {"Tatiana", 0x8 + 0x10 + 0x10000}, {"Primm", 0x2 + 0x10000}, {"Generic", 0x1 + 0x10000}, {"Generic2", 0x20 + 0x40 + 0x10000} }}},
	{"Bishop", Job{{"Chara/Cleric_HG_F.mbs"}, {"Chara/Cleric_HG_F00.ftx"},                             { {"Sharon", 0x4 + 0x10000}, {"Tatiana", 0x8 + 0x10 + 0x10000}, {"Primm", 0x2 + 0x10000}, {"Generic", 0x1 + 0x10000}, {"Generic2", 0x20 + 0x40 + 0x10000} }}},
	{"Wizard", Job{{"Chara/Wizard_M.mbs"}, {"Chara/Wizard_M00.ftx"},                                   { {"Auch", 0}, {"Generic", 0} }}},
	{"Warlock", Job{{"Chara/Wizard_HG_M.mbs"}, {"Chara/Wizard_HG_M00.ftx"},                            { {"Auch", 0}, {"Generic", 0} }}},
	{"Witch", Job{{"Chara/Witch_F.mbs"}, {"Chara/Witch_F00.ftx"},                                      { {"Yahna", 0x2 + 0x8000}, {"Alcina", 0x4}, {"Generic", 0x1 + 0x8000} }}},
	{"Sorceress", Job{{"Chara/Witch_HG_F.mbs"}, {"Chara/Witch_HG_F00.ftx"},                            { {"Yahna", 0x2 + 0x8000}, {"Alcina", 0x4}, {"Generic", 0x1 + 0x8000} }}},
	{"Shaman", Job{{"Chara/Shaman_F.mbs"}, {"Chara/Shaman_F00.ftx"},                                   { {"Selvie", 0}, {"Generic", 0} }}},
	{"Druid", Job{{"Chara/Shaman_HG_F.mbs"}, {"Chara/Shaman_HG_F00.ftx"},                              { {"Selvie", 0}, {"Generic", 0} }}},
	{"WyvernKnight",  Job{{"Chara/WyvernKnight_F.mbs"}, {"Chara/WyvernKnight_F00.ftx"},                { {"Hilda", 0x2 + 0x10000}, {"Generic", 0x1} }}},
	{"WyvernMaster",  Job{{"Chara/WyvernKnight_HG_F.mbs"}, {"Chara/WyvernKnight_HG_F00.ftx"},          { {"Hilda", 0x2 + 0x10000}, {"Generic", 0x1} }}},
	{"GryphonKnight", Job{{"Chara/GriffonKnight_F.mbs"}, {"Chara/GriffonKnight_F00.ftx"},              { {"Fran", 0}, {"Celeste", 0}, {"Generic", 0} }}},
	{"GryphonMaster", Job{{"Chara/GriffonKnight_HG_F.mbs"}, {"Chara/GriffonKnight_HG_F00.ftx"},        { {"Fran", 0}, {"Celeste", 0}, {"Generic", 0} }}},
	{"Lord", Job{{"Chara/Lord_M.mbs"}, {"Chara/Lord_M00.ftx"},                                         { {"Alain", 0} }}},
	{"HighLord", Job{{"Chara/Lord_HG_M.mbs"}, {"Chara/Lord_HG_M00.ftx"},                               { {"Alain", 0} }}},
	{"Crusader", Job{{"Chara/Virginia_F.mbs"}, {"Chara/Virginia_F00.ftx"},                             { {"Virginia", 0x1 + 0x2000 + 0x1000 + 0x10000}, {"Ilenia", 0x2 + 0x2000 + 0x1000 + 0x10000 + 0x20000} }}},
	{"Valkyria", Job{{"Chara/Virginia_F.mbs"}, {"Chara/Virginia_F00.ftx"},                             { {"Virginia", 0x1 + 0x4000 + 0x1000 + 0x10000}, {"Ilenia", 0x2 + 0x4000 + 0x1000 + 0x10000 + 0x20000} }}},
	{"Paladin", Job{{"Chara/WhiteKnight_HG_M.mbs"}, {"Chara/WhiteKnight_HG_M00.ftx"},                  { {"Josef", 0} }}},
	{"Prince", Job{{"Chara/Prince_M.mbs"}, {"Chara/Prince_M00.ftx"},                                   { {"Gilbert", 0} }}},
	{"Dreadnought", Job{{"Chara/Armoria_HG_F.mbs"}, {"Chara/Armoria_HG_F00.ftx"},                      { {"Amalia", 0} }}},
	{"ElvenFencer_F", Job{{"Chara/ElfFencer_F.mbs"}, {"Chara/ElfFencer_F00.ftx"},                      { {"Railanor", 0}, {"Generic", 0} }}},
	{"ElvenFencer_M", Job{{"Chara/ElfFencer_M.mbs"}, {"Chara/ElfFencer_M00.ftx"},                      { {"Ithilion", 0}, {"Generic", 0} }}},
	{"ElvenArcher_F", Job{{"Chara/ElfArcher_F.mbs"}, {"Chara/ElfArcher_F00.ftx"},                      { {"Ridiel", 0}, {"Galadmir", 0}, {"Generic", 0} }}},
	{"ElvenArcher_M", Job{{"Chara/ElfArcher_M.mbs"}, {"Chara/ElfArcher_M00.ftx"},                      { {"Lhinalagos", 0}, {"Generic", 0} }}},
	{"ElvenSibyl", Job{{"Chara/Eltlinde_F.mbs", "Chara/Eltlinde_F00.mbs"}, {"Chara/Eltlinde_F00.ftx"}, { {"Eltolinde", 0x2 + 0x4000 + 0x8000 + 0x10000} } }},
	{"ElvenAugur", Job{{"Chara/Eltlinde_F.mbs", "Chara/Eltlinde_F10.mbs"}, {"Chara/Eltlinde_F10.ftx"}, { {"Rosalinde", 0x1 + 0x4000 +          0x20000} } }},
	{"Werewolf", Job{{"Chara/WereWolf_M.mbs"}, {"Chara/WereWolf_M00.ftx"},                             { {"Govil", 0}, {"Generic", 0} }}},
	{"Werebear", Job{{"Chara/WereBear_M.mbs"}, {"Chara/WereBear_M00.ftx"},                             { {"Dinah", 0}, {"Generic", 0} }}},
	{"Werefox",  Job{{"Chara/WereFox_F.mbs"}, {"Chara/WereFox_F00.ftx"},                               { {"Bertrans", 0}, {"Generic", 0} }}},
	{"Wereowl",  Job{{"Chara/WereOwl_F.mbs"}, {"Chara/WereOwl_F00.ftx"},                               { {"Ramona", 0}, {"Generic", 0} }}},
	{"Werelion", Job{{"Chara/Gladiator_HG_M.mbs"}, {"Chara/Gladiator_HG_M00.ftx"},                     { {"Morard", 0} }}},
	{"SnowRanger", Job{{"Chara/Unify_M.mbs"}, {"Chara/Unify_M00.ftx"},                                 { {"Yunifi", 0} }}},
	{"Feathersword",  Job{{"Chara/FeatherSword_F.mbs"}, {"Chara/FeatherSword_F00.ftx"},                { {"Ochlys", 0x2 + 0x4}, {"Umerus", 0x8}, {"Generic", 0x1 + 0x4} }}},
	{"Featherbow",    Job{{"Chara/FeatherBow_F.mbs"}, {"Chara/FeatherBow_F00.ftx"},                    { {"Raenys", 0x2 + 0x10000}, {"Generic", 0x1} }}},
	{"Featherstaff",  Job{{"Chara/FeatherRod_M.mbs"}, {"Chara/FeatherRod_M00.ftx"},                    { {"Sanatio", 0}, {"Generic", 0} }}},
	{"Feathershield", Job{{"Chara/FeatherShield_M.mbs"}, {"Chara/FeatherShield_M00.ftx"},              { {"Fodoquia", 0}, {"Generic", 0} }}},
	{"Priestess",     Job{{"Chara/Scarlet_F.mbs"}, {"Chara/Scarlet_F00.ftx"},                          { {"Scarlett", 0x800  + 0x2000 + 0x54010000} }}},
	{"HighPriestess", Job{{"Chara/Scarlet_F.mbs"}, {"Chara/Scarlet_F00.ftx"},                          { {"Scarlett", 0x1000 + 0x4000 + 0x54010000} }}},
	{"DarkLord", Job{{"Chara/BlackPrince_HG_M.mbs"}, {"Chara/BlackPrince_HG_M00.ftx"},                 { {"Galerius", 0} }}},
	{"Overlord", Job{{"Chara/BlackPrince_M.mbs"}, {"Chara/BlackPrince_M00.ftx"},                       { {"Galerius", 0} }}},
	{"Necromancer", Job{{"Chara/Necromancer_M.mbs"}, {"Chara/Necromancer_M00.ftx"},                    { {"Baltro", 0} }}},
	{"DarkMarquess_Sword", Job{{"Chara/DarkMarquess_S_M.mbs"}, {"Chara/DarkMarquess_S_M00.ftx"},       { {"Nigel", 0x1}, {"Holonius", 0x2} }}},
	{"DarkMarquess_Axe",   Job{{"Chara/DarkMarquess_A_F.mbs"}, {"Chara/DarkMarquess_A_F00.ftx"},       { {"Berengaria", 0x1 + 0x60030000}, {"Theodora", 0x2} }}},
	{"DarkMarquess_Lance", Job{{"Chara/DarkMarquess_L_M.mbs"}, {"Chara/DarkMarquess_L_M00.ftx"},       { {"Elgor", 0x1}, {"Belisarios", 0x2} }}},
	{"DarkMarquess_Rod",   Job{{"Chara/DarkMarquess_R_F.mbs"}, {"Chara/DarkMarquess_R_F00.ftx"},       { {"Alcina", 0x1 + 0x10000}, {"Narcesse", 0x2} }}},
};
// clang-format on

static GLuint make_texture_array(std::vector<FTX::Entry> textures) {
	float max_x = 0, max_y = 0;
	for (auto& t : textures) {
		FTX::decompress(t);
		FTX::deswizzle(t);
		max_x = std::max(max_x, float(t.width));
		max_y = std::max(max_y, float(t.height));
		std::cout << t.name << '\t' << t.width << 'x' << t.height << '\n';
	}
	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D_ARRAY, id);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_ALWAYS);

	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, max_x, max_y, textures.size());
	for (int i = 0; i < textures.size(); ++i) {
		const auto& t = textures[i];
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, t.width, t.height, 1, GL_RGBA, GL_UNSIGNED_BYTE, t.rgba.data());
	}
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	return id;
}

State::State(const std::string& path)
	: _cpk(path) {
	//glGenFramebuffers(1, &_tgt_fb);
	//glBindFramebuffer(GL_FRAMEBUFFER, _tgt_fb);
}

State::~State() noexcept { /*glDeleteFramebuffers(1, &_tgt_fb);*/ }

State::Sprite State::FetchSprite(const std::string& classname, const std::string& charaname) {
	const auto iter = Characters.find(classname);
	if (iter == Characters.end()) {
		throw std::runtime_error("MBS for character class " + classname + " was not found.");
	}

	if (auto entry = _cpk.by_name(iter->second.mbs[0]); entry == _cpk.end()) {
		throw std::runtime_error("MBS for character class " + classname + " was not found.");
	} else {
		_cpk.extract(*entry, _buffer);
	}

	Sprite sprite { MBS::From(_buffer) };
	sprite.flags = iter->second.variants.at(charaname);
	for (const auto& ftx : iter->second.ftx) {
		auto entry = _cpk.by_name(ftx);
		if (entry == _cpk.end()) {
			continue;
		}
		_cpk.extract(*entry, _buffer);
		auto txt = FTX::parse(_buffer);
		std::move(txt.begin(), txt.end(), std::back_inserter(sprite.textures));
	}
	sprite.glTexHandle = make_texture_array(sprite.textures);
	glBindTexture(GL_TEXTURE_2D_ARRAY, sprite.glTexHandle);
	glActiveTexture(GL_TEXTURE0);
	return sprite;
}
