#include "state.hpp"

#include <glad/glad.h>

// NOTE: Eltlinde needs override
// clang-format off
const std::unordered_map<std::string, State::MbsFtx> State::Chara = {
    {"Fighter", MbsFtx{"Fighter_M.mbs", "Fighter_M00.ftx", {0}}},
    {"Vanguard", MbsFtx{"Fighter_HG_M.mbs", "Fighter_HG_M00.ftx", {0}}},
    {"Soldier_F", MbsFtx{"Soldier_F.mbs", "Soldier_F00.ftx", {0}}},
    {"Sergeant_F", MbsFtx{"Soldier_HG_F.mbs", "Soldier_HG_F00.ftx", {0}}},
    {"Soldier_M", MbsFtx{"Soldier_M.mbs", "Soldier_M00.ftx", {0}}},
    {"Sergeant_M", MbsFtx{"Soldier_HG_M.mbs", "Soldier_HG_M00.ftx", {0}}},
    {"Housecarl", MbsFtx{"Huscarl_M.mbs", "Huscarl_M00.ftx", {0}}},
    {"Viking", MbsFtx{"Huscarl_HG_M.mbs", "Huscarl_HG_M00.ftx", {0}}},
    {"Swordfighter_F", MbsFtx{"Swordsman_F.mbs", "Swordsman_F00.ftx", {0}}},
    {"Swordmaster_F", MbsFtx{"Swordsman_HG_F.mbs", "Swordsman_HG_F00.ftx", {0}}},
    {"Swordfighter_M", MbsFtx{"Swordsman_M.mbs", "Swordsman_M00.ftx", {0}}},
    {"Swordmaster_M", MbsFtx{"Swordsman_HG_M.mbs", "Swordsman_HG_M00.ftx", {0}}},
    {"Sellsword_F", MbsFtx{"Mercenary_F.mbs", "Mercenary_F00.ftx", {0}}},
    {"Landsknecht_F", MbsFtx{"Mercenary_HG_F.mbs", "Mercenary_HG_F00.ftx", {0}}},
    {"Sellsword_M", MbsFtx{"Mercenary_M.mbs", "Mercenary_M00.ftx", {0}}},
    {"Landsknecht_M", MbsFtx{"Mercenary_HG_M.mbs", "Mercenary_HG_M00.ftx", {0}}},
    {"Hoplite", MbsFtx{"Hoplite_M.mbs", "Hoplite_M00.ftx", {0}}},
    {"Legionnaire", MbsFtx{"Hoplite_HG_M.mbs", "Hoplite_HG_M00.ftx", {0}}},
    {"Gladiator", MbsFtx{"Gladiator_HG_M.mbs", "Gladiator_HG_M00.ftx", {0}}},
    {"Berserker", MbsFtx{"Gladiator_M.mbs", "Gladiator_M00.ftx", {0}}},
    {"Warrior_F", MbsFtx{"Warrior_F.mbs", "Warrior_F00.ftx", {0}}},
    {"Breaker_F", MbsFtx{"Warrior_HG_F.mbs", "Warrior_HG_F00.ftx", {0}}},
    {"Warrior_M", MbsFtx{"Warrior_M.mbs", "Warrior_M00.ftx", {0}}},
    {"Breaker_M", MbsFtx{"Warrior_HG_M.mbs", "Warrior_HG_M00.ftx", {0}}},
    {"Hunter", MbsFtx{"Hunter_M.mbs", "Hunter_M00.ftx", {0}}},
    {"Sniper", MbsFtx{"Hunter_HG_M.mbs", "Hunter_HG_M00.ftx", {0}}},
    {"Shieldshooter", MbsFtx{"Ranger_F.mbs", "Ranger_F00.ftx", {0}}},
    {"Arbalist", MbsFtx{"Ranger_HG_F.mbs", "Ranger_HG_F00.ftx", {0}}},
    {"Thief", MbsFtx{"Thief_M.mbs", "Thief_M00.ftx", {0}}},
    {"Rogue", MbsFtx{"Thief_HG_M.mbs", "Thief_HG_M00.ftx", {0}}},
    {"Knight", MbsFtx{"Knight_M.mbs", "Knight_M00.ftx", {0}}},
    {"GreatKnight", MbsFtx{"Knight_HG_M.mbs", "Knight_HG_M00.ftx", {0}}},
    {"RadiantKnight", MbsFtx{"WhiteKnight_F.mbs", "WhiteKnight_F00.ftx", {0}}},
    {"SaintedKnight", MbsFtx{"WhiteKnight_HG_F.mbs", "WhiteKnight_HG_F00.ftx", {0}}},
    {"DarkKnight", MbsFtx{"BlackKnight_M.mbs", "BlackKnight_M00.ftx", {0}}},
    {"DoomKnight", MbsFtx{"BlackKnight_HG_M.mbs", "BlackKnight_HG_M00.ftx", {0}}},
    {"Cleric", MbsFtx{"Cleric_F.mbs", "Cleric_F00.ftx", {0}}},
    {"Bishop", MbsFtx{"Cleric_HG_F.mbs", "Cleric_HG_F00.ftx", {0}}},
    {"Wizard", MbsFtx{"Wizard_M.mbs", "Wizard_M00.ftx", {0}}},
    {"Warlock", MbsFtx{"Wizard_HG_M.mbs", "Wizard_HG_M00.ftx", {0}}},
    {"Witch", MbsFtx{"Witch_F.mbs", "Witch_F00.ftx", {0}}},
    {"Sorceress", MbsFtx{"Witch_HG_F.mbs", "Witch_HG_F00.ftx", {0}}},
    {"Shaman", MbsFtx{"Shaman_F.mbs", "Shaman_F00.ftx", {0}}},
    {"Druid", MbsFtx{"Shaman_HG_F.mbs", "Shaman_HG_F00.ftx", {0}}},
    {"WyvernKnight", MbsFtx{"WyvernKnight_F.mbs", "WyvernKnight_F00.ftx", {0}}},
    {"WyvernMaster", MbsFtx{"WyvernKnight_HG_F.mbs", "WyvernKnight_HG_F00.ftx", {0}}},
    {"GryphonKnight", MbsFtx{"GriffonKnight_F.mbs", "GriffonKnight_F00.ftx", {0}}},
    {"GryphonMaster", MbsFtx{"GriffonKnight_HG_F.mbs", "GriffonKnight_HG_F00.ftx", {0}}},
    {"Lord", MbsFtx{"Lord_M.mbs", "Lord_M00.ftx", {0}}},
    {"High LordMbsFtx", MbsFtx{"Lord_HG_M.mbs", "Lord_HG_M00.ftx", {0}}},
    {"Crusader", MbsFtx{"Virginia_F.mbs", "Virginia_F00.ftx", {0}}},
    {"Valkyria", MbsFtx{"Virginia_F.mbs", "Virginia_F00.ftx", {0}}},
    {"Paladin", MbsFtx{"WhiteKnight_HG_M.mbs", "WhiteKnight_HG_M00.ftx", {0}}},
    {"Prince", MbsFtx{"Prince_M.mbs", "Prince_M00.ftx", {0}}},
    {"Dreadnought", MbsFtx{"Armoria_HG_F.mbs", "Armoria_HG_F00.ftx", {0}}},
    {"ElvenFencer_F", MbsFtx{"ElfFencer_F.mbs", "ElfFencer_F00.ftx", {0}}},
    {"ElvenFencer_M", MbsFtx{"ElfFencer_M.mbs", "ElfFencer_M00.ftx", {0}}},
    {"ElvenArcher_F", MbsFtx{"ElfArcher_F.mbs", "ElfArcher_F00.ftx", {0}}},
    {"ElvenArcher_M", MbsFtx{"ElfArcher_M.mbs", "ElfArcher_M00.ftx", {0}}},
    {"ElvenAugur", MbsFtx{"Eltlinde_F00.mbs", "Eltlinde_F00.ftx", {0}}},
    {"ElvenSibyl", MbsFtx{"Eltlinde_F10.mbs", "Eltlinde_F10.ftx", {0}}},
    {"Werewolf", MbsFtx{"WereWolf_M.mbs", "WereWolf_M00.ftx", {0}}},
    {"Werebear", MbsFtx{"WereBear_M.mbs", "WereBear_M00.ftx", {0}}},
    {"Werefox", MbsFtx{"WereFox_F.mbs", "WereFox_F00.ftx", {0}}},
    {"Wereowl", MbsFtx{"WereOwl_F.mbs", "WereOwl_F00.ftx", {0}}},
    {"Werelion", MbsFtx{"Gladiator_HG_M.mbs", "Gladiator_HG_M00.ftx", {0}}},
    {"SnowRanger", MbsFtx{"Unify_M.mbs", "Unify_M00.ftx", {0}}},
    {"Feathersword", MbsFtx{"FeatherSword_F.mbs", "FeatherSword_F00.ftx", {0}}},
    {"Featherbow", MbsFtx{"FeatherBow_F.mbs", "FeatherBow_F00.ftx", {0}}},
    {"Featherstaff", MbsFtx{"FeatherRod_M.mbs", "FeatherRod_M00.ftx", {0}}},
    {"Feathershield", MbsFtx{"FeatherShield_M.mbs", "FeatherShield_M00.ftx", {0}}},
    {"Priestess", MbsFtx{"Scarlet_F.mbs", "Scarlet_F00.ftx", {0x4000 + 0x1000}}},
    {"HighPriestess", MbsFtx{"Scarlet_F.mbs", "Scarlet_F00.ftx", {0x2000 + 0x800}}},
    {"DarkLord", MbsFtx{"BlackPrince_HG_M.mbs", "BlackPrince_HG_M00.ftx", {0}}},
    {"Overlord", MbsFtx{"BlackPrince_M.mbs", "BlackPrince_M00.ftx", {0}}},
    {"Necromancer", MbsFtx{"Necromancer_M.mbs", "Necromancer_M00.ftx", {0}}},
    {"DarkMarquess_Sword", MbsFtx{"DarkMarquess_S_M.mbs", "DarkMarquess_S_M00.ftx", {0}}},
    {"DarkMarquess_Axe", MbsFtx{"DarkMarquess_A_F.mbs", "DarkMarquess_A_F00.ftx", {0}}},
    {"DarkMarquess_Spear", MbsFtx{"DarkMarquess_L_M.mbs", "DarkMarquess_L_M00.ftx", {0}}},
    {"DarkMarquess_Staff", MbsFtx{"DarkMarquess_R_F.mbs", "DarkMarquess_R_F00.ftx", {0}}},
};
// clang-format on

State::State() {
	glGenFramebuffers(1, &_tgt_fb);
	glBindFramebuffer(GL_FRAMEBUFFER, _tgt_fb);
}

State::~State() noexcept { glDeleteFramebuffers(1, &_tgt_fb); }

void State::render() {
	GLuint renderedTexture;
	glGenTextures(1, &renderedTexture);
	glBindTexture(GL_TEXTURE_2D, renderedTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTexture, 0);
    GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers);
}

namespace gl {

void depth(int d) {
	if (d == 0) {
		glClear(GL_DEPTH_BUFFER_BIT);
		glClearDepthf(0.0f);
		return glDisable(GL_DEPTH_TEST);
	}
	glDepthFunc(d);
	return glEnable(GL_DEPTH_TEST);
}

} // namespace gl