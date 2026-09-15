# xppautX — build the classic X11 xppaut binary from core/
# Phase 0: same code as upstream XPPAUT 8.0, just relocated.
# Requires: gcc, make, X11 headers (Debian/Ubuntu: apt install libx11-dev)

VERSION  = 8.0
MAJORVER = 8.0
MINORVER = 1

CC      ?= gcc
CSTD    ?= -std=c99 -pedantic -D_XOPEN_SOURCE=600
WARN    ?= -Wall
OPT     ?= -g -O2
DEFS     = -DNOERRNO -DNON_UNIX_STDIO -DAUTO -DCVODE_YES -DHAVEDLL \
           -DMYSTR1=$(MAJORVER) -DMYSTR2=$(MINORVER)
# -I. is needed because fftn.c does "#include __FILE__"
INCS     = -I. -Icore -Icore/bitmaps $(X11_INC)
CFLAGS  ?= $(CSTD) $(WARN) $(OPT) $(DEFS) $(INCS) -fcommon
LDFLAGS ?= $(X11_LIB) -fcommon
LIBS     = -lX11 -lm -ldl

# macOS/XQuartz users: make X11_INC=-I/opt/X11/include X11_LIB=-L/opt/X11/lib
X11_INC ?=
X11_LIB ?=

SRCDIR   = core
BUILDDIR = build/obj

# Sources that need X11 (the front end). Everything else is libxppcore.
UI_SOURCES := $(addprefix $(SRCDIR)/, abort.c aniparse.c arrayplot.c auto_x11.c \
  browse.c calc.c choice_box.c color.c dialog_box.c edit_rhs.c eig_list.c \
  ggets.c graphics_x11.c init_conds.c kinescope.c main.c many_pops.c \
  menu.c menudrive.c pop_list.c rubber.c scrngif.c txtread.c ui_x11.c \
  userbut.c xppaut_main.c)
# sbml2xpp.c needs libsbml and is not part of the upstream build.
CORE_SOURCES := $(filter-out $(UI_SOURCES) $(SRCDIR)/sbml2xpp.c $(SRCDIR)/xppcore_cli.c,$(wildcard $(SRCDIR)/*.c))
SOURCES := $(CORE_SOURCES) $(UI_SOURCES)
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
CORE_OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(CORE_SOURCES))

.PHONY: all clean x11free lib cli
all: xppaut
lib: libxppcore.a
cli: xppcore-cli

xppaut: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

libxppcore.a: $(CORE_OBJECTS)
	ar rcs $@ $(CORE_OBJECTS)

xppcore-cli: $(BUILDDIR)/xppcore_cli.o libxppcore.a
	$(CC) -o $@ $(BUILDDIR)/xppcore_cli.o libxppcore.a -lm -ldl

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR):
	mkdir -p $@

-include $(OBJECTS:.o=.d) $(BUILDDIR)/xppcore_cli.d

clean:
	rm -rf $(BUILDDIR) xppaut libxppcore.a xppcore-cli

.PHONY: x11free
x11free:
	tools/x11free.sh -v
