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

# sbml2xpp.c needs libsbml and is not part of the upstream build.
SOURCES := $(filter-out $(SRCDIR)/sbml2xpp.c,$(wildcard $(SRCDIR)/*.c))
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))

.PHONY: all clean x11free
all: xppaut

xppaut: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR):
	mkdir -p $@

-include $(OBJECTS:.o=.d)

clean:
	rm -rf $(BUILDDIR) xppaut

.PHONY: x11free
x11free:
	tools/x11free.sh -v
