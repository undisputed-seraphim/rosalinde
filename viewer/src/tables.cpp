#include "tables.hpp"

#include <cstdlib>
#include <fstream>

namespace {
std::string data_dir() {
	if (const char* e = std::getenv("VIEWER_DATA"))
		return e;
	return VIEWER_DATA_DIR;
}

std::map<std::string, Job, std::less<>> load_characters() {
	std::map<std::string, Job, std::less<>> out;
	std::ifstream f(data_dir() + "/classes.tsv");
	std::string line;
	while (std::getline(f, line)) {
		if (line.empty() || line[0] == '#')
			continue;
		std::vector<std::string> c;
		size_t s = 0;
		for (size_t i = 0; i <= line.size(); ++i)
			if (i == line.size() || line[i] == '\t') {
				c.push_back(line.substr(s, i - s));
				s = i + 1;
			}
		if (c.size() < 4)
			continue;
		Job j;
		j.mbs = {"Chara", c[2]};
		j.ftx = {"Chara", c[3]};
		out.emplace(c[0], std::move(j));
	}
	std::ifstream v(data_dir() + "/variants.tsv");
	while (std::getline(v, line)) {
		if (line.empty() || line[0] == '#')
			continue;
		std::vector<std::string> c;
		size_t s = 0;
		for (size_t i = 0; i <= line.size(); ++i)
			if (i == line.size() || line[i] == '\t') {
				c.push_back(line.substr(s, i - s));
				s = i + 1;
			}
		if (c.size() < 3)
			continue;
		auto it = out.find(c[0]);
		if (it == out.end())
			continue;
		it->second.variants[c[1]] = static_cast<uint32_t>(std::stoul(c[2], nullptr, 0));
	}
	return out;
}
} // namespace


// clang-format off
const std::map<std::string, Job, std::less<>> Characters = load_characters();

