# Lunduke Civ

Lunduke Civ is a turn-based game of building an empire from a single settler in 4000 BC. It plays by the rules of the 1991 DOS *Civilization*: the same terrain table, the same 28 units with their attack, defense, movement, cost, required advance and obsolescence, the same advance tree (67 advances plus Future Tech), the same governments and city growth. Everything is re-implemented; every picture is original pixel art drawn by code, in the look of Lunduke City 2.

The default window is **1280×800** (Steam Deck native) and is resizable down to 1024×700.

## Build on LCOS / Debian 13

```sh
sudo apt install build-essential meson ninja-build pkg-config \
  libsdl3-dev libcairo2-dev libpango1.0-dev fonts-dejavu-core
meson setup build
meson compile -C build
meson test -C build
./build/src/lunduke-civ
```

SDL 3.2 or newer is required. `meson setup build -Dgame=false` builds only the rules library and the tests (no SDL, Cairo or Pango needed).

`lunduke-civ path/to/game.lciv` starts with a saved game loaded. Saves go to `~/.local/share/Lunduke/Lunduke Civ/` (six slots, `slotN.lciv`, plain text).

## Windows

From Debian or Ubuntu, one script cross-builds the x86_64 exe, runs the tests under Wine and writes `build-windows/dist/lunduke-civ-windows-x86_64.zip`:

```sh
sudo apt install g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix \
  binutils-mingw-w64-x86-64 meson ninja-build pkg-config python3 \
  gnupg zstd zip wine wine64
packaging/windows/build-windows.sh
```

The script builds a MinGW sysroot from signed MSYS2 packages (SDL3, Cairo, Pango, DejaVu). Set `SYSROOT=/path/to/sysroot` to reuse one that is already built, and `LUNDUKE_WIN_TESTS=0` to skip the Wine test run.

## Playing

The top band shows the empire: civilization, year, treasury, tax · luxury · science rates, population, government and research. Click any of them for the matching advisor. The left sidebar has the world map (click to jump), the active unit with the yields of its square, its orders, and Next Unit / End Turn.

| Key | Action |
|---|---|
| Arrows, keypad | Move the active unit (keypad 7 9 1 3, Home, PgUp, End, PgDn for diagonals) |
| B | Found a city (or join one) |
| R I M P | Settlers: road, irrigate, mine, fortress |
| F S | Fortify, sentry |
| G | Go to a square (then click it) |
| W Space N | Wait, skip this turn, next unit |
| Shift+D | Disband |
| Enter | End the turn |
| C H | Center on the unit, on the capital |
| + − wheel | Zoom 100–300% |
| T A V | Tax rates, science, city list |
| Ctrl+S Ctrl+L Ctrl+N | Save, load, new game |
| Click | Select a unit, open a city, move next door |
| Right click | Center the map there |

Clicking a city opens its screen: citizens, food/shield/trade, food box, supported units, the 21-square city radius (click a square to move a worker), production with Change and Buy, and the improvements already built. Citizens taken off the land become specialists, shown after the workers: click one, in a city of size 5 or more, to turn an entertainer (+2 luxuries) into a taxman (+2 gold), a taxman into a scientist (+2 bulbs) and a scientist back into an entertainer, as in the DOS game. Specialists count before the Marketplace, Bank, Library and University multiply the city's output; under Anarchy scientists find nothing. Smaller cities, and cities placed automatically, only keep entertainers.

## Rules

From the DOS game: 80×50 wrapping world; terrain yields with roads, irrigation, mines, rivers and specials; Despotism −1 on squares producing more than 2; movement in thirds with roads and zones of control; round-by-round combat (each unit has 10 hit points; every round the attacker wins with chance A/(A+D) and the loser loses a hit point; the odds shown are the exact chance to win the whole fight) with terrain, fortify, fortress, City Walls and veteran bonuses and whole-stack loss outside cities; wounded units heal at the start of their turn (faster fortified, fully in a city); veteran promotion; huts; growth with a (size+1)×10 food box, the Granary, the size 10 limit without an Aqueduct; settlers costing a citizen; unit shield support and settler food; tax/luxury/science splits with government limits; Marketplace, Library, Bank, University, Factory; building upkeep; happiness with Temples, Colosseums, Cathedrals and luxuries, disorder; Anarchy during revolutions; capturing and plundering cities; conquest victory.

All 21 Wonders of the World from the DOS game, each built once in the world, with their effects and the advances that make them obsolete (once their owner learns them): Colossus, Pyramids, Great Wall, Hanging Gardens, Lighthouse, Great Library, Oracle, Copernicus' Observatory, Michelangelo's Chapel, Shakespeare's Theatre, J.S. Bach's Cathedral, Magellan's Expedition, Isaac Newton's College, Darwin's Voyage, Women's Suffrage, Hoover Dam, Manhattan Project, United Nations, Apollo Program, SETI Program and Cure for Cancer. Starting and completing one is announced to everyone; the Wonders advisor (O) shows who holds each.

