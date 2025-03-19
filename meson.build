project('pipemixer', 'c',
  version : '0.1.0',
  default_options : ['warning_level=3'])

# stupid gnuware (pipewire)
add_project_arguments('-Wno-gnu-statement-expression-from-macro-expansion', language: 'c')
add_project_arguments('-Wno-gnu-zero-variadic-macro-arguments', language: 'c')
add_project_arguments('-Wno-gnu-case-range', language: 'c')

add_project_arguments('-Wno-unused-parameter', language: 'c')

pipewire_dep = dependency('libpipewire-0.3')
ncursesw_dep = dependency('ncursesw')

# needed for unicode support in curses + wcwidth()
add_project_arguments('-D_XOPEN_SOURCE=700', language: 'c')
add_project_arguments('-Wno-macro-redefined', language: 'c')

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

