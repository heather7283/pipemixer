project('pipemixer', 'c',
  version: '0.1.0',
  license: 'GPL-3.0-or-later',
  default_options: ['warning_level=3'])

add_project_arguments('-Wno-pedantic', language: 'c')
add_project_arguments('-Wno-unused-value', language: 'c')
add_project_arguments('-Wno-unused-parameter', language: 'c')

pipewire_dep = dependency('libpipewire-0.3')
ncursesw_dep = dependency('ncursesw')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m')

executable('pipemixer',
  'src/pipemixer.c',
  'src/tui.c',
  'src/log.c',
  'src/xmalloc.c',
  'src/pw.c',
  'src/utils.c',
  'src/stb_ds_impl.c',
  'src/event_loop_impl.c',
  include_directories: ['thirdparty'],
  dependencies: [ncursesw_dep, pipewire_dep, m_dep],
  install: true)