Diplomats moving into a foreign city (or onto a lone foreign unit) can establish an embassy, investigate the city, steal an advance (once per city), sabotage production or a building, incite a revolt (not in a capital) or bribe the unit; the hostile acts against a people at peace mean war, and the Diplomat is spent. Caravans entering a city (or pressing B in one) can add 50 shields to a Wonder under way or, 9 or more squares from home, open a trade route (up to 3 per city): a one-time gold and science bonus plus trade every turn, more with foreign and overseas partners. Embassies show a rival's treasury, advances and research in Foreign Affairs. Under a Republic or Democracy, troops away from home make citizens unhappy (one or two each), which Women's Suffrage reduces.

Barbarians appear from turn 50: landings from the sea next to a coastal city (raiders, their ship and a Barbarian Leader) or uprisings in empty country, with armies that grow with the age of the world. They never make peace and do not count toward victory. Their ships sail off once the raiders are ashore; the raiders march on the nearest city, burn a town of size 1 or 2 and keep a larger one (a captured city holds only a few troops). Walking onto a Barbarian Leader left alone pays 100 gold ransom. With Railroad known, Settlers lay railroad on a road (R): moving along it costs nothing and the square gives +50% shields; city squares get railroad automatically.

Unlike the DOS game there is no pollution and no global warming: industry never dirties the land and the climate never changes. Mass Transit and the Recycling Center, whose only job was to cut pollution, are left out (Recycling is still an advance to research, with nothing to build). Power, Hydro and Nuclear Plants and the Hoover Dam keep their DOS effect of +50% shields with a Factory. Older saves still load; their polluted squares and warming are dropped, and a city that was building Mass Transit or a Recycling Center switches to a defender.

The space race opens once anyone completes the Apollo Program. Cities then build spaceship parts like improvements: SS Structurals (Space Flight, 80 shields, up to 39), SS Components (Plastics, 160, up to 16, fuel and propulsion in turn) and SS Modules (Superconductor, 320, up to 12, habitation, life support and solar panels in turn). Every component and module needs a structural to hold it. The Spaceship screen (Y) shows the ship on its blueprint with colonists, life support, energy, mass, flight time and chance of success, and launches it from the capital (L). The ship flies for 4 + 0.6 x mass units / engine pairs years; if it arrives (by its chance of success) its owner wins. If the capital falls, the ship is lost, on the pad or in flight. Computer players build a modest complete ship and launch it, sooner if a rival is already on the way.

Controllers work as in Lunduke City 2, made for the Steam Deck at 1280x800. The D-pad or left stick moves a tile cursor; A moves or attacks next door, opens the unit's orders on the unit itself, opens one of your cities, or sends the unit further away; B backs out (on the map: back to the unit, then wait); X opens the orders menu; Y founds a city with Settlers or fortifies troops; L1/R1 cycle units; View ends the turn; the Menu button opens the menus. In menus and dialogs the D-pad moves a gold focus between buttons, rows and city squares, A chooses and B closes. The triggers zoom, the right stick scrolls the map and the stick clicks center the view. The bottom bar shows the buttons as key caps, named for the pad in hand (Deck, Xbox or PlayStation); on a Deck (SteamDeck=1) the hints stay on even when the touchpad or keyboard is used. F1 lists both keyboard and controller bindings.

Approximated or not yet: corruption and research cost use simple formulas close to the DOS numbers. Combat is fought in rounds with hit points (10 per unit) rather than the DOS single roll. A Wonder goes obsolete when its owner learns the obsoleting advance. Diplomats are always used up, and trade route income is an approximation. Barbarian cities only raise a few troops, and raiders burn towns of size 1 or 2 instead of holding them. The spaceship's flight time and success chance use simplified formulas, and the spaceship screen lists rival ships even without an embassy.

The computer players research toward Monarchy, Currency, Writing and Trade first and expand early. They keep settlers irrigating and mining around their cities and build Temples, Granaries, Libraries, Marketplaces, Aqueducts and the like before Wonders, caravans or more troops. They garrison cities and rush-buy a defender when enemies come close. They attack enemy and barbarian cities on their own continent, buy, declare war and make peace, and build and launch a modest spaceship. They do not use ships or aircraft yet. Civ-style, computer rulers get a level handicap: their food boxes and build costs are 110% on Chieftain, 90% Warlord, 75% Prince, 65% King and 55% Emperor.

## Layout

| Path | What |
|---|---|
| `src/core` | Rules tables, map generator, game state and turn loop, AI, save format (no SDL) |
| `src/art` | Original pixel art: terrain painter, rivers, cities, unit counters, icons |
| `src/ui` | The Lunduke City 2-style interface: band, sidebar, map, dialogs |
| `src/main.cpp` | SDL3 window, input, Cairo surface → texture |
| `tests` | `meson test`: calendar, yields, tech tree, unit table, combat, growth, settlers, movement, save round trip, AI smoke game, wonders, envoys, railroads, no pollution, specialists, barbarians, space race, controller navigation |
| `packaging/windows`, `cross` | Windows cross build |

## License

GPL-3.0-or-later. See `COPYING` and `NOTICE`.
