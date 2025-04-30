sizes = [16]
icons = ['tap-create', 'tap-extract-to', 'tap-extract']

foreach size : sizes
  foreach icon : icons
    install_data(
      '@0@x@0@'.format(size) / '@0@.png'.format(icon),
      install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '@0@x@0@'.format(size) / 'apps',
    )
  endforeach
endforeach
