# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Bryan Lunduke
project(
  'lunduke-civ',
  'cpp',
  version: '0.1.0',
  default_options: ['cpp_std=c++17', 'warning_level=2', 'buildtype=debugoptimized'],
  meson_version: '>= 0.60',
)

civcore_inc = include_directories('src/core')
civcore = static_library(
  'civcore',
  files(
    'src/core/rules.cpp',
    'src/core/map.cpp',
    'src/core/game.cpp',
    'src/core/ai.cpp',
    'src/core/save.cpp',
    'src/core/tribes.cpp',
    'src/core/diplomacy.cpp',
    'src/core/earth.cpp',
  ),
  include_directories: civcore_inc,
)
civcore_dep = declare_dependency(link_with: civcore, include_directories: civcore_inc)

if get_option('game')
  sdl3_dep = dependency('sdl3', version: '>= 3.2.0')
  cairo_dep = dependency('cairo')
  pangocairo_dep = dependency('pangocairo')
  subdir('src')
endif

subdir('tests')

if not meson.is_cross_build()
  install_data('data/lunduke-civ.desktop', install_dir: get_option('datadir') / 'applications')
endif
