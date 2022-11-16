bashcompletiondir=@datadir@/bash-completion/completions

BASH_COMPLETION_TEMPLATES = \
    doom.template

if HAVE_BASH_COMPLETION

BASH_COMPLETION_SCRIPTLETS = \
    @PROGRAM_PREFIX@doom

bashcompletion_DATA = $(BASH_COMPLETION_SCRIPTLETS)
CLEANFILES = $(BASH_COMPLETION_SCRIPTLETS)

DOCGEN = $(top_srcdir)/man/docgen
DOCGEN_COMMON_ARGS = -n "@PROGRAM_SPREFIX@" -s "@PACKAGE_NAME@" -z "@PACKAGE_SHORTNAME@"

@PROGRAM_PREFIX@doom: $(top_srcdir)/src $(DOCGEN) $(BASH_COMPLETION_TEMPLATES)
	$(DOCGEN) $(DOCGEN_COMMON_ARGS) \
                  -g doom -b doom.template \
                  $(top_srcdir)/src $(top_srcdir)/src/doom > $@

EXTRA_DIST = \
    $(BASH_COMPLETION_TEMPLATES) \
    $(BASH_COMPLETION_SCRIPTLETS)

else

EXTRA_DIST = \
    $(BASH_COMPLETION_TEMPLATES)

endif
