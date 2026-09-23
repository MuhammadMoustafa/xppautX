# xppautX — build the classic X11 xppaut binary from core/
# Phase 0: same code as upstream XPPAUT 8.0, just relocated.
# Requires: gcc, make, X11 headers (Debian/Ubuntu: apt install libx11-dev)

VERSION  = 8.0
MAJORVER = 8.0
MINORVER = 1

CC      ?= gcc
CSTD    ?= -std=c99 -pedantic -D_XOPEN_SOURCE=600
WARN    ?= -Wall
# gcc 14 and clang 16 turned these into errors; keep older compilers strict
# about them too, so a build that only runs here does not break CI.
STRICT  ?= -Werror=implicit-function-declaration -Werror=implicit-int -Werror=int-conversion -Werror=incompatible-pointer-types -Werror=return-type
OPT     ?= -g -O2
DEFS     = -DNOERRNO -DNON_UNIX_STDIO -DAUTO -DCVODE_YES -DHAVEDLL \
           -DMYSTR1=$(MAJORVER) -DMYSTR2=$(MINORVER)
# what `xppautX --version` prints: the release tag (release.yml sets
# XPP_VERSION), else git describe
XPPAUTX_VERSION ?= $(or $(XPP_VERSION),$(shell git describe --tags --always 2>/dev/null),dev)
# -I. is needed because fftn.c does "#include __FILE__"
INCS     = -I. -Icore -Icore/bitmaps $(X11_INC)
CFLAGS  ?= $(CSTD) $(WARN) $(STRICT) $(OPT) $(DEFS) $(INCS) -fcommon
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
NETLIBS  = -lpthread -lws2_32
else
EXE      =
DLLIB    = -ldl
LDSTATIC =
NETLIBS  = -lpthread
endif

# Every warning category gcc 13 ever reported here is fixed; WERROR=1 (what
# tools/verify.sh builds with) makes them errors so none comes back. Not the
# default: another compiler (clang on macOS, a newer gcc) may not know these
# names or may warn where gcc 13 does not, and must still build.
ifeq ($(WERROR),1)
STRICT += -Werror=unused-result -Werror=format-overflow -Werror=unused-variable   -Werror=misleading-indentation -Werror=unused-but-set-variable -Werror=format-security   -Werror=maybe-uninitialized -Werror=stringop-truncation -Werror=restrict -Werror=format   -Werror=tautological-compare -Werror=stringop-overflow   -Werror=aggressive-loop-optimizations -Werror=use-after-free -Werror=array-bounds   -Werror=format-truncation
endif

# For the legacy X11 xppaut target only: macOS/XQuartz users, pass
# X11_INC=-I/opt/X11/include X11_LIB=-L/opt/X11/lib (not needed for xppautx)
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
SERVER_SOURCES := $(addprefix $(SRCDIR)/, ui_json.c xppautx_main.c xpp_http.c xpp_inbox.c)
CORE_SOURCES := $(filter-out $(UI_SOURCES) $(SERVER_SOURCES) $(SRCDIR)/sbml2xpp.c,$(wildcard $(SRCDIR)/*.c))
# the page and script xppautX serves, compiled in
WEB_FILES := web/index.html web/xpp-client.js web/xpp-client.css
SERVER_OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SERVER_SOURCES)) $(BUILDDIR)/web_assets.o
$(BUILDDIR)/xppautx_main.o: CFLAGS += -DXPPAUTX_VERSION='"$(XPPAUTX_VERSION)"'
SOURCES := $(CORE_SOURCES) $(UI_SOURCES)
OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
CORE_OBJECTS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(CORE_SOURCES))
# per build directory, so a MinGW build does not replace the Linux library
CORELIB := $(BUILDDIR)/libxppcore.a

.PHONY: all clean x11free lib objects ltocheck lto-link xppautx test
all: xppaut
lib: $(CORELIB)

# every object of both programs, nothing linked (tools/warnings.sh)
objects: $(OBJECTS) $(SERVER_OBJECTS)

# Types that disagree across files, such as an extern whose array bound is
# not its definition's: an LTO link of both programs reports them
# (-Wlto-type-mismatch), where a normal build cannot see them. Built and
# linked in build/lto, so the programs in the tree are left alone.
ltocheck:
	@$(MAKE) -s BUILDDIR=build/lto OPT="-O1 -flto=auto -ffat-lto-objects" lto-link
lto-link: $(OBJECTS) $(SERVER_OBJECTS)
	@$(CC) -flto=auto -fcommon -o $(BUILDDIR)/xppaut $(OBJECTS) $(LDFLAGS) $(LIBS) 2> $(BUILDDIR)/lto.log || { cat $(BUILDDIR)/lto.log; exit 1; }
	@$(CC) -flto=auto -fcommon -o $(BUILDDIR)/xppautX$(EXE) $(SERVER_OBJECTS) $(CORE_OBJECTS) -lm $(DLLIB) $(NETLIBS) 2>> $(BUILDDIR)/lto.log || { cat $(BUILDDIR)/lto.log; exit 1; }
	@if grep -A4 'lto-type-mismatch' $(BUILDDIR)/lto.log; then echo "ltocheck: types differ across files"; exit 1; fi
# one X11-free program: browser front end, --server protocol and -silent batch
xppautx: xppautX$(EXE)

xppaut: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

$(CORELIB): $(CORE_OBJECTS)
	ar rcs $@ $(CORE_OBJECTS)

xppautX$(EXE): $(SERVER_OBJECTS) $(CORELIB)
	$(CC) $(LDSTATIC) -o $@ $(SERVER_OBJECTS) $(CORELIB) -lm $(DLLIB) $(NETLIBS)

# unit tests over libxppcore, for pure code that an end-to-end run would only
# report as a puzzling difference somewhere else. tests/README.md says more.
TEST_SOURCES := $(wildcard tests/test_*.c)
TEST_BINS := $(patsubst tests/%.c,$(BUILDDIR)/tests/%$(EXE),$(TEST_SOURCES))

test: $(TEST_BINS)
	@fail=0; for t in $(TEST_BINS); do ./$$t || fail=1; done; \
	  if [ $$fail -eq 0 ]; then echo "unit tests: all passed"; \
	  else echo "unit tests: FAILURES"; exit 1; fi

$(BUILDDIR)/tests/%$(EXE): tests/%.c $(CORELIB) | $(BUILDDIR)/tests
	$(CC) $(CFLAGS) -Itests -o $@ $< $(CORELIB) -lm $(DLLIB)

$(BUILDDIR)/tests:
	mkdir -p $@

$(BUILDDIR)/embed$(EXE): tools/embed.c | $(BUILDDIR)
	$(CC) -O2 -o $@ $<

$(BUILDDIR)/web_assets.c: $(BUILDDIR)/embed$(EXE) $(WEB_FILES)
	$(BUILDDIR)/embed$(EXE) $@ $(WEB_FILES)

$(BUILDDIR)/web_assets.o: $(BUILDDIR)/web_assets.c
	$(CC) -O2 -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR):
	mkdir -p $@

-include $(OBJECTS:.o=.d) $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.d,$(SERVER_SOURCES))

clean:
	rm -rf $(BUILDDIR) xppaut libxppcore.a xppautX xppautX.exe

.PHONY: x11free
x11free:
	tools/x11free.sh -v
