project('pipemixer', 'c',
  version: '0.4.3',
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

# would preferably use #embed for this but it's a C23 feature
default_config_source = custom_target(
  'default_config.c',
  input: 'assets/pipemixer.ini',
  output: 'default_config.c',
  feed: true,
  capture: true,
  command: [ find_program('awk'), '-f', files('assets/mkconfig.awk') ]
)

pipemixer_sources = [
  'src/pipemixer.c',
  'src/log.c',
  'src/xmalloc.c',
  'src/utils.c',
  'src/config.c',
  'src/events.c',
  'src/format.c',
  'src/tui/tui.c',
  'src/tui/pad.c',
  'src/tui/menu.c',
  'src/collections/vec.c',
  'src/collections/map.c',
  'src/collections/string.c',
  'src/collections/wstring.c',
  'src/collections/dict.c',
  'src/pw/common.c',
  'src/pw/types.c',
  'src/pw/device.c',
  'src/pw/node.c',
  default_config_source
]

include_dirs = [
  'src'
]

executable('pipemixer', pipemixer_sources,
  include_directories: include_dirs,
  dependencies: [ncursesw_dep, pipewire_dep, m_dep, inih_dep],
  install: true)

install_data(
  'assets/io.github.heather7283.pipemixer.desktop',
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

conf_data = configuration_data()
conf_data.set('VERSION', meson.project_version())

foreach man: [ 'pipemixer.1', 'pipemixer.ini.5' ]
  configure_file(
    input: join_paths('man', '@0@.in'.format(man)),
    configuration: conf_data,
    output: man,
    install: true,
    install_dir: join_paths(get_option('mandir'), 'man@0@'.format(man.split('.')[-1])),
  )
endforeach

if get_option('shell-completions')
  zsh_install_dir = join_paths(get_option('datadir'), 'zsh', 'site-functions')
  install_data('completions/_pipemixer', install_dir: zsh_install_dir)
endif

