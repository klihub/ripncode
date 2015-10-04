SUBDIRS = . src

BUILT_SOURCES = ripncode-symlink

ripncode-symlink:
	if test ! -L $(top_srcdir)/ripncode; then		\
	   ln -sf ./src $(top_srcdir)/ripncode;			\
	fi;							\
	if test ! -L $(top_builddir)/ripncode; then		\
	   ln -sf ./src $(top_builddir)/ripncode;		\
	fi