const std::map<std::string, Job, std::less<>> BattleBGs = {
	{"BGBtAbandonedPort_B", Job{{"BattleBG", "BGBtAbandonedPort_B.mbs"}, {"BattleBG", "BGBtAbandonedPort_B.ftx"}, {}}},
	{"BGBtAirRock_01", Job{{"BattleBG", "BGBtAirRock_01.mbs"}, {"BattleBG", "BGBtAirRock_01.ftx"}, {}}},
	{"BGBtAirRock_B_01", Job{{"BattleBG", "BGBtAirRock_B_01.mbs"}, {"BattleBG", "BGBtAirRock_B_01.ftx"}, {}}},
	{"BGBtAirRock_D_01", Job{{"BattleBG", "BGBtAirRock_D_01.mbs"}, {"BattleBG", "BGBtAirRock_D_01.ftx"}, {}}},
	{"BGBtAlbionForest_a", Job{{"BattleBG", "BGBtAlbionForest_a.mbs"}, {"BattleBG", "BGBtAlbionForest_a.ftx"}, {}}},
	{"BGBtBridge_A", Job{{"BattleBG", "BGBtBridge_A.mbs"}, {"BattleBG", "BGBtBridge_A.ftx"}, {}}},
	{"BGBtCampAnime", Job{{"BattleBG", "BGBtCampAnime.mbs"}, {"BattleBG", "BGBtCampAnime.ftx"}, {}}},
	{"BGBtCampDesertPlain", Job{{"BattleBG", "BGBtCampDesertPlain.mbs"}, {"BattleBG", "BGBtCampDesertPlain.ftx"}, {}}},
	{"BGBtCampElfPlain", Job{{"BattleBG", "BGBtCampElfPlain.mbs"}, {"BattleBG", "BGBtCampElfPlain.ftx"}, {}}},
	{"BGBtCampPlain", Job{{"BattleBG", "BGBtCampPlain.mbs"}, {"BattleBG", "BGBtCampPlain.ftx"}, {}}},
	{"BGBtCampPlain_A", Job{{"BattleBG", "BGBtCampPlain_A.mbs"}, {"BattleBG", "BGBtCampPlain_A.ftx"}, {}}},
	{"BGBtCampSnowPlain", Job{{"BattleBG", "BGBtCampSnowPlain.mbs"}, {"BattleBG", "BGBtCampSnowPlain.ftx"}, {}}},
	{"BGBtCampThornForest", Job{{"BattleBG", "BGBtCampThornForest.mbs"}, {"BattleBG", "BGBtCampThornForest.ftx"}, {}}},
	{"BGBtCampWasteland", Job{{"BattleBG", "BGBtCampWasteland.mbs"}, {"BattleBG", "BGBtCampWasteland.ftx"}, {}}},
	{"BGBtCapital_A_a", Job{{"BattleBG", "BGBtCapital_A_a.mbs"}, {"BattleBG", "BGBtCapital_A_a.ftx"}, {}}},
	{"BGBtCapital_B_01", Job{{"BattleBG", "BGBtCapital_B_01.mbs"}, {"BattleBG", "BGBtCapital_B_01.ftx"}, {}}},
	{"BGBtCapital_C_a", Job{{"BattleBG", "BGBtCapital_C_a.mbs"}, {"BattleBG", "BGBtCapital_C_a.ftx"}, {}}},
	{"BGBtCapital_C_b", Job{{"BattleBG", "BGBtCapital_C_b.mbs"}, {"BattleBG", "BGBtCapital_C_b.ftx"}, {}}},
	{"BGBtCapital_D_a", Job{{"BattleBG", "BGBtCapital_D_a.mbs"}, {"BattleBG", "BGBtCapital_D_a.ftx"}, {}}},
	{"BGBtCapital_D_b", Job{{"BattleBG", "BGBtCapital_D_b.mbs"}, {"BattleBG", "BGBtCapital_D_b.ftx"}, {}}},
	{"BGBtCapital_E_a", Job{{"BattleBG", "BGBtCapital_E_a.mbs"}, {"BattleBG", "BGBtCapital_E_a.ftx"}, {}}},
	{"BGBtCastleGate_c", Job{{"BattleBG", "BGBtCastleGate_c.mbs"}, {"BattleBG", "BGBtCastleGate_c.ftx"}, {}}},
	{"BGBtCathedral", Job{{"BattleBG", "BGBtCathedral.mbs"}, {"BattleBG", "BGBtCathedral.ftx"}, {}}},
	{"BGBtCathedral_A", Job{{"BattleBG", "BGBtCathedral_A.mbs"}, {"BattleBG", "BGBtCathedral_A.ftx"}, {}}},
	{"BGBtCave_a", Job{{"BattleBG", "BGBtCave_a.mbs"}, {"BattleBG", "BGBtCave_a.ftx"}, {}}},
	{"BGBtColosseum_a", Job{{"BattleBG", "BGBtColosseum_a.mbs"}, {"BattleBG", "BGBtColosseum_a.ftx"}, {}}},
	{"BGBtDarkElfCave_a", Job{{"BattleBG", "BGBtDarkElfCave_a.mbs"}, {"BattleBG", "BGBtDarkElfCave_a.ftx"}, {}}},
	{"BGBtDesertOasis_a", Job{{"BattleBG", "BGBtDesertOasis_a.mbs"}, {"BattleBG", "BGBtDesertOasis_a.ftx"}, {}}},
	{"BGBtDesertPlain_a", Job{{"BattleBG", "BGBtDesertPlain_a.mbs"}, {"BattleBG", "BGBtDesertPlain_a.ftx"}, {}}},
	{"BGBtDesertPlain_b", Job{{"BattleBG", "BGBtDesertPlain_b.mbs"}, {"BattleBG", "BGBtDesertPlain_b.ftx"}, {}}},
	{"BGBtDesertPlain_c", Job{{"BattleBG", "BGBtDesertPlain_c.mbs"}, {"BattleBG", "BGBtDesertPlain_c.ftx"}, {}}},
	{"BGBtElfForest_a", Job{{"BattleBG", "BGBtElfForest_a.mbs"}, {"BattleBG", "BGBtElfForest_a.ftx"}, {}}},
	{"BGBtElfPlain_a", Job{{"BattleBG", "BGBtElfPlain_a.mbs"}, {"BattleBG", "BGBtElfPlain_a.ftx"}, {}}},
	{"BGBtElfPlain_b", Job{{"BattleBG", "BGBtElfPlain_b.mbs"}, {"BattleBG", "BGBtElfPlain_b.ftx"}, {}}},
	{"BGBtForest_a", Job{{"BattleBG", "BGBtForest_a.mbs"}, {"BattleBG", "BGBtForest_a.ftx"}, {}}},
	{"BGBtFort", Job{{"BattleBG", "BGBtFort.mbs"}, {"BattleBG", "BGBtFort.ftx"}, {}}},
	{"BGBtFort_A", Job{{"BattleBG", "BGBtFort_A.mbs"}, {"BattleBG", "BGBtFort_A.ftx"}, {}}},
	{"BGBtFort_E", Job{{"BattleBG", "BGBtFort_E.mbs"}, {"BattleBG", "BGBtFort_E.ftx"}, {}}},
	{"BGBtGraveyard", Job{{"BattleBG", "BGBtGraveyard.mbs"}, {"BattleBG", "BGBtGraveyard.ftx"}, {}}},
	{"BGBtIceFall", Job{{"BattleBG", "BGBtIceFall.mbs"}, {"BattleBG", "BGBtIceFall.ftx"}, {}}},
	{"BGBtLakeside_a", Job{{"BattleBG", "BGBtLakeside_a.mbs"}, {"BattleBG", "BGBtLakeside_a.ftx"}, {}}},
	{"BGBtLimestoneCave_a", Job{{"BattleBG", "BGBtLimestoneCave_a.mbs"}, {"BattleBG", "BGBtLimestoneCave_a.ftx"}, {}}},
	{"BGBtNightsky_a", Job{{"BattleBG", "BGBtNightsky_a.mbs"}, {"BattleBG", "BGBtNightsky_a.ftx"}, {}}},
	{"BGBtPlain_A_2", Job{{"BattleBG", "BGBtPlain_A_2.mbs"}, {"BattleBG", "BGBtPlain_A_2.ftx"}, {}}},
	{"BGBtPlain_A_a", Job{{"BattleBG", "BGBtPlain_A_a.mbs"}, {"BattleBG", "BGBtPlain_A_a.ftx"}, {}}},
	{"BGBtPlain_A_b", Job{{"BattleBG", "BGBtPlain_A_b.mbs"}, {"BattleBG", "BGBtPlain_A_b.ftx"}, {}}},
	{"BGBtPlain_A_c", Job{{"BattleBG", "BGBtPlain_A_c.mbs"}, {"BattleBG", "BGBtPlain_A_c.ftx"}, {}}},
	{"BGBtPlain_A_d", Job{{"BattleBG", "BGBtPlain_A_d.mbs"}, {"BattleBG", "BGBtPlain_A_d.ftx"}, {}}},
	{"BGBtPlain_a", Job{{"BattleBG", "BGBtPlain_a.mbs"}, {"BattleBG", "BGBtPlain_a.ftx"}, {}}},
	{"BGBtPlain_b", Job{{"BattleBG", "BGBtPlain_b.mbs"}, {"BattleBG", "BGBtPlain_b.ftx"}, {}}},
	{"BGBtPort_B_a", Job{{"BattleBG", "BGBtPort_B_a.mbs"}, {"BattleBG", "BGBtPort_B_a.ftx"}, {}}},
	{"BGBtPort_B_b", Job{{"BattleBG", "BGBtPort_B_b.mbs"}, {"BattleBG", "BGBtPort_B_b.ftx"}, {}}},
	{"BGBtPort_B_c", Job{{"BattleBG", "BGBtPort_B_c.mbs"}, {"BattleBG", "BGBtPort_B_c.ftx"}, {}}},
	{"BGBtPort_D", Job{{"BattleBG", "BGBtPort_D.mbs"}, {"BattleBG", "BGBtPort_D.ftx"}, {}}},
	{"BGBtPort_a", Job{{"BattleBG", "BGBtPort_a.mbs"}, {"BattleBG", "BGBtPort_a.ftx"}, {}}},
	{"BGBtRoad_a", Job{{"BattleBG", "BGBtRoad_a.mbs"}, {"BattleBG", "BGBtRoad_a.ftx"}, {}}},
	{"BGBtRoad_b", Job{{"BattleBG", "BGBtRoad_b.mbs"}, {"BattleBG", "BGBtRoad_b.ftx"}, {}}},
	{"BGBtRoad_c", Job{{"BattleBG", "BGBtRoad_c.mbs"}, {"BattleBG", "BGBtRoad_c.ftx"}, {}}},
	{"BGBtRoad_d", Job{{"BattleBG", "BGBtRoad_d.mbs"}, {"BattleBG", "BGBtRoad_d.ftx"}, {}}},
	{"BGBtRoad_e", Job{{"BattleBG", "BGBtRoad_e.mbs"}, {"BattleBG", "BGBtRoad_e.ftx"}, {}}},
	{"BGBtRoad_f", Job{{"BattleBG", "BGBtRoad_f.mbs"}, {"BattleBG", "BGBtRoad_f.ftx"}, {}}},
	{"BGBtRoad_g", Job{{"BattleBG", "BGBtRoad_g.mbs"}, {"BattleBG", "BGBtRoad_g.ftx"}, {}}},
	{"BGBtRoad_h", Job{{"BattleBG", "BGBtRoad_h.mbs"}, {"BattleBG", "BGBtRoad_h.ftx"}, {}}},
	{"BGBtRoad_i", Job{{"BattleBG", "BGBtRoad_i.mbs"}, {"BattleBG", "BGBtRoad_i.ftx"}, {}}},
	{"BGBtRoad_j", Job{{"BattleBG", "BGBtRoad_j.mbs"}, {"BattleBG", "BGBtRoad_j.ftx"}, {}}},
	{"BGBtRoad_k", Job{{"BattleBG", "BGBtRoad_k.mbs"}, {"BattleBG", "BGBtRoad_k.ftx"}, {}}},
	{"BGBtRoad_l", Job{{"BattleBG", "BGBtRoad_l.mbs"}, {"BattleBG", "BGBtRoad_l.ftx"}, {}}},
	{"BGBtRuins_a", Job{{"BattleBG", "BGBtRuins_a.mbs"}, {"BattleBG", "BGBtRuins_a.ftx"}, {}}},
	{"BGBtRuins_b", Job{{"BattleBG", "BGBtRuins_b.mbs"}, {"BattleBG", "BGBtRuins_b.ftx"}, {}}},
	{"BGBtSanctuary", Job{{"BattleBG", "BGBtSanctuary.mbs"}, {"BattleBG", "BGBtSanctuary.ftx"}, {}}},
	{"BGBtSanctum", Job{{"BattleBG", "BGBtSanctum.mbs"}, {"BattleBG", "BGBtSanctum.ftx"}, {}}},
	{"BGBtSanctum_Opening", Job{{"BattleBG", "BGBtSanctum_Opening.mbs"}, {"BattleBG", "BGBtSanctum_Opening.ftx"}, {}}},
	{"BGBtShore_a", Job{{"BattleBG", "BGBtShore_a.mbs"}, {"BattleBG", "BGBtShore_a.ftx"}, {}}},
	{"BGBtSnowForest_a", Job{{"BattleBG", "BGBtSnowForest_a.mbs"}, {"BattleBG", "BGBtSnowForest_a.ftx"}, {}}},
	{"BGBtSnowPlain_a", Job{{"BattleBG", "BGBtSnowPlain_a.mbs"}, {"BattleBG", "BGBtSnowPlain_a.ftx"}, {}}},
	{"BGBtSnowPlain_b", Job{{"BattleBG", "BGBtSnowPlain_b.mbs"}, {"BattleBG", "BGBtSnowPlain_b.ftx"}, {}}},
	{"BGBtSnowPlain_c", Job{{"BattleBG", "BGBtSnowPlain_c.mbs"}, {"BattleBG", "BGBtSnowPlain_c.ftx"}, {}}},
	{"BGBtSnowRuins_a", Job{{"BattleBG", "BGBtSnowRuins_a.mbs"}, {"BattleBG", "BGBtSnowRuins_a.ftx"}, {}}},
	{"BGBtSnowRuins_b", Job{{"BattleBG", "BGBtSnowRuins_b.mbs"}, {"BattleBG", "BGBtSnowRuins_b.ftx"}, {}}},
	{"BGBtSnowShore_a", Job{{"BattleBG", "BGBtSnowShore_a.mbs"}, {"BattleBG", "BGBtSnowShore_a.ftx"}, {}}},
	{"BGBtSquare_C_01", Job{{"BattleBG", "BGBtSquare_C_01.mbs"}, {"BattleBG", "BGBtSquare_C_01.ftx"}, {}}},
	{"BGBtThornForest_a", Job{{"BattleBG", "BGBtThornForest_a.mbs"}, {"BattleBG", "BGBtThornForest_a.ftx"}, {}}},
	{"BGBtTown_A_Citadel", Job{{"BattleBG", "BGBtTown_A_Citadel.mbs"}, {"BattleBG", "BGBtTown_A_Citadel.ftx"}, {}}},
	{"BGBtTown_A_Cliff_01", Job{{"BattleBG", "BGBtTown_A_Cliff_01.mbs"}, {"BattleBG", "BGBtTown_A_Cliff_01.ftx"}, {}}},
	{"BGBtTown_A_a", Job{{"BattleBG", "BGBtTown_A_a.mbs"}, {"BattleBG", "BGBtTown_A_a.ftx"}, {}}},
	{"BGBtTown_B_Forest_a", Job{{"BattleBG", "BGBtTown_B_Forest_a.mbs"}, {"BattleBG", "BGBtTown_B_Forest_a.ftx"}, {}}},
	{"BGBtTown_B_a", Job{{"BattleBG", "BGBtTown_B_a.mbs"}, {"BattleBG", "BGBtTown_B_a.ftx"}, {}}},
	{"BGBtTown_C_Citadel_01", Job{{"BattleBG", "BGBtTown_C_Citadel_01.mbs"}, {"BattleBG", "BGBtTown_C_Citadel_01.ftx"}, {}}},
	{"BGBtTown_D_Citadel", Job{{"BattleBG", "BGBtTown_D_Citadel.mbs"}, {"BattleBG", "BGBtTown_D_Citadel.ftx"}, {}}},
	{"BGBtTown_D_a", Job{{"BattleBG", "BGBtTown_D_a.mbs"}, {"BattleBG", "BGBtTown_D_a.ftx"}, {}}},
	{"BGBtTown_D_b", Job{{"BattleBG", "BGBtTown_D_b.mbs"}, {"BattleBG", "BGBtTown_D_b.ftx"}, {}}},
	{"BGBtTown_E_Elf_a", Job{{"BattleBG", "BGBtTown_E_Elf_a.mbs"}, {"BattleBG", "BGBtTown_E_Elf_a.ftx"}, {}}},
	{"BGBtTown_E_Human_a", Job{{"BattleBG", "BGBtTown_E_Human_a.mbs"}, {"BattleBG", "BGBtTown_E_Human_a.ftx"}, {}}},
	{"BGBtTown_E_Human_b", Job{{"BattleBG", "BGBtTown_E_Human_b.mbs"}, {"BattleBG", "BGBtTown_E_Human_b.ftx"}, {}}},
	{"BGBtTown_a", Job{{"BattleBG", "BGBtTown_a.mbs"}, {"BattleBG", "BGBtTown_a.ftx"}, {}}},
	{"BGBtUndergroundRuinsDesert", Job{{"BattleBG", "BGBtUndergroundRuinsDesert.mbs"}, {"BattleBG", "BGBtUndergroundRuinsDesert.ftx"}, {}}},
	{"BGBtUndergroundRuins_a", Job{{"BattleBG", "BGBtUndergroundRuins_a.mbs"}, {"BattleBG", "BGBtUndergroundRuins_a.ftx"}, {}}},
	{"BGBtUndergroundRuins_b", Job{{"BattleBG", "BGBtUndergroundRuins_b.mbs"}, {"BattleBG", "BGBtUndergroundRuins_b.ftx"}, {}}},
	{"BGBtVillage_a", Job{{"BattleBG", "BGBtVillage_a.mbs"}, {"BattleBG", "BGBtVillage_a.ftx"}, {}}},
	{"BGBtWarPlain", Job{{"BattleBG", "BGBtWarPlain.mbs"}, {"BattleBG", "BGBtWarPlain.ftx"}, {}}},
	{"BGBtWastelandForest_a", Job{{"BattleBG", "BGBtWastelandForest_a.mbs"}, {"BattleBG", "BGBtWastelandForest_a.ftx"}, {}}},
	{"BGBtWastelandMist", Job{{"BattleBG", "BGBtWastelandMist.mbs"}, {"BattleBG", "BGBtWastelandMist.ftx"}, {}}},
	{"BGBtWasteland_a", Job{{"BattleBG", "BGBtWasteland_a.mbs"}, {"BattleBG", "BGBtWasteland_a.ftx"}, {}}},
	{"BGBtWetlandVillage_a", Job{{"BattleBG", "BGBtWetlandVillage_a.mbs"}, {"BattleBG", "BGBtWetlandVillage_a.ftx"}, {}}},
	{"BGBtWetland_a", Job{{"BattleBG", "BGBtWetland_a.mbs"}, {"BattleBG", "BGBtWetland_a.ftx"}, {}}},
	{"BGBtWoodFort", Job{{"BattleBG", "BGBtWoodFort.mbs"}, {"BattleBG", "BGBtWoodFort.ftx"}, {}}},
	{"BGBtWoodFort_B", Job{{"BattleBG", "BGBtWoodFort_B.mbs"}, {"BattleBG", "BGBtWoodFort_B.ftx"}, {}}},
	{"BGEffect", Job{{"BattleBG", "BGEffect.mbs"}, {"BattleBG", "BGEffect.ftx"}, {}}},
	{"BGEtWaitingRoom", Job{{"BattleBG", "BGEtWaitingRoom.mbs"}, {"BattleBG", "BGEtWaitingRoom.ftx"}, {}}},
	{"BGEvArenaRoom", Job{{"BattleBG", "BGEvArenaRoom.mbs"}, {"BattleBG", "BGEvArenaRoom.ftx"}, {}}},
	{"BGEvAssemblyHall_a", Job{{"BattleBG", "BGEvAssemblyHall_a.mbs"}, {"BattleBG", "BGEvAssemblyHall_a.ftx"}, {}}},
	{"BGEvBalcony_D_01", Job{{"BattleBG", "BGEvBalcony_D_01.mbs"}, {"BattleBG", "BGEvBalcony_D_01.ftx"}, {}}},
	{"BGEvBar_a", Job{{"BattleBG", "BGEvBar_a.mbs"}, {"BattleBG", "BGEvBar_a.ftx"}, {}}},
	{"BGEvBasin_a", Job{{"BattleBG", "BGEvBasin_a.mbs"}, {"BattleBG", "BGEvBasin_a.ftx"}, {}}},
	{"BGEvBazaar_a", Job{{"BattleBG", "BGEvBazaar_a.mbs"}, {"BattleBG", "BGEvBazaar_a.ftx"}, {}}},
	{"BGEvBedroom", Job{{"BattleBG", "BGEvBedroom.mbs"}, {"BattleBG", "BGEvBedroom.ftx"}, {}}},
	{"BGEvCaveEntrance", Job{{"BattleBG", "BGEvCaveEntrance.mbs"}, {"BattleBG", "BGEvCaveEntrance.ftx"}, {}}},
	{"BGEvCaveEntrance_C_01", Job{{"BattleBG", "BGEvCaveEntrance_C_01.mbs"}, {"BattleBG", "BGEvCaveEntrance_C_01.ftx"}, {}}},
	{"BGEvChurch_01", Job{{"BattleBG", "BGEvChurch_01.mbs"}, {"BattleBG", "BGEvChurch_01.ftx"}, {}}},
	{"BGEvChurch_B_01", Job{{"BattleBG", "BGEvChurch_B_01.mbs"}, {"BattleBG", "BGEvChurch_B_01.ftx"}, {}}},
	{"BGEvChurch_D_01", Job{{"BattleBG", "BGEvChurch_D_01.mbs"}, {"BattleBG", "BGEvChurch_D_01.ftx"}, {}}},
	{"BGEvCorridor", Job{{"BattleBG", "BGEvCorridor.mbs"}, {"BattleBG", "BGEvCorridor.ftx"}, {}}},
	{"BGEvCourtyard_C", Job{{"BattleBG", "BGEvCourtyard_C.mbs"}, {"BattleBG", "BGEvCourtyard_C.ftx"}, {}}},
	{"BGEvDarkElf_Room_01", Job{{"BattleBG", "BGEvDarkElf_Room_01.mbs"}, {"BattleBG", "BGEvDarkElf_Room_01.ftx"}, {}}},
	{"BGEvDungeon", Job{{"BattleBG", "BGEvDungeon.mbs"}, {"BattleBG", "BGEvDungeon.ftx"}, {}}},
	{"BGEvElfTree_a", Job{{"BattleBG", "BGEvElfTree_a.mbs"}, {"BattleBG", "BGEvElfTree_a.ftx"}, {}}},
	{"BGEvEnding_01", Job{{"BattleBG", "BGEvEnding_01.mbs"}, {"BattleBG", "BGEvEnding_01.ftx"}, {}}},
	{"BGEvEnding_02", Job{{"BattleBG", "BGEvEnding_02.mbs"}, {"BattleBG", "BGEvEnding_02.ftx"}, {}}},
	{"BGEvFarmer_A", Job{{"BattleBG", "BGEvFarmer_A.mbs"}, {"BattleBG", "BGEvFarmer_A.ftx"}, {}}},
	{"BGEvFarmer_A_Capital", Job{{"BattleBG", "BGEvFarmer_A_Capital.mbs"}, {"BattleBG", "BGEvFarmer_A_Capital.ftx"}, {}}},
	{"BGEvFarmer_B", Job{{"BattleBG", "BGEvFarmer_B.mbs"}, {"BattleBG", "BGEvFarmer_B.ftx"}, {}}},
	{"BGEvFarmer_C", Job{{"BattleBG", "BGEvFarmer_C.mbs"}, {"BattleBG", "BGEvFarmer_C.ftx"}, {}}},
	{"BGEvFarmer_D_b", Job{{"BattleBG", "BGEvFarmer_D_b.mbs"}, {"BattleBG", "BGEvFarmer_D_b.ftx"}, {}}},
	{"BGEvFarmer_E", Job{{"BattleBG", "BGEvFarmer_E.mbs"}, {"BattleBG", "BGEvFarmer_E.ftx"}, {}}},
	{"BGEvForestEffect", Job{{"BattleBG", "BGEvForestEffect.mbs"}, {"BattleBG", "BGEvForestEffect.ftx"}, {}}},
	{"BGEvForestFlowerField", Job{{"BattleBG", "BGEvForestFlowerField.mbs"}, {"BattleBG", "BGEvForestFlowerField.ftx"}, {}}},
	{"BGEvForest_a", Job{{"BattleBG", "BGEvForest_a.mbs"}, {"BattleBG", "BGEvForest_a.ftx"}, {}}},
	{"BGEvForest_b", Job{{"BattleBG", "BGEvForest_b.mbs"}, {"BattleBG", "BGEvForest_b.ftx"}, {}}},
	{"BGEvFortGate_A", Job{{"BattleBG", "BGEvFortGate_A.mbs"}, {"BattleBG", "BGEvFortGate_A.ftx"}, {}}},
	{"BGEvFortGate_D_01", Job{{"BattleBG", "BGEvFortGate_D_01.mbs"}, {"BattleBG", "BGEvFortGate_D_01.ftx"}, {}}},
	{"BGEvFort_a", Job{{"BattleBG", "BGEvFort_a.mbs"}, {"BattleBG", "BGEvFort_a.ftx"}, {}}},
	{"BGEvHeadquarters_B_01", Job{{"BattleBG", "BGEvHeadquarters_B_01.mbs"}, {"BattleBG", "BGEvHeadquarters_B_01.ftx"}, {}}},
	{"BGEvHeadquarters_a", Job{{"BattleBG", "BGEvHeadquarters_a.mbs"}, {"BattleBG", "BGEvHeadquarters_a.ftx"}, {}}},
	{"BGEvHotSpring_B", Job{{"BattleBG", "BGEvHotSpring_B.mbs"}, {"BattleBG", "BGEvHotSpring_B.ftx"}, {}}},
	{"BGEvHotSpring_D", Job{{"BattleBG", "BGEvHotSpring_D.mbs"}, {"BattleBG", "BGEvHotSpring_D.ftx"}, {}}},
	{"BGEvHouse_C", Job{{"BattleBG", "BGEvHouse_C.mbs"}, {"BattleBG", "BGEvHouse_C.ftx"}, {}}},
	{"BGEvJousting_a", Job{{"BattleBG", "BGEvJousting_a.mbs"}, {"BattleBG", "BGEvJousting_a.ftx"}, {}}},
	{"BGEvRitual_01", Job{{"BattleBG", "BGEvRitual_01.mbs"}, {"BattleBG", "BGEvRitual_01.ftx"}, {}}},
	{"BGEvRoad_j", Job{{"BattleBG", "BGEvRoad_j.mbs"}, {"BattleBG", "BGEvRoad_j.ftx"}, {}}},
	{"BGEvShipDeck", Job{{"BattleBG", "BGEvShipDeck.mbs"}, {"BattleBG", "BGEvShipDeck.ftx"}, {}}},
	{"BGEvTent", Job{{"BattleBG", "BGEvTent.mbs"}, {"BattleBG", "BGEvTent.ftx"}, {}}},
	{"BGEvThrone_C", Job{{"BattleBG", "BGEvThrone_C.mbs"}, {"BattleBG", "BGEvThrone_C.ftx"}, {}}},
	{"BGEvThrone_D", Job{{"BattleBG", "BGEvThrone_D.mbs"}, {"BattleBG", "BGEvThrone_D.ftx"}, {}}},
	{"BGEvThrone_E", Job{{"BattleBG", "BGEvThrone_E.mbs"}, {"BattleBG", "BGEvThrone_E.ftx"}, {}}},
	{"BGEvVikingSword_01", Job{{"BattleBG", "BGEvVikingSword_01.mbs"}, {"BattleBG", "BGEvVikingSword_01.ftx"}, {}}},
	{"BGEvWaitingRoom", Job{{"BattleBG", "BGEvWaitingRoom.mbs"}, {"BattleBG", "BGEvWaitingRoom.ftx"}, {}}},
	{"BGEvWitchHouse", Job{{"BattleBG", "BGEvWitchHouse.mbs"}, {"BattleBG", "BGEvWitchHouse.ftx"}, {}}},
	{"BGEvWoodHeadquarters", Job{{"BattleBG", "BGEvWoodHeadquarters.mbs"}, {"BattleBG", "BGEvWoodHeadquarters.ftx"}, {}}},
	{"BGEvWoodPassage", Job{{"BattleBG", "BGEvWoodPassage.mbs"}, {"BattleBG", "BGEvWoodPassage.ftx"}, {}}},
	{"BGEvthrone_C_long", Job{{"BattleBG", "BGEvthrone_C_long.mbs"}, {"BattleBG", "BGEvthrone_C_long.ftx"}, {}}},
	{"BGOpening010", Job{{"BattleBG", "BGOpening010.mbs"}, {"BattleBG", "BGOpening010.ftx"}, {}}},
	{"BGOpening011", Job{{"BattleBG", "BGOpening011.mbs"}, {"BattleBG", "BGOpening011.ftx"}, {}}},
	{"BGOpening090", Job{{"BattleBG", "BGOpening090.mbs"}, {"BattleBG", "BGOpening090.ftx"}, {}}},
	{"BGTest", Job{{"BattleBG", "BGTest.mbs"}, {"BattleBG", "BGTest.ftx"}, {}}},
	{"Classpedia_BG", Job{{"BattleBG", "Classpedia_BG.mbs"}, {"BattleBG", "Classpedia_BG.ftx"}, {}}},
	{"komorebi00", Job{{"BattleBG", "komorebi00.mbs"}, {"BattleBG", "komorebi00.ftx"}, {}}},
};
// clang-format on