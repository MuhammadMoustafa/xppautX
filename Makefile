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

# Native Windows (MinGW-w64 gcc, from Git Bash or MSYS2): only the X11-free
# targets build there: make server cli
ifeq ($(OS),Windows_NT)
ifeq ($(origin CC),default)
CC       = gcc
endif
CSTD     = -std=gnu99
EXE      = .exe
DLLIB    =
LDSTATIC = -static
else
EXE      =
DLLIB    = -ldl
LDSTATIC =
endif

# macOS/XQuartz users: make X11_INC=-I/opt/X11/include X11_LIB=-L/opt/X11/lib
X11_INC ?=
X11_LIB ?=

SRCDIR   = core
BUILDDIR = build/obj

# Sources that need X11 (the front end). Everything else is libxppcore.
UI_SOURCES := $(addprefix $(SRCDIR)/, abort.c aniwin.c aplotwin.c auto_x11.c \
  browse.c calc.c choice_box.c color.c dialog_box.c eig_list.c \
  ggets.c graphics_x11.c init_conds.c kinescope.c main.c many_pops.c \
  menu.c menudrive.c pop_list.c rubber.c txtread.c ui_x11.c \
  xppaut_main.c)
# sbml2xpp.c needs libsbml and is not part of the upstream build.
SERVER_SOURCES := $(addprefix $(SRCDIR)/, ui_json.c xppcore_server.c)
CORE_SOURCES := $(filter-out $(UI_SOURCES) $(SERVER_SOURCES) $(SRCDIR)/sbml2xpp.c $(SRCDIR)/xppcore_cli.c,$(wildcard $(SRCDIR)/*.c))
SOURCES := $(CORE_SOURCES) $(UI_SOURCES)
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
CORE_OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(CORE_SOURCES))
# per build directory, so a MinGW build does not replace the Linux library
CORELIB := $(BUILDDIR)/libxppcore.a

.PHONY: all clean x11free lib cli server
all: xppaut
lib: $(CORELIB)
cli: xppcore-cli$(EXE)
server: xppcore-server$(EXE)

xppaut: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

$(CORELIB): $(CORE_OBJECTS)
	ar rcs $@ $(CORE_OBJECTS)

xppcore-cli$(EXE): $(BUILDDIR)/xppcore_cli.o $(CORELIB)
	$(CC) $(LDSTATIC) -o $@ $(BUILDDIR)/xppcore_cli.o $(CORELIB) -lm $(DLLIB)

xppcore-server$(EXE): $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SERVER_SOURCES)) $(CORELIB)
	$(CC) $(LDSTATIC) -o $@ $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SERVER_SOURCES)) $(CORELIB) -lm $(DLLIB)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR):
	mkdir -p $@

-include $(OBJECTS:.o=.d) $(BUILDDIR)/xppcore_cli.d $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.d,$(SERVER_SOURCES))

clean:
	rm -rf $(BUILDDIR) xppaut libxppcore.a xppcore-cli xppcore-server xppcore-cli.exe xppcore-server.exe

.PHONY: x11free
x11free:
	tools/x11free.sh -v
