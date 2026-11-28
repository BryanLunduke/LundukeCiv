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
             'treaties', 'senate', 'tribute', 'talks', 'ai_peace', 'civil_war', 'tribes', 'worlds', 'databank', 'musketeers']
  test(t, m4_tests, args: [t], timeout: 120, suite: 'm4')
endforeach
m5_tests = executable('m5_tests', files('m5_tests.cpp'), dependencies: civcore_dep)
foreach t : ['score', 'end_2100', 'hall_of_fame', 'save_v6', 'huts', 'diplomats', 'science_carryover', 'ai_specialists',
    'ship_boarding', 'ai_overseas_settling', 'ai_invasion', 'ai_naval_attack', 'ai_air_strike', 'ai_navy_build',
    'events', 'options', 'find_city']
  test(t, m5_tests, args: [t], timeout: 120, suite: 'm5')
endforeach
qa_tests = executable('qa_tests', files('qa_tests.cpp'), dependencies: civcore_dep)
foreach t : ['ship_capture', 'barbarian_restraint', 'hut_barbarians_gentle', 'civil_war_stacks', 'ai_save_roundtrip',
    'ai_space_part_balance', 'ai_government', 'ai_rates_pay_upkeep', 'ai_army_upkeep', 'ai_retakes_barbarian_city']
  test(t, qa_tests, args: [t], timeout: 120, suite: 'qa')
endforeach

techtree_tests = executable('techtree_tests',
  files('techtree_tests.cpp', '../src/ui/tree_layout.cpp'),
  include_directories: [civcore_inc, include_directories('../src/ui')],
  dependencies: civcore_dep)
foreach t : ['layout_from_data', 'add_remove_advance', 'cycle_safety', 'state_computation']
  test(t, techtree_tests, args: [t], timeout: 60, suite: 'techtree')
endforeach
