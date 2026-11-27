// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Bryan Lunduke
// See COPYING and NOTICE.
//
// A coarse Earth for the 80x50 map: our own outlines of the continents in
// longitude and latitude (a few dozen points each, drawn for this game),
// filled onto the grid, plus boxes for the great deserts, rain forests and
// mountain ranges that steer the terrain.

#include "map.hpp"

#include <cmath>

namespace civ {

namespace {

struct Pt {
    double lon, lat;
};

// Each outline runs round its coast; the fill is even-odd.
const Pt kNorthAmerica[] = {
    {-168, 66}, {-160, 71}, {-140, 70}, {-125, 70}, {-110, 68}, {-95, 72}, {-82, 73}, {-75, 68}, {-64, 60},
    {-56, 53},  {-60, 47},  {-66, 44},  {-70, 42},  {-76, 37},  {-76, 34}, {-81, 31}, {-80, 25}, {-83, 29},
    {-90, 30},  {-97, 27},  {-97, 21},  {-91, 19},  {-87, 21},  {-88, 16}, {-83, 10}, {-78, 8},  {-80, 7},
    {-86, 11},  {-92, 14},  {-105, 20}, {-110, 24}, {-112, 30}, {-117, 32}, {-122, 37}, {-124, 42}, {-124, 48},
    {-131, 55}, {-140, 59}, {-152, 58}, {-158, 56}, {-164, 59}, {-166, 62},
};
const Pt kGreenland[] = {
    {-52, 61}, {-43, 60}, {-36, 66}, {-22, 70}, {-18, 77}, {-25, 82}, {-45, 83}, {-62, 81}, {-70, 77}, {-58, 75}, {-54, 69},
};
const Pt kSouthAmerica[] = {
    {-78, 8},   {-72, 12},  {-62, 11},  {-52, 4},   {-50, 0},   {-44, -2},  {-35, -6},  {-38, -13}, {-41, -22},
    {-48, -26}, {-53, -34}, {-58, -38}, {-64, -41}, {-66, -47}, {-69, -52}, {-74, -53}, {-74, -45}, {-73, -37},
    {-71, -28}, {-70, -18}, {-76, -14}, {-81, -6},  {-80, -1},  {-78, 2},
};
const Pt kEurasia[] = {
    {-9, 37},   {-9, 43},   {-2, 44},   {-4, 48},   {2, 50},    {8, 54},    {8, 57},    {5, 59},    {5, 62},
    {12, 66},   {18, 70},   {28, 71},   {40, 67},   {45, 68},   {60, 69},   {70, 73},   {80, 74},   {95, 77},
    {110, 77},  {130, 72},  {150, 71},  {170, 70},  {180, 68},  {180, 64},  {165, 60},  {162, 56},  {156, 51},
    {155, 58},  {142, 59},  {140, 53},  {141, 48},  {135, 43},  {129, 41},  {129, 35},  {126, 35},  {125, 39},
    {121, 39},  {122, 32},  {121, 28},  {117, 23},  {110, 21},  {108, 17},  {109, 12},  {105, 9},   {100, 13},
    {100, 7},   {103, 2},   {101, 2},   {98, 8},    {98, 16},   {94, 17},   {92, 22},   {87, 21},   {80, 15},
    {80, 10},   {77, 8},    {73, 16},   {72, 21},   {67, 24},   {58, 25},   {57, 27},   {51, 30},   {48, 29},
    {51, 25},   {56, 26},   {59, 22},   {52, 17},   {45, 13},   {43, 13},   {39, 20},   {35, 28},   {34, 31},
    {35, 36},   {30, 36},   {27, 37},   {26, 40},   {23, 38},   {22, 37},   {20, 40},   {19, 42},   {14, 45},
    {12, 44},   {16, 41},   {16, 38},   {12, 38},   {11, 42},   {9, 44},    {4, 43},    {0, 39},    {-2, 37},
};
const Pt kAfrica[] = {
    {-17, 21}, {-17, 14}, {-12, 7},  {-5, 5},   {5, 5},    {9, 4},    {10, -2},  {12, -6},  {13, -12}, {12, -18},
    {15, -27}, {18, -34}, {26, -34}, {32, -29}, {35, -22}, {40, -15}, {40, -5},  {43, 0},   {51, 11},  {44, 11},
    {39, 18},  {34, 28},  {32, 31},  {25, 32},  {20, 31},  {11, 33},  {10, 37},  {0, 36},   {-6, 36},  {-10, 30},
    {-13, 27},
};
const Pt kBritain[] = {{-6, 50}, {2, 51}, {1, 53}, {-2, 56}, {-2, 58}, {-6, 59}, {-6, 55}, {-4, 53}};
const Pt kIreland[] = {{-10, 51}, {-6, 52}, {-6, 55}, {-9, 55}};
const Pt kIceland[] = {{-24, 63}, {-14, 63}, {-13, 66}, {-22, 67}};
const Pt kJapan[] = {{129, 31}, {135, 33}, {140, 35}, {142, 40}, {143, 45}, {140, 43}, {138, 38}, {133, 36}, {129, 34}};
const Pt kMadagascar[] = {{44, -25}, {47, -26}, {50, -15}, {49, -12}, {44, -16}};
const Pt kAustralia[] = {
    {114, -22}, {115, -34}, {122, -34}, {131, -32}, {138, -35}, {141, -38}, {150, -38}, {153, -32},
    {153, -26}, {146, -19}, {142, -10}, {140, -17}, {136, -12}, {130, -12}, {126, -14}, {122, -18},
};
const Pt kBorneo[] = {{109, 2}, {110, -3}, {116, -4}, {119, 1}, {117, 7}};
const Pt kSumatra[] = {{95, 6}, {106, -6}, {104, -6}, {97, 1}};
const Pt kJava[] = {{105, -6}, {114, -7}, {114, -9}, {106, -8}};
const Pt kNewGuinea[] = {{131, -1}, {141, -2}, {150, -10}, {141, -9}, {134, -4}};
const Pt kNewZealand[] = {{172, -34}, {178, -38}, {175, -42}, {170, -46}, {166, -46}, {172, -41}};
const Pt kPhilippines[] = {{120, 18}, {122, 18}, {126, 7}, {122, 7}, {120, 13}};
const Pt kCuba[] = {{-85, 22}, {-80, 23}, {-74, 20}, {-78, 20}};
const Pt kSriLanka[] = {{80, 10}, {82, 7}, {80, 6}};

struct Shape {
    const Pt *p;
    int n;
};
#define SHAPE(a) {a, static_cast<int>(sizeof(a) / sizeof(a[0]))}
const Shape kShapes[] = {
    SHAPE(kNorthAmerica), SHAPE(kGreenland), SHAPE(kSouthAmerica), SHAPE(kEurasia),   SHAPE(kAfrica),
    SHAPE(kBritain),      SHAPE(kIreland),   SHAPE(kIceland),      SHAPE(kJapan),     SHAPE(kMadagascar),
    SHAPE(kAustralia),    SHAPE(kBorneo),    SHAPE(kSumatra),      SHAPE(kJava),      SHAPE(kNewGuinea),
    SHAPE(kNewZealand),   SHAPE(kPhilippines), SHAPE(kCuba),       SHAPE(kSriLanka),
};
#undef SHAPE

struct Box {
    double lon0, lat0, lon1, lat1;
    EarthZone zone;
};
const Box kBoxes[] = {
    {-15, 15, 32, 30, EarthZone::Desert},     // Sahara
    {36, 15, 56, 30, EarthZone::Desert},      // Arabia
    {120, -30, 140, -20, EarthZone::Desert},  // the Australian interior
    {90, 38, 112, 46, EarthZone::Desert},     // Gobi
    {-117, 28, -106, 36, EarthZone::Desert},  // the American south-west
    {-73, -30, -68, -18, EarthZone::Desert},  // Atacama
    {-75, -10, -48, 4, EarthZone::Jungle},    // Amazon
    {10, -5, 28, 5, EarthZone::Jungle},       // Congo
    {96, -8, 120, 20, EarthZone::Jungle},     // south-east Asia
    {-122, 35, -105, 60, EarthZone::Mountains},   // Rockies
    {-78, -45, -67, 5, EarthZone::Mountains},     // Andes
    {72, 27, 98, 36, EarthZone::Mountains},       // Himalaya
    {6, 44, 15, 48, EarthZone::Mountains},        // Alps
    {40, 38, 50, 44, EarthZone::Mountains},       // Caucasus
    {56, 50, 62, 66, EarthZone::Hills},           // Urals
    {33, -10, 40, 10, EarthZone::Hills},          // East African highlands
    {-62, 66, -35, 83, EarthZone::Ice},           // Greenland's ice
    {-4, 45, 40, 60, EarthZone::Temperate},       // European plains
    {-100, 30, -80, 48, EarthZone::Temperate},    // American prairie
};

bool inside(const Shape &s, double lon, double lat)
{
    bool in = false;
    for (int i = 0, j = s.n - 1; i < s.n; j = i++) {
        const Pt &a = s.p[i], &b = s.p[j];
        if ((a.lat > lat) != (b.lat > lat) && lon < (b.lon - a.lon) * (lat - a.lat) / (b.lat - a.lat) + a.lon) in = !in;
    }
    return in;
}

} // namespace

double earth_lon(int x) { return (x + 0.5) * 360.0 / kMapW - 180.0; }
double earth_lat(int y) { return 90.0 - (y + 0.5) * 180.0 / kMapH; }

bool earth_land(int x, int y)
{
    const double lon = earth_lon(x), lat = earth_lat(y);
    // Sample the square's middle and a little to each side, so thin
    // isthmuses and islands still show at this scale.
    const double dl = 360.0 / kMapW * 0.3, dt = 180.0 / kMapH * 0.3;
    int hits = 0;
    bool centre = false;
    const double probes[5][2] = {{0, 0}, {-dl, 0}, {dl, 0}, {0, -dt}, {0, dt}};
    for (int i = 0; i < 5; ++i) {
        for (const Shape &s : kShapes) {
            if (inside(s, lon + probes[i][0], lat + probes[i][1])) {
                ++hits;
                centre |= i == 0;
                break;
            }
        }
    }
    return hits >= 3 || (centre && hits >= 2);
}

EarthZone earth_zone(int x, int y)
{
    const double lon = earth_lon(x), lat = earth_lat(y);
    for (const Box &b : kBoxes) {
        if (lon >= b.lon0 && lon <= b.lon1 && lat >= b.lat0 && lat <= b.lat1) return b.zone;
    }
    return EarthZone::None;
}

} // namespace civ
