tap_sources = [
  'tap-backend.c',
  'tap-backend.h',
  'tap-provider.c',
  'tap-provider.h',
  'thunar-archive-plugin.c',
]

shared_module(
  'thunar-archive-plugin',
  tap_sources,
  gnu_symbol_visibility: 'hidden',
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4util,
    thunarx,
  ],
  name_prefix: '',
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / 'thunarx-3',
)
