// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.

#include "tribes.hpp"

namespace civ {

namespace {

// Colours: 0 blue, 1 red, 2 yellow, 3 green, 4 teal, 5 purple, 6 orange, 7 white.
// Red is left for a people of the player's own making.
const TribeInfo kTribes[kTribeCount] = {
    {"Romans", "Roman", "Caesar", 0, 7, 0, 1, 1,
     {"Roma", "Ostia", "Neapolis", "Capua", "Mediolanum", "Verona", "Aquileia", "Ravenna", "Genua", "Tarentum",
      "Lugdunum", "Londinium", "Corduba", "Massilia", "Treverorum", "Salona"}},
    {"Babylonians", "Babylonian", "Hammurabi", 1, 3, -1, -1, 1,
     {"Babylon", "Borsippa", "Kutha", "Dilbat", "Marad", "Kish", "Sippar", "Nippur", "Isin", "Larsa",
      "Uruk", "Ur", "Eridu", "Der", "Mari", "Eshnunna"}},
    {"Germans", "German", "Frederick", 2, 0, 1, -1, 1,
     {"Berlin", "Potsdam", "Hamburg", "Bremen", "Lubeck", "Dresden", "Leipzig", "Magdeburg", "Cologne", "Aachen",
      "Mainz", "Frankfurt", "Nuremberg", "Munich", "Augsburg", "Stuttgart"}},
    {"Egyptians", "Egyptian", "Ramesses", 3, 2, 0, 0, 1,
     {"Thebes", "Memphis", "Pi-Ramesses", "Abydos", "Edfu", "Elephantine", "Heliopolis", "Bubastis", "Tanis", "Sais",
      "Hermopolis", "Dendera", "Kom Ombo", "Avaris", "Buto", "Coptos"}},
    {"Americans", "American", "Lincoln", 4, 4, -1, 0, 1,
     {"Washington", "Philadelphia", "Boston", "Baltimore", "Richmond", "Charleston", "Savannah", "Albany", "Pittsburgh",
      "Cincinnati", "St. Louis", "Chicago", "Springfield", "New Orleans", "Denver", "San Francisco"}},
    {"Greeks", "Greek", "Alexander", 5, 5, 0, 1, -1,
     {"Pella", "Athens", "Sparta", "Corinth", "Thebes", "Argos", "Delphi", "Olynthus", "Amphipolis", "Ephesus",
      "Miletus", "Halicarnassus", "Rhodes", "Alexandria", "Antioch", "Seleucia"}},
    {"Indians", "Indian", "Gandhi", 6, 6, -1, -1, 0,
     {"Delhi", "Agra", "Varanasi", "Patna", "Kolkata", "Mumbai", "Chennai", "Hyderabad", "Bangalore", "Ahmedabad",
      "Lucknow", "Jaipur", "Madurai", "Pune", "Mysore", "Kanpur"}},
    {"Russians", "Russian", "Stalin", 0, 7, 1, 0, -1,
     {"Moscow", "Vladimir", "Novgorod", "Kiev", "Smolensk", "Tver", "Ryazan", "Kazan", "Nizhny", "Yaroslavl",
      "Pskov", "Tula", "Saratov", "Perm", "Omsk", "Irkutsk"}},
    {"Zulus", "Zulu", "Shaka", 1, 3, 1, 0, 0,
     {"Zimbabwe", "Ulundi", "Bulawayo", "Gibixhegu", "Dukuza", "Nobamba", "Mgungundlovu", "Kwadukuza", "Eshowe",
      "Nongoma", "Mahlabatini", "Nkandla", "Babanango", "Melmoth", "Vryheid", "Ondini"}},
    {"French", "French", "Napoleon", 2, 0, 1, 1, 1,
     {"Paris", "Orleans", "Lyon", "Marseille", "Bordeaux", "Toulouse", "Rouen", "Reims", "Tours", "Nantes",
      "Dijon", "Lille", "Strasbourg", "Avignon", "Grenoble", "Brest"}},
    {"Aztecs", "Aztec", "Montezuma", 3, 2, 0, -1, 1,
     {"Tenochtitlan", "Texcoco", "Tlacopan", "Tlatelolco", "Cholula", "Xochimilco", "Coyoacan", "Chalco", "Tula",
      "Teotihuacan", "Cuernavaca", "Malinalco", "Tepeyac", "Iztapalapa", "Azcapotzalco", "Culhuacan"}},
    {"Chinese", "Chinese", "Mao Zedong", 4, 4, 0, 0, 1,
     {"Beijing", "Xi'an", "Luoyang", "Nanjing", "Kaifeng", "Hangzhou", "Guangzhou", "Chengdu", "Wuhan", "Suzhou",
      "Shanghai", "Tianjin", "Changsha", "Kunming", "Fuzhou", "Shenyang"}},
    {"English", "English", "Elizabeth I", 5, 5, 0, 1, 0,
     {"London", "York", "Winchester", "Canterbury", "Bristol", "Norwich", "Oxford", "Cambridge", "Exeter", "Lincoln",
      "Coventry", "Chester", "Durham", "Plymouth", "Leeds", "Dover"}},
    {"Mongols", "Mongol", "Genghis Khan", 6, 6, 1, 1, -1,
     {"Karakorum", "Khanbaliq", "Sarai", "Otrar", "Almaliq", "Kashgar", "Bukhara", "Samarkand", "Merv", "Herat",
      "Tabriz", "Urgench", "Turfan", "Kharkhorin", "Ulaanbaatar", "Hovd"}},
};

} // namespace

const TribeInfo &tribe_info(int t) { return kTribes[((t % kTribeCount) + kTribeCount) % kTribeCount]; }
int tribe_pair(int t) { return (t + 7) % kTribeCount; }
const char *mood_name(int m) { return m < 0 ? "Friendly" : m > 0 ? "Aggressive" : "Neutral"; }
const char *policy_name(int p) { return p < 0 ? "Perfectionist" : p > 0 ? "Expansionist" : "Balanced"; }
const char *ideology_name(int i) { return i < 0 ? "Militaristic" : i > 0 ? "Civilized" : "Balanced"; }

} // namespace civ
