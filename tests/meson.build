# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Bryan Lunduke
rules_tests = executable('rules_tests', files('rules_tests.cpp'), dependencies: civcore_dep)
foreach t : ['calendar', 'terrain_yields', 'tech_prereqs', 'units_table', 'combat_odds', 'city_growth',
             'settlers_work', 'movement', 'save_roundtrip', 'ai_smoke', 'wonders', 'envoys',
             'railroads', 'no_pollution', 'no_recycling', 'specialists', 'corruption', 'research_cost', 'barbarians', 'space_race', 'pad_nav']
  test(t, rules_tests, args: [t], timeout: 120)
endforeach
m4_tests = executable('m4_tests', files('m4_tests.cpp'), dependencies: civcore_dep)
foreach t : ['empire_size', 'celebration', 'democracy_collapse', 'anarchy_length', 'buy_cost', 'sell', 'barracks',
             'capture', 'nuke', 'trireme', 'air_targets', 'orders', 'disasters',
             'treaties', 'senate', 'tribute', 'talks', 'ai_peace', 'civil_war', 'tribes']
  test(t, m4_tests, args: [t], timeout: 120, suite: 'm4')
endforeach
