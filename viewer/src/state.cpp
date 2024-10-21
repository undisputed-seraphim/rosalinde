#include "state.hpp"

#include <glad/glad.h>

// clang-format off
// NOTE: Eltlinde needs override
const std::unordered_map<std::string, State::MbsFtx> State::Characters = {
    {"Armoria_HG_F", MbsFtx{"Chara/Armoria_HG_F.mbs", "Chara/Armoria_HG_F00.ftx", "CharaFace/Face_Armoria_HG_F.mbs", "CharaFace/Face_Armoria_HG_F00.ftx", "CharaOp/Op_Armoria_HG_F.mbs", "CharaOp/Op_Armoria_HG_F00.ftx"}},
    {"BlackKnight_HG_M", MbsFtx{"Chara/BlackKnight_HG_M.mbs", "Chara/BlackKnight_HG_M00.ftx", "CharaFace/Face_BlackKnight_HG_M.mbs", "CharaFace/Face_BlackKnight_HG_M00.ftx", "CharaOp/Op_BlackKnight_HG_M.mbs", "CharaOp/Op_BlackKnight_HG_M00.ftx"}},
    {"BlackKBlackKnight_Mnight", MbsFtx{"Chara/BlackKBlackKnight_Mnight.mbs", "Chara/BlackKBlackKnight_Mnight00.ftx", "CharaFace/Face_BlackKBlackKnight_Mnight.mbs", "CharaFace/Face_BlackKBlackKnight_Mnight00.ftx", "CharaOp/Op_BlackKBlackKnight_Mnight.mbs", "CharaOp/Op_BlackKBlackKnight_Mnight00.ftx"}},
    {"BlackPrince_HG_M", MbsFtx{"Chara/BlackPrince_HG_M.mbs", "Chara/BlackPrince_HG_M00.ftx", "CharaFace/Face_BlackPrince_HG_M.mbs", "CharaFace/Face_BlackPrince_HG_M00.ftx", "CharaOp/Op_BlackPrince_HG_M.mbs", "CharaOp/Op_BlackPrince_HG_M00.ftx"}},
    {"BlackPrince_M", MbsFtx{"Chara/BlackPrince_M.mbs", "Chara/BlackPrince_M00.ftx", "CharaFace/Face_BlackPrince_M.mbs", "CharaFace/Face_BlackPrince_M00.ftx", "CharaOp/Op_BlackPrince_M.mbs", "CharaOp/Op_BlackPrince_M00.ftx"}},
    {"Cleric_F", MbsFtx{"Chara/Cleric_F.mbs", "Chara/Cleric_F00.ftx", "CharaFace/Face_Cleric_F.mbs", "CharaFace/Face_Cleric_F00.ftx", "CharaOp/Op_Cleric_F.mbs", "CharaOp/Op_Cleric_F00.ftx"}},
    {"Cleric_HG_F", MbsFtx{"Chara/Cleric_HG_F.mbs", "Chara/Cleric_HG_F00.ftx", "CharaFace/Face_Cleric_HG_F.mbs", "CharaFace/Face_Cleric_HG_F00.ftx", "CharaOp/Op_Cleric_HG_F.mbs", "CharaOp/Op_Cleric_HG_F00.ftx"}},
    {"DarkMarquess_A_F", MbsFtx{"Chara/DarkMarquess_A_F.mbs", "Chara/DarkMarquess_A_F00.ftx", "CharaFace/Face_DarkMarquess_A_F.mbs", "CharaFace/Face_DarkMarquess_A_F00.ftx", "CharaOp/Op_DarkMarquess_A_F.mbs", "CharaOp/Op_DarkMarquess_A_F00.ftx"}},
    {"DarkMarquess_L_M", MbsFtx{"Chara/DarkMarquess_L_M.mbs", "Chara/DarkMarquess_L_M00.ftx", "CharaFace/Face_DarkMarquess_L_M.mbs", "CharaFace/Face_DarkMarquess_L_M00.ftx", "CharaOp/Op_DarkMarquess_L_M.mbs", "CharaOp/Op_DarkMarquess_L_M00.ftx"}},
    {"DarkMarquess_R_F", MbsFtx{"Chara/DarkMarquess_R_F.mbs", "Chara/DarkMarquess_R_F00.ftx", "CharaFace/Face_DarkMarquess_R_F.mbs", "CharaFace/Face_DarkMarquess_R_F00.ftx", "CharaOp/Op_DarkMarquess_R_F.mbs", "CharaOp/Op_DarkMarquess_R_F00.ftx"}},
    {"DarkMarquess_S_M", MbsFtx{"Chara/DarkMarquess_S_M.mbs", "Chara/DarkMarquess_S_M00.ftx", "CharaFace/Face_DarkMarquess_S_M.mbs", "CharaFace/Face_DarkMarquess_S_M00.ftx", "CharaOp/Op_DarkMarquess_S_M.mbs", "CharaOp/Op_DarkMarquess_S_M00.ftx"}},
    {"ElfArcher_F", MbsFtx{"Chara/ElfArcher_F.mbs", "Chara/ElfArcher_F00.ftx", "CharaFace/Face_ElfArcher_F.mbs", "CharaFace/Face_ElfArcher_F00.ftx", "CharaOp/Op_ElfArcher_F.mbs", "CharaOp/Op_ElfArcher_F00.ftx"}},
    {"ElfArcher_M", MbsFtx{"Chara/ElfArcher_M.mbs", "Chara/ElfArcher_M00.ftx", "CharaFace/Face_ElfArcher_M.mbs", "CharaFace/Face_ElfArcher_M00.ftx", "CharaOp/Op_ElfArcher_M.mbs", "CharaOp/Op_ElfArcher_M00.ftx"}},
    {"ElfFencer_F", MbsFtx{"Chara/ElfFencer_F.mbs", "Chara/ElfFencer_F00.ftx", "CharaFace/Face_ElfFencer_F.mbs", "CharaFace/Face_ElfFencer_F00.ftx", "CharaOp/Op_ElfFencer_F.mbs", "CharaOp/Op_ElfFencer_F00.ftx"}},
    {"ElfFencer_M", MbsFtx{"Chara/ElfFencer_M.mbs", "Chara/ElfFencer_M00.ftx", "CharaFace/Face_ElfFencer_M.mbs", "CharaFace/Face_ElfFencer_M00.ftx", "CharaOp/Op_ElfFencer_M.mbs", "CharaOp/Op_ElfFencer_M00.ftx"}},
    {"Eltlinde_F0", MbsFtx{"Chara/Eltlinde_F00.mbs", "Chara/Eltlinde_F00.ftx", "CharaFace/Face_Eltlinde_F00.mbs", "CharaFace/Face_Eltlinde_F00.ftx", "CharaOp/Op_Eltlinde_F00.mbs", "CharaOp/Op_Eltlinde_F00.ftx"}},
    {"Eltlinde_F1", MbsFtx{"Chara/Eltlinde_F10.mbs", "Chara/Eltlinde_F10.ftx", "CharaFace/Face_Eltlinde_F10.mbs", "CharaFace/Face_Eltlinde_F10.ftx", "CharaOp/Op_Eltlinde_F10.mbs", "CharaOp/Op_Eltlinde_F10.ftx"}},
    {"FeatherBow_F", MbsFtx{"Chara/FeatherBow_F.mbs", "Chara/FeatherBow_F00.ftx", "CharaFace/Face_FeatherBow_F.mbs", "CharaFace/Face_FeatherBow_F00.ftx", "CharaOp/Op_FeatherBow_F.mbs", "CharaOp/Op_FeatherBow_F00.ftx"}},
    {"FeatherRod_M", MbsFtx{"Chara/FeatherRod_M.mbs", "Chara/FeatherRod_M00.ftx", "CharaFace/Face_FeatherRod_M.mbs", "CharaFace/Face_FeatherRod_M00.ftx", "CharaOp/Op_FeatherRod_M.mbs", "CharaOp/Op_FeatherRod_M00.ftx"}},
    {"FeatherShield_M", MbsFtx{"Chara/FeatherShield_M.mbs", "Chara/FeatherShield_M00.ftx", "CharaFace/Face_FeatherShield_M.mbs", "CharaFace/Face_FeatherShield_M00.ftx", "CharaOp/Op_FeatherShield_M.mbs", "CharaOp/Op_FeatherShield_M00.ftx"}},
    {"FeatherSword_F", MbsFtx{"Chara/FeatherSword_F.mbs", "Chara/FeatherSword_F00.ftx", "CharaFace/Face_FeatherSword_F.mbs", "CharaFace/Face_FeatherSword_F00.ftx", "CharaOp/Op_FeatherSword_F.mbs", "CharaOp/Op_FeatherSword_F00.ftx"}},
    {"Fighter_HG_M", MbsFtx{"Chara/Fighter_HG_M.mbs", "Chara/Fighter_HG_M00.ftx", "CharaFace/Face_Fighter_HG_M.mbs", "CharaFace/Face_Fighter_HG_M00.ftx", "CharaOp/Op_Fighter_HG_M.mbs", "CharaOp/Op_Fighter_HG_M00.ftx"}},
    {"Fighter_M", MbsFtx{"Chara/Fighter_M.mbs", "Chara/Fighter_M00.ftx", "CharaFace/Face_Fighter_M.mbs", "CharaFace/Face_Fighter_M00.ftx", "CharaOp/Op_Fighter_M.mbs", "CharaOp/Op_Fighter_M00.ftx"}},
    {"Gladiator_HG_M", MbsFtx{"Chara/Gladiator_HG_M.mbs", "Chara/Gladiator_HG_M00.ftx", "CharaFace/Face_Gladiator_HG_M.mbs", "CharaFace/Face_Gladiator_HG_M00.ftx", "CharaOp/Op_Gladiator_HG_M.mbs", "CharaOp/Op_Gladiator_HG_M00.ftx"}},
    {"Gladiator_M", MbsFtx{"Chara/Gladiator_M.mbs", "Chara/Gladiator_M00.ftx", "CharaFace/Face_Gladiator_M.mbs", "CharaFace/Face_Gladiator_M00.ftx", "CharaOp/Op_Gladiator_M.mbs", "CharaOp/Op_Gladiator_M00.ftx"}},
    {"GriffonKnight_F", MbsFtx{"Chara/GriffonKnight_F.mbs", "Chara/GriffonKnight_F00.ftx", "CharaFace/Face_GriffonKnight_F.mbs", "CharaFace/Face_GriffonKnight_F00.ftx", "CharaOp/Op_GriffonKnight_F.mbs", "CharaOp/Op_GriffonKnight_F00.ftx"}},
    {"GriffonKnight_HG_F", MbsFtx{"Chara/GriffonKnight_HG_F.mbs", "Chara/GriffonKnight_HG_F00.ftx", "CharaFace/Face_GriffonKnight_HG_F.mbs", "CharaFace/Face_GriffonKnight_HG_F00.ftx", "CharaOp/Op_GriffonKnight_HG_F.mbs", "CharaOp/Op_GriffonKnight_HG_F00.ftx"}},
    {"Hoplite_M", MbsFtx{"Chara/Hoplite_M.mbs", "Chara/Hoplite_M00.ftx", "CharaFace/Face_Hoplite_M.mbs", "CharaFace/Face_Hoplite_M00.ftx", "CharaOp/Op_Hoplite_M.mbs", "CharaOp/Op_Hoplite_M00.ftx"}},
    {"Hoplite_HG_M", MbsFtx{"Chara/Hoplite_HG_M.mbs", "Chara/Hoplite_HG_M00.ftx", "CharaFace/Face_Hoplite_HG_M.mbs", "CharaFace/Face_Hoplite_HG_M00.ftx", "CharaOp/Op_Hoplite_HG_M.mbs", "CharaOp/Op_Hoplite_HG_M00.ftx"}},
    {"Hunter_M", MbsFtx{"Chara/Hunter_M.mbs", "Chara/Hunter_M00.ftx", "CharaFace/Face_Hunter_M.mbs", "CharaFace/Face_Hunter_M00.ftx", "CharaOp/Op_Hunter_M.mbs", "CharaOp/Op_Hunter_M00.ftx"}},
    {"Hunter_HG_M", MbsFtx{"Chara/Hunter_HG_M.mbs", "Chara/Hunter_HG_M00.ftx", "CharaFace/Face_Hunter_HG_M.mbs", "CharaFace/Face_Hunter_HG_M00.ftx", "CharaOp/Op_Hunter_HG_M.mbs", "CharaOp/Op_Hunter_HG_M00.ftx"}},
    {"Huscarl_M", MbsFtx{"Chara/Huscarl_M.mbs", "Chara/Huscarl_M00.ftx", "CharaFace/Face_Huscarl_M.mbs", "CharaFace/Face_Huscarl_M00.ftx", "CharaOp/Op_Huscarl_M.mbs", "CharaOp/Op_Huscarl_M00.ftx"}},
    {"Huscarl_HG_M", MbsFtx{"Chara/Huscarl_HG_M.mbs", "Chara/Huscarl_HG_M00.ftx", "CharaFace/Face_Huscarl_HG_M.mbs", "CharaFace/Face_Huscarl_HG_M00.ftx", "CharaOp/Op_Huscarl_HG_M.mbs", "CharaOp/Op_Huscarl_HG_M00.ftx"}},
    {"Knight_M", MbsFtx{"Chara/Knight_M.mbs", "Chara/Knight_M00.ftx", "CharaFace/Face_Knight_M.mbs", "CharaFace/Face_Knight_M00.ftx", "CharaOp/Op_Knight_M.mbs", "CharaOp/Op_Knight_M00.ftx"}},
    {"Knight_HG_M", MbsFtx{"Chara/Knight_HG_M.mbs", "Chara/Knight_HG_M00.ftx", "CharaFace/Face_Knight_HG_M.mbs", "CharaFace/Face_Knight_HG_M00.ftx", "CharaOp/Op_Knight_HG_M.mbs", "CharaOp/Op_Knight_HG_M00.ftx"}},
    {"Lord_M", MbsFtx{"Chara/Lord_M.mbs", "Chara/Lord_M00.ftx", "CharaFace/Face_Lord_M.mbs", "CharaFace/Face_Lord_M00.ftx", "CharaOp/Op_Lord_M.mbs", "CharaOp/Op_Lord_M00.ftx"}},
    {"Lord_HG_M", MbsFtx{"Chara/Lord_HG_M.mbs", "Chara/Lord_HG_M00.ftx", "CharaFace/Face_Lord_HG_M.mbs", "CharaFace/Face_Lord_HG_M00.ftx", "CharaOp/Op_Lord_HG_M.mbs", "CharaOp/Op_Lord_HG_M00.ftx"}},
    {"Mercenary_F", MbsFtx{"Chara/Mercenary_F.mbs", "Chara/Mercenary_F00.ftx", "CharaFace/Face_Mercenary_F.mbs", "CharaFace/Face_Mercenary_F00.ftx", "CharaOp/Op_Mercenary_F.mbs", "CharaOp/Op_Mercenary_F00.ftx"}},
    {"Mercenary_HG_F", MbsFtx{"Chara/Mercenary_HG_F.mbs", "Chara/Mercenary_HG_F00.ftx", "CharaFace/Face_Mercenary_HG_F.mbs", "CharaFace/Face_Mercenary_HG_F00.ftx", "CharaOp/Op_Mercenary_HG_F.mbs", "CharaOp/Op_Mercenary_HG_F00.ftx"}},
    {"Mercenary_M", MbsFtx{"Chara/Mercenary_M.mbs", "Chara/Mercenary_M00.ftx", "CharaFace/Face_Mercenary_M.mbs", "CharaFace/Face_Mercenary_M00.ftx", "CharaOp/Op_Mercenary_M.mbs", "CharaOp/Op_Mercenary_M00.ftx"}},
    {"Mercenary_HG_M", MbsFtx{"Chara/Mercenary_HG_M.mbs", "Chara/Mercenary_HG_M00.ftx", "CharaFace/Face_Mercenary_HG_M.mbs", "CharaFace/Face_Mercenary_HG_M00.ftx", "CharaOp/Op_Mercenary_HG_M.mbs", "CharaOp/Op_Mercenary_HG_M00.ftx"}},
    {"Necromancer_M", MbsFtx{"Chara/Necromancer_M.mbs", "Chara/Necromancer_M00.ftx", "CharaFace/Face_Necromancer_M.mbs", "CharaFace/Face_Necromancer_M00.ftx", "CharaOp/Op_Necromancer_M.mbs", "CharaOp/Op_Necromancer_M00.ftx"}},
    {"Prince_M", MbsFtx{"Chara/Prince_M.mbs", "Chara/Prince_M00.ftx", "CharaFace/Face_Prince_M.mbs", "CharaFace/Face_Prince_M00.ftx", "CharaOp/Op_Prince_M.mbs", "CharaOp/Op_Prince_M00.ftx"}},
    {"Ranger_F", MbsFtx{"Chara/Ranger_F.mbs", "Chara/Ranger_F00.ftx", "CharaFace/Face_Ranger_F.mbs", "CharaFace/Face_Ranger_F00.ftx", "CharaOp/Op_Ranger_F.mbs", "CharaOp/Op_Ranger_F00.ftx"}},
    {"Ranger_HG_F", MbsFtx{"Chara/Ranger_HG_F.mbs", "Chara/Ranger_HG_F00.ftx", "CharaFace/Face_Ranger_HG_F.mbs", "CharaFace/Face_Ranger_HG_F00.ftx", "CharaOp/Op_Ranger_HG_F.mbs", "CharaOp/Op_Ranger_HG_F00.ftx"}},
    {"Scarlet_F", MbsFtx{"Chara/Scarlet_F.mbs", "Chara/Scarlet_F00.ftx", "CharaFace/Face_Scarlet_F.mbs", "CharaFace/Face_Scarlet_F00.ftx", "CharaOp/Op_Scarlet_F.mbs", "CharaOp/Op_Scarlet_F00.ftx"}},
    {"Shaman_F", MbsFtx{"Chara/Shaman_F.mbs", "Chara/Shaman_F00.ftx", "CharaFace/Face_Shaman_F.mbs", "CharaFace/Face_Shaman_F00.ftx", "CharaOp/Op_Shaman_F.mbs", "CharaOp/Op_Shaman_F00.ftx"}},
    {"Shaman_HG_F", MbsFtx{"Chara/Shaman_HG_F.mbs", "Chara/Shaman_HG_F00.ftx", "CharaFace/Face_Shaman_HG_F.mbs", "CharaFace/Face_Shaman_HG_F00.ftx", "CharaOp/Op_Shaman_HG_F.mbs", "CharaOp/Op_Shaman_HG_F00.ftx"}},
    {"Soldier_F", MbsFtx{"Chara/Soldier_F.mbs", "Chara/Soldier_F00.ftx", "CharaFace/Face_Soldier_F.mbs", "CharaFace/Face_Soldier_F00.ftx", "CharaOp/Op_Soldier_F.mbs", "CharaOp/Op_Soldier_F00.ftx"}},
    {"Soldier_HG_F", MbsFtx{"Chara/Soldier_HG_F.mbs", "Chara/Soldier_HG_F00.ftx", "CharaFace/Face_Soldier_HG_F.mbs", "CharaFace/Face_Soldier_HG_F00.ftx", "CharaOp/Op_Soldier_HG_F.mbs", "CharaOp/Op_Soldier_HG_F00.ftx"}},
    {"Soldier_M", MbsFtx{"Chara/Soldier_M.mbs", "Chara/Soldier_M00.ftx", "CharaFace/Face_Soldier_M.mbs", "CharaFace/Face_Soldier_M00.ftx", "CharaOp/Op_Soldier_M.mbs", "CharaOp/Op_Soldier_M00.ftx"}},
    {"Soldier_HG_M", MbsFtx{"Chara/Soldier_HG_M.mbs", "Chara/Soldier_HG_M00.ftx", "CharaFace/Face_Soldier_HG_M.mbs", "CharaFace/Face_Soldier_HG_M00.ftx", "CharaOp/Op_Soldier_HG_M.mbs", "CharaOp/Op_Soldier_HG_M00.ftx"}},
    {"Swordsman_F", MbsFtx{"Chara/Swordsman_F.mbs", "Chara/Swordsman_F00.ftx", "CharaFace/Face_Swordsman_F.mbs", "CharaFace/Face_Swordsman_F00.ftx", "CharaOp/Op_Swordsman_F.mbs", "CharaOp/Op_Swordsman_F00.ftx"}},
    {"Swordsman_HG_F", MbsFtx{"Chara/Swordsman_HG_F.mbs", "Chara/Swordsman_HG_F00.ftx", "CharaFace/Face_Swordsman_HG_F.mbs", "CharaFace/Face_Swordsman_HG_F00.ftx", "CharaOp/Op_Swordsman_HG_F.mbs", "CharaOp/Op_Swordsman_HG_F00.ftx"}},
    {"Swordsman_M", MbsFtx{"Chara/Swordsman_M.mbs", "Chara/Swordsman_M00.ftx", "CharaFace/Face_Swordsman_M.mbs", "CharaFace/Face_Swordsman_M00.ftx", "CharaOp/Op_Swordsman_M.mbs", "CharaOp/Op_Swordsman_M00.ftx"}},
    {"Swordsman_HG_M", MbsFtx{"Chara/Swordsman_HG_M.mbs", "Chara/Swordsman_HG_M00.ftx", "CharaFace/Face_Swordsman_HG_M.mbs", "CharaFace/Face_Swordsman_HG_M00.ftx", "CharaOp/Op_Swordsman_HG_M.mbs", "CharaOp/Op_Swordsman_HG_M00.ftx"}},
    {"Thief_M", MbsFtx{"Chara/Thief_M.mbs", "Chara/Thief_M00.ftx", "CharaFace/Face_Thief_M.mbs", "CharaFace/Face_Thief_M00.ftx", "CharaOp/Op_Thief_M.mbs", "CharaOp/Op_Thief_M00.ftx"}},
    {"Thief_HG_M", MbsFtx{"Chara/Thief_HG_M.mbs", "Chara/Thief_HG_M00.ftx", "CharaFace/Face_Thief_HG_M.mbs", "CharaFace/Face_Thief_HG_M00.ftx", "CharaOp/Op_Thief_HG_M.mbs", "CharaOp/Op_Thief_HG_M00.ftx"}},
    {"Unify_M", MbsFtx{"Chara/Unify_M.mbs", "Chara/Unify_M00.ftx", "CharaFace/Face_Unify_M.mbs", "CharaFace/Face_Unify_M00.ftx", "CharaOp/Op_Unify_M.mbs", "CharaOp/Op_Unify_M00.ftx"}},
    {"Virginia_F", MbsFtx{"Chara/Virginia_F.mbs", "Chara/Virginia_F00.ftx", "CharaFace/Face_Virginia_F.mbs", "CharaFace/Face_Virginia_F00.ftx", "CharaOp/Op_Virginia_F.mbs", "CharaOp/Op_Virginia_F00.ftx"}},
    {"Warrior_F", MbsFtx{"Chara/Warrior_F.mbs", "Chara/Warrior_F00.ftx", "CharaFace/Face_Warrior_F.mbs", "CharaFace/Face_Warrior_F00.ftx", "CharaOp/Op_Warrior_F.mbs", "CharaOp/Op_Warrior_F00.ftx"}},
    {"Warrior_HG_F", MbsFtx{"Chara/Warrior_HG_F.mbs", "Chara/Warrior_HG_F00.ftx", "CharaFace/Face_Warrior_HG_F.mbs", "CharaFace/Face_Warrior_HG_F00.ftx", "CharaOp/Op_Warrior_HG_F.mbs", "CharaOp/Op_Warrior_HG_F00.ftx"}},
    {"Warrior_M", MbsFtx{"Chara/Warrior_M.mbs", "Chara/Warrior_M00.ftx", "CharaFace/Face_Warrior_M.mbs", "CharaFace/Face_Warrior_M00.ftx", "CharaOp/Op_Warrior_M.mbs", "CharaOp/Op_Warrior_M00.ftx"}},
    {"Warrior_HG_M", MbsFtx{"Chara/Warrior_HG_M.mbs", "Chara/Warrior_HG_M00.ftx", "CharaFace/Face_Warrior_HG_M.mbs", "CharaFace/Face_Warrior_HG_M00.ftx", "CharaOp/Op_Warrior_HG_M.mbs", "CharaOp/Op_Warrior_HG_M00.ftx"}},
    {"WereBear_M", MbsFtx{"Chara/WereBear_M.mbs", "Chara/WereBear_M00.ftx", "CharaFace/Face_WereBear_M.mbs", "CharaFace/Face_WereBear_M00.ftx", "CharaOp/Op_WereBear_M.mbs", "CharaOp/Op_WereBear_M00.ftx"}},
    {"WereFox_F", MbsFtx{"Chara/WereFox_F.mbs", "Chara/WereFox_F00.ftx", "CharaFace/Face_WereFox_F.mbs", "CharaFace/Face_WereFox_F00.ftx", "CharaOp/Op_WereFox_F.mbs", "CharaOp/Op_WereFox_F00.ftx"}},
    {"WereOwl_F", MbsFtx{"Chara/WereOwl_F.mbs", "Chara/WereOwl_F00.ftx", "CharaFace/Face_WereOwl_F.mbs", "CharaFace/Face_WereOwl_F00.ftx", "CharaOp/Op_WereOwl_F.mbs", "CharaOp/Op_WereOwl_F00.ftx"}},
    {"WereWolf_M", MbsFtx{"Chara/WereWolf_M.mbs", "Chara/WereWolf_M00.ftx", "CharaFace/Face_WereWolf_M.mbs", "CharaFace/Face_WereWolf_M00.ftx", "CharaOp/Op_WereWolf_M.mbs", "CharaOp/Op_WereWolf_M00.ftx"}},
    {"WhiteKnight_F", MbsFtx{"Chara/WhiteKnight_F.mbs", "Chara/WhiteKnight_F00.ftx", "CharaFace/Face_WhiteKnight_F.mbs", "CharaFace/Face_WhiteKnight_F00.ftx", "CharaOp/Op_WhiteKnight_F.mbs", "CharaOp/Op_WhiteKnight_F00.ftx"}},
    {"WhiteKnight_HG_F", MbsFtx{"Chara/WhiteKnight_HG_F.mbs", "Chara/WhiteKnight_HG_F00.ftx", "CharaFace/Face_WhiteKnight_HG_F.mbs", "CharaFace/Face_WhiteKnight_HG_F00.ftx", "CharaOp/Op_WhiteKnight_HG_F.mbs", "CharaOp/Op_WhiteKnight_HG_F00.ftx"}},
    {"WhiteKnight_HG_M", MbsFtx{"Chara/WhiteKnight_HG_M.mbs", "Chara/WhiteKnight_HG_M00.ftx", "CharaFace/Face_WhiteKnight_HG_M.mbs", "CharaFace/Face_WhiteKnight_HG_M00.ftx", "CharaOp/Op_WhiteKnight_HG_M.mbs", "CharaOp/Op_WhiteKnight_HG_M00.ftx"}},
    {"Witch_F", MbsFtx{"Chara/Witch_F.mbs", "Chara/Witch_F00.ftx", "CharaFace/Face_Witch_F.mbs", "CharaFace/Face_Witch_F00.ftx", "CharaOp/Op_Witch_F.mbs", "CharaOp/Op_Witch_F00.ftx"}},
    {"Witch_HG_F", MbsFtx{"Chara/Witch_HG_F.mbs", "Chara/Witch_HG_F00.ftx", "CharaFace/Face_Witch_HG_F.mbs", "CharaFace/Face_Witch_HG_F00.ftx", "CharaOp/Op_Witch_HG_F.mbs", "CharaOp/Op_Witch_HG_F00.ftx"}},
    {"Wizard_M", MbsFtx{"Chara/Wizard_M.mbs", "Chara/Wizard_M00.ftx", "CharaFace/Face_Wizard_M.mbs", "CharaFace/Face_Wizard_M00.ftx", "CharaOp/Op_Wizard_M.mbs", "CharaOp/Op_Wizard_M00.ftx"}},
    {"Wizard_HG_M", MbsFtx{"Chara/Wizard_HG_M.mbs", "Chara/Wizard_HG_M00.ftx", "CharaFace/Face_Wizard_HG_M.mbs", "CharaFace/Face_Wizard_HG_M00.ftx", "CharaOp/Op_Wizard_HG_M.mbs", "CharaOp/Op_Wizard_HG_M00.ftx"}},
    {"WyvernKnight_F", MbsFtx{"Chara/WyvernKnight_F.mbs", "Chara/WyvernKnight_F00.ftx", "CharaFace/Face_WyvernKnight_F.mbs", "CharaFace/Face_WyvernKnight_F00.ftx", "CharaOp/Op_WyvernKnight_F.mbs", "CharaOp/Op_WyvernKnight_F00.ftx"}},
    {"WyvernKnight_HG_F", MbsFtx{"Chara/WyvernKnight_HG_F.mbs", "Chara/WyvernKnight_HG_F00.ftx", "CharaFace/Face_WyvernKnight_HG_F.mbs", "CharaFace/Face_WyvernKnight_HG_F00.ftx", "CharaOp/Op_WyvernKnight_HG_F.mbs", "CharaOp/Op_WyvernKnight_HG_F00.ftx"}},
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