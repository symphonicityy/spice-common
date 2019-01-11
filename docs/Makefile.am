NULL =
CLEANFILES =

ASCIIDOC_FILES =				\
	spice_protocol.txt			\
	spice_uri_scheme.txt			\
	$(NULL)

ASCIIDOC_FLAGS = -n -a icons -a toc

EXTRA_DIST =					\
	$(ASCIIDOC_FILES)			\
	meson.build				\
	$(NULL)

if BUILD_HTML_MANUAL
ASCIIDOC_HTML = $(ASCIIDOC_FILES:.txt=.html)

CLEANFILES += $(ASCIIDOC_HTML)

all-local: $(ASCIIDOC_HTML)

.txt.html:
	$(AM_V_GEN)$(ASCIIDOC) $(ASCIIDOC_FLAGS) -o $@ $<
endif

-include $(top_srcdir)/git.mk
