plugin_install_dir = get_option('prefix') / get_option('libexecdir') / 'thunar-archive-plugin'

install_data(
  ['ark.tap', 'engrampa.tap', 'file-roller.tap'],
  install_dir: plugin_install_dir,
  install_mode: 'rwxr-xr-x',
)

# Install symlink to 'gnome-file-roller.tap'
install_symlink(
  'gnome-file-roller.tap',
  install_dir: plugin_install_dir,
  pointing_to: 'file-roller.tap',
)
install_symlink(
  'org.gnome.FileRoller.tap',
  install_dir: plugin_install_dir,
  pointing_to: 'file-roller.tap',
)

# Install symlink to 'mate-engrampa.tap'
install_symlink(
  'mate-engrampa.tap',
  install_dir: plugin_install_dir,
  pointing_to: 'engrampa.tap',
)

# Install symlink to 'ark.tap'
install_symlink(
  'org.kde.ark.tap',
  install_dir: plugin_install_dir,
  pointing_to: 'ark.tap',
)
