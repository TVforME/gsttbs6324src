project('gst-tbs6324', 'c',
  version : '0.1.0',
  meson_version : '>= 0.49.0',
  default_options : ['warning_level=1', 'c_std=gnu99', 'buildtype=release'])

gst_version = meson.project_version()

cc = meson.get_compiler('c')

# Get build date and time
build_date = run_command('date', '+%Y-%m-%d').stdout().strip()
build_time = run_command('date', '+%H:%M:%S').stdout().strip()

# Generate config.h
conf = configuration_data()
conf.set('BUILD_DATE', build_date)
conf.set('BUILD_TIME', build_time)
conf.set('VERSION', meson.project_version())
conf.set('PACKAGE', meson.project_name())
config_h = configure_file(
  input : 'src/config.h.in',
  output : 'config.h',
  configuration : conf
)

# GStreamer dependencies
glib_dep = dependency('glib-2.0', version : '>= 2.40')
gst_dep = dependency('gstreamer-1.0', version : '>= 1.14')
gstbase_dep = dependency('gstreamer-base-1.0', version : '>= 1.14')

# Build type specific options
buildtype = get_option('buildtype')
c_debug_args = []
c_release_args = []

if buildtype == 'debug'
  c_debug_args = ['-g', '-DDEBUG', '-DGST_DEBUG=3']
elif buildtype == 'debugoptimized'
  c_debug_args = ['-g', '-DDEBUG']
  c_release_args = ['-O2']
elif buildtype == 'release'
  c_release_args = ['-O3', '-DNDEBUG']
elif buildtype == 'minsize'
  c_release_args = ['-Os', '-DNDEBUG']
endif

# Build plugin from src directory
tbs6324src_plugin = shared_library('gsttbs6324',
  'src/gsttbs6324.c',
  c_args : ['-DHAVE_CONFIG_H', '-I' + meson.build_root()] + c_debug_args + c_release_args,
  dependencies : [glib_dep, gst_dep, gstbase_dep],
  install : true,
  install_dir : gst_dep.get_pkgconfig_variable('pluginsdir'),
  install_tag : 'runtime'
)

# Strip symbols in release builds
if buildtype == 'release' or buildtype == 'minsize'
  meson.add_install_script('strip.sh', 'libgsttbs6324.so')
endif

# Install plugin
pkgconfig = import('pkgconfig')
pkgconfig.generate(tbs6324src_plugin,
  name : 'gst-tbs6324',
  description : 'GStreamer plugin for TBS6324 Quad 3G-SDI Encoder'
)
