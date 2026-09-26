# Standalone Makefile for the enhanced Geany Markdown preview plugin.
#
# Builds the plugin outside the geany-plugins tree.  The GPL-friendly
# peg-markdown parser (with GFM pipe-table support) is compiled directly
# into the plugin (the FULL_PRICE code path in src/viewer.c).
#
# Targets:   all (default) / test / install / uninstall / clean
#
# Useful overrides (make VARIABLE=value ...):
#   PREFIX=/usr          install prefix
#   PLUGINDIR=...        where markdown.so goes  (default: geany libdir/geany)
#   PLUGINDATADIR=...    where the math assets go
#   NO_MATH=1            skip installing the offline math assets

DESTDIR  ?=
PREFIX   ?= /usr
LIBDIR   ?= $(PREFIX)/lib
DATADIR  ?= $(PREFIX)/share
GEANY_LIBDIR  ?= $(shell pkg-config --variable=libdir geany)
PLUGINDIR     ?= $(GEANY_LIBDIR)/geany
PLUGINDATADIR ?= $(DATADIR)/geany-plugins/markdown
DOCDIR        ?= $(DATADIR)/doc/geany-plugins/markdown
LOCALEDIR     ?= $(DATADIR)/locale

# Translations (po/<lang>.po -> <LOCALEDIR>/<lang>/LC_MESSAGES/geany-markdown.mo)
GETTEXT_PACKAGE = geany-markdown
LINGUAS  ?= $(shell cat po/LINGUAS)
MO_FILES = $(patsubst %,po/%.mo,$(LINGUAS))

PKGS = geany gtk+-3.0 webkit2gtk-4.1 glib-2.0 gmodule-2.0

PEG_SRC = \
	peg-markdown/markdown_lib.c \
	peg-markdown/markdown_output.c \
	peg-markdown/markdown_parser.c \
	peg-markdown/odf.c \
	peg-markdown/parsing_functions.c \
	peg-markdown/utility_functions.c

PLUGIN_SRC = \
	src/conf.c \
	src/plugin.c \
	src/viewer.c \
	src/math.c \
	src/markdown-gtk-compat.c

PEG_OBJS    = $(PEG_SRC:.c=.o)
PLUGIN_OBJS = $(PLUGIN_SRC:.c=.o)

CFLAGS ?= -O2 -g
BASE_FLAGS = -fPIC \
	$(shell pkg-config --cflags $(PKGS)) \
	-I. -Isrc -Ipeg-markdown \
	-DFULL_PRICE \
	-DG_LOG_DOMAIN="\"Markdown\"" \
	-DPLUGINDATADIR="\"$(PLUGINDATADIR)\"" \
	-DMARKDOWN_DOC_DIR="\"$(DOCDIR)\"" \
	-DMARKDOWN_HELP_FILE="\"$(DOCDIR)/html/help.html\"" \
	-DLOCALEDIR="\"$(LOCALEDIR)\"" \
	-Wno-deprecated-declarations

override CFLAGS += $(BASE_FLAGS)

LDLIBS = $(shell pkg-config --libs $(PKGS))

.PHONY: all test install uninstall clean pot

all: markdown.so test-math $(MO_FILES)

po/%.mo: po/%.po
	msgfmt -c --statistics -o $@ $<

markdown.so: $(PEG_OBJS) $(PLUGIN_OBJS)
	$(CC) -shared -o $@ $^ $(LDLIBS)

src/%.o: src/%.c $(wildcard src/*.h) config.h
	$(CC) $(CFLAGS) -Wall -c -o $@ $<

peg-markdown/%.o: peg-markdown/%.c
	$(CC) $(filter-out -Wall,$(CFLAGS)) -Wno-unused-result -c -o $@ $<

test-math: src/test-math.c src/math.c src/math.h
	$(CC) $(CFLAGS) -DTEST_MATH_DIR="\"$(CURDIR)/math\"" \
		-o $@ src/test-math.c src/math.c $(LDLIBS)

test: test-math
	./test-math

install: all
	install -d $(DESTDIR)$(PLUGINDIR)
	install -m 755 markdown.so $(DESTDIR)$(PLUGINDIR)/markdown.so
ifndef NO_MATH
	install -d $(DESTDIR)$(PLUGINDATADIR)
	cp -r math/katex math/mathjax math/README $(DESTDIR)$(PLUGINDATADIR)/
endif
	install -d $(DESTDIR)$(DOCDIR)/html
	install -m 644 docs/help.html docs/*.png $(DESTDIR)$(DOCDIR)/html/
	install -m 644 AUTHORS COPYING $(DESTDIR)$(DOCDIR)/
	for l in $(LINGUAS); do \
		install -D -m 644 po/$$l.mo \
			$(DESTDIR)$(LOCALEDIR)/$$l/LC_MESSAGES/$(GETTEXT_PACKAGE).mo; \
	done
	@echo "Installed. Restart Geany and enable 'Markdown' in the Plugin Manager."

uninstall:
	rm -f $(DESTDIR)$(PLUGINDIR)/markdown.so
	rm -rf $(DESTDIR)$(PLUGINDATADIR) $(DESTDIR)$(DOCDIR)
	for l in $(LINGUAS); do \
		rm -f $(DESTDIR)$(LOCALEDIR)/$$l/LC_MESSAGES/$(GETTEXT_PACKAGE).mo; \
	done

clean:
	rm -f markdown.so test-math $(MO_FILES) $(PEG_OBJS) $(PLUGIN_OBJS)

# Regenerate the translation template after changing _() strings in src/.
pot:
	xgettext --language=C --keyword=_ \
		--package-name=$(GETTEXT_PACKAGE) --package-version=1.0.0 \
		--msgid-bugs-address="https://github.com/yikuide-lab/geany-markdown/issues" \
		--from-code=UTF-8 -f po/POTFILES.in -o po/$(GETTEXT_PACKAGE).pot
	sed -i 's/"CHARSET"/"UTF-8"/; s/"ENCODING"/"8bit"/' po/$(GETTEXT_PACKAGE).pot
	@echo "Template updated: po/$(GETTEXT_PACKAGE).pot — merge into po/*.po with msgmerge."
