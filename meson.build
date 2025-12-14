project('pipemixer', 'c',
  version: '0.3.1',
  license: 'GPL-3.0-or-later',
  default_options: ['warning_level=3'])

add_project_arguments('-Wno-pedantic', language: 'c')
add_project_arguments('-Wno-unused-value', language: 'c')
add_project_arguments('-Wno-unused-parameter', language: 'c')

git = find_program('git', required: false)
if git.found()
  git_tag = run_command(git, 'describe', '--tags', check: false).stdout().strip()
  if git_tag != ''
    add_project_arguments('-DPIPEMIXER_GIT_TAG="@0@"'.format(git_tag), language: 'c')
  endif
  git_branch = run_command(git, 'branch', '--show-current', check: false).stdout().strip()
  if git_branch != ''
    add_project_arguments('-DPIPEMIXER_GIT_BRANCH="@0@"'.format(git_branch), language: 'c')
  endif
endif
add_project_arguments('-DPIPEMIXER_VERSION="@0@"'.format(meson.project_version()), language: 'c')

pipewire_dep = dependency('libpipewire-0.3')
ncursesw_dep = dependency('ncursesw')
inih_dep = dependency('inih')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m')

pipemixer_sources = [
  'src/pipemixer.c',
  'src/tui.c',
  'src/menu.c',
  'src/log.c',
  'src/xmalloc.c',
  'src/utils.c',
  'src/filter.c',
  'src/config.c',
  'src/signals.c',
  'src/eventloop.c',
  'src/collections/vec.c',
  'src/collections/map.c',
  'src/pw/common.c',
  'src/pw/device.c',
  'src/pw/node.c',
  'src/pw/roundtrip.c',
  'src/pw/events.c',
  'src/pw/default.c',
]

include_dirs = [
  'src'
]

executable('pipemixer', pipemixer_sources,
  include_directories: include_dirs,
  dependencies: [ncursesw_dep, pipewire_dep, m_dep, inih_dep],
  install: true)

install_data(
  'com.github.pipemixer.desktop',
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

install_data(
  'man/pipemixer.1',
  install_dir: join_paths(get_option('mandir'), 'man1'),
)

install_data(
  'man/pipemixer.ini.5',
  install_dir: join_paths(get_option('mandir'), 'man5'),
)

if get_option('shell-completions')
  zsh_install_dir = join_paths(get_option('datadir'), 'zsh', 'site-functions')
  install_data('completion/_pipemixer', install_dir: zsh_install_dir)
endif

