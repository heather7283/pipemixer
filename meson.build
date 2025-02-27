project('pipemixer', 'c',
  version : '0.1.0',
  default_options : ['warning_level=3'])

# stupid gnuware (pipewire)
add_project_arguments('-Wno-gnu-statement-expression-from-macro-expansion', language: 'c')
add_project_arguments('-Wno-gnu-zero-variadic-macro-arguments', language: 'c')
add_project_arguments('-Wno-gnu-case-range', language: 'c')

# needed for unicode support in curses
add_project_arguments('-DNCURSES_WIDECHAR', language: 'c')

pipewire_dep = dependency('libpipewire-0.3')
ncursesw_dep = dependency('ncursesw')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m')


executable('pipemixer',
  'src/pipemixer.c',
  'src/tui.c',
  'src/log.c',
  'src/event_loop.c',
  'src/xmalloc.c',
  'src/pw.c',
  'src/param_print.c',
  'src/props.c',
  dependencies: [ncursesw_dep, pipewire_dep, m_dep],
  install: true)

executable('pwtest',
  'src/pwtest.c',
  'src/log.c',
  'src/pw.c',
  'src/param_print.c',
  'src/props.c',
  'src/event_loop.c',
  'src/xmalloc.c',
  dependencies: [pipewire_dep, m_dep],
  install: false)

