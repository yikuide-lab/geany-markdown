/* config.h - standalone build configuration.
 *
 * Only used when building with the repository's own Makefile (outside
 * the geany-plugins autotools tree, which generates its own config.h
 * that takes precedence there).  Provides the gettext macros needed by
 * <geanyplugin.h> via glib's gi18n-lib.h.
 */
#ifndef GEANY_MARKDOWN_STANDALONE_CONFIG_H
#define GEANY_MARKDOWN_STANDALONE_CONFIG_H

/* Translation domain: matches the geany-plugins package so existing
 * translations of the marker strings are picked up when present. */
#define GETTEXT_PACKAGE "geany-plugins"

/* System locale directory; override here if Geany is installed
 * somewhere other than /usr. */
#define LOCALEDIR "/usr/share/locale"

#endif /* GEANY_MARKDOWN_STANDALONE_CONFIG_H */
