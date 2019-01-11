if get_option('manual')
  asciidoc = find_program('asciidoc', required : false)
  if asciidoc.found()
    foreach f: ['spice_protocol.txt', 'spice_uri_scheme.txt']
      custom_target('HTML for @0@'.format(f),
                    input : f,
                    output : '@BASENAME@.html',
                    build_by_default : true,
                    command : [asciidoc, '-n', '-a', 'icons', '-a', 'toc', '-o', '@OUTPUT@', '@INPUT@'])
    endforeach
  endif
endif
