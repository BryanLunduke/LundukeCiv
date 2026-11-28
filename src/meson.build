# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Bryan Lunduke
civart = static_library(
  'civart',
  files('art/sprites.cpp', 'art/terrain_paint.cpp', 'art/wonders.cpp', 'art/space.cpp', 'art/leaders.cpp', 'art/events.cpp'),
  include_directories: [civcore_inc, include_directories('art')],
  dependencies: [cairo_dep],
)

game_sources = files('main.cpp', 'ui/app.cpp', 'ui/dialogs.cpp', 'ui/draw.cpp', 'ui/pad.cpp',
                     'ui/tree_layout.cpp', 'ui/tree_view.cpp', 'ui/tech_tree.cpp')
game_link_args = []
if host_machine.system() == 'windows'
  game_link_args += ['-mwindows']
endif

executable(
  'lunduke-civ',
  game_sources,
  include_directories: [civcore_inc, include_directories('art', 'ui')],
  link_with: [civart],
  dependencies: [civcore_dep, sdl3_dep, cairo_dep, pangocairo_dep],
  cpp_args: ['-DLUNDUKE_CIV_VERSION="' + meson.project_version() + '"'],
  link_args: game_link_args,
  install: true,
)
