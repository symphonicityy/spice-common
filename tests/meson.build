#
# Build tests
#
tests = [
  'test-logging',
  'test-region',
  'test-ssl-verify',
  'test-utils',
]
tests_deps = [spice_common_dep, gio2_deps]

foreach t : tests
  name = t.underscorify()
  exe = executable(name, '@0@.c'.format(t),
                   dependencies : tests_deps,
                   install : false)
  test(name, exe)
endforeach

#
# test_marshallers
#
if spice_common_generate_client_code or spice_common_generate_server_code
  test_proto = files('test-marshallers.proto')

  test_marshallers_sources = ['test-marshallers.c', 'test-marshallers.h']
  generated_marshallers = []

  targets = [
      ['test_marshallers', test_proto, 'generated_test_marshallers.c', [
        '--suffix=_test',
        '--generate-marshallers',
        '--server',
        '--include', 'test-marshallers.h',
        '@INPUT@', '@OUTPUT@'
      ]],
      ['test_marshallers_h', test_proto, 'generated_test_marshallers.h', [
        '--suffix=_test',
        '--generate-marshallers',
        '--server',
        '--include', 'test-marshallers.h',
        '-H',
        '@INPUT@', '@OUTPUT@'
      ]],
      ['test_demarshallers', test_proto, 'generated_test_demarshallers.c', [
        '--suffix=_test',
        '--generate-demarshallers',
        '--client',
        '--include', 'test-marshallers.h',
        '--generated-declaration-file', meson.current_build_dir() /'generated_test_messages.h',
        '@INPUT@', '@OUTPUT@'
      ]],
      ['test_enums_h', test_proto, 'generated_test_enums.h', [
        '--suffix=_test',
        '-e',
        '@INPUT@', '@OUTPUT@'
      ]],
  ]

  foreach t : targets
    cmd = [python, spice_codegen] + t[3]
    generated_marshallers += custom_target(t[0], input: t[1], output : t[2], command: cmd, depend_files : spice_codegen_files)
  endforeach

  test_marshallers_sources += generated_marshallers

  test('test_marshallers',
       executable('test_marshallers', test_marshallers_sources,
                  dependencies : spice_common_dep,
                  install : false))
endif

#
# test_quic
#
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version : '>= 2.26', required : false)
if gdk_pixbuf_dep.found()
  test('test_quic',
       executable('test_quic', 'test-quic.c',
                  dependencies : [spice_common_dep, gdk_pixbuf_dep],
                  install : false), timeout : 120)
endif

#
# helper_fuzzer_demarshallers
#
if spice_common_generate_client_code and spice_common_generate_server_code
  executable('helper_fuzzer_demarshallers',
             ['helper-fuzzer-demarshallers.c'] + generated_marshallers,
             dependencies : [tests_deps, spice_common_server_dep, spice_common_client_dep],
             install : false)
endif
