/* config.h - standalone build configuration.
 *
 * Only used when building with the repository's own Makefile (outside
 * the geany-plugins autotools tree, which generates its own config.h
 * that takes precedence there).  Provides the gettext macros needed by
 * <geanyplugin.h> via glib's gi18n-lib.h.
 *
 * The plugin ships its own translations under the "geany-markdown"
 * domain (see po/); using a dedicated domain avoids shadowing the
 * geany-plugins package catalogs.  Both macros can be overridden on
 * the compiler command line (-DGETTEXT_PACKAGE=... -DLOCALEDIR=...),
 * and the Makefile passes LOCALEDIR to match its install layout.
 */
#ifndef GEANY_MARKDOWN_STANDALONE_CONFIG_H
#define GEANY_MARKDOWN_STANDALONE_CONFIG_H

#ifndef GETTEXT_PACKAGE
#define GETTEXT_PACKAGE "geany-markdown"
#endif

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

#endif /* GEANY_MARKDOWN_STANDALONE_CONFIG_H */
