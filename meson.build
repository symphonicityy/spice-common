#
# project definition
#
project('spice-common', 'c',
        meson_version : '>= 0.48.0',
        license : 'LGPLv2.1',
        default_options : ['warning_level=2'])

if not meson.is_subproject()
  warning('This project is only intended to be used as a subproject!')
endif

# some global vars
spice_common_global_cflags = ['-DG_LOG_DOMAIN="Spice"',
                              '-Wno-unused-parameter']

if get_option('alignment-checks')
  spice_common_global_cflags += ['-DSPICE_DEBUG_ALIGNMENT']
endif

spice_common_deps = []
spice_common_include = include_directories('.')

spice_proto = files('spice.proto')
spice_codegen = files('spice_codegen.py')
spice_codegen_files = [spice_codegen]

compiler = meson.get_compiler('c')
spice_common_config_data = configuration_data()
if get_option('extra-checks')
  spice_common_config_data.set('ENABLE_EXTRA_CHECKS', '1')
endif
if host_machine.endian() == 'big'
  spice_common_config_data.set('WORDS_BIGENDIAN', '1')
endif

if get_option('instrumentation') == 'recorder'
  spice_common_config_data.set('ENABLE_RECORDER', '1')
endif
if get_option('instrumentation') == 'agent'
  spice_common_config_data.set('ENABLE_AGENT_INTERFACE', '1')
endif

spice_common_generate_code = get_option('generate-code')
spice_common_generate_client_code = spice_common_generate_code == 'all' or spice_common_generate_code == 'client'
spice_common_generate_server_code = spice_common_generate_code == 'all' or spice_common_generate_code == 'server'

#
# check for system headers
#
headers = ['alloca.h',
           'arpa/inet.h',
           'dlfcn.h',
           'inttypes.h',
           'netinet/in.h',
           'stdlib.h',
           'sys/socket.h',
           'sys/stat.h',
           'sys/types.h',
           'unistd.h',
           'regex.h',
           'sys/mman.h']

foreach header : headers
  if compiler.has_header(header)
    spice_common_config_data.set('HAVE_@0@'.format(header.underscorify().to_upper()), '1')
  endif
endforeach

#
# check for system functions
#
functions = ['alloca',
             'sigaction',
             'drand48',
             'setlinebuf']

foreach func : functions
  if compiler.has_function(func)
    spice_common_config_data.set('HAVE_@0@'.format(func.underscorify().to_upper()), '1')
  endif
endforeach

# FIXME
# check for libm, as workaround for https://github.com/mesonbuild/meson/issues/3740
libm = compiler.find_library('m', required : false)
if libm.found()
  spice_common_deps += libm
endif

#
# check for mandatory dependencies
#
glib_version = '2.38'
glib_version_info = '>= @0@'.format(glib_version)

spice_protocol_version = '0.14.2'

spice_protocol_version_req = get_option('spice-protocol-version')
if spice_protocol_version_req.version_compare('> @0@'.format(spice_protocol_version))
  spice_protocol_version = spice_protocol_version_req
endif

deps = {'spice-protocol' : '>= @0@'.format(spice_protocol_version),
        'glib-2.0'       : glib_version_info,
        'pixman-1'       : '>= 0.17.7',
        'openssl'        : '>= 1.0.0'}

foreach dep, version : deps
  spice_common_deps += dependency(dep, version : version)
endforeach

gio2_deps = dependency('gio-2.0', version : glib_version_info)

#
# Non-mandatory/optional dependencies
#
optional_deps = {'opus' : '>= 0.9.14'}
foreach dep, version : optional_deps
  d = dependency(dep, required : get_option(dep), version : version)
  if d.found()
    spice_common_deps += d
    spice_common_config_data.set('HAVE_@0@'.format(dep.underscorify().to_upper()), '1')
  endif
endforeach

# Python
if spice_common_generate_client_code or spice_common_generate_server_code
  py_module = import('python')
  python = py_module.find_installation()

  if get_option('python-checks')
    foreach module : ['six', 'pyparsing']
      message('Checking for python module @0@'.format(module))
      cmd = run_command(python, '-m', module)
      if cmd.returncode() != 0
        error('Python module @0@ not found'.format(module))
      endif
    endforeach
  endif
endif

# smartcard check
smartcard_dep = dependency('libcacard', required : get_option('smartcard'), version : '>= 2.5.1')
if smartcard_dep.found()
  spice_common_deps += smartcard_dep
  spice_common_config_data.set('USE_SMARTCARD', '1')
endif

#
# global C defines
#
glib_encoded_version = 'GLIB_VERSION_@0@'.format(glib_version.underscorify())
spice_common_global_cflags += ['-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_encoded_version),
                               '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_encoded_version)]

add_project_arguments(compiler.get_supported_arguments(spice_common_global_cflags),
                      language : 'c')

#
# Subdirectories
#
subdir('python_modules')
subdir('common')
if get_option('tests')
  subdir('tests')
endif
subdir('docs')

#
# write config.h
#
configure_file(output : 'config.h',
               configuration : spice_common_config_data)
