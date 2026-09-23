# xppautX — build the classic X11 xppaut binary from core/
# Phase 0: same code as upstream XPPAUT 8.0, just relocated.
# Requires: gcc, make, X11 headers (Debian/Ubuntu: apt install libx11-dev)

VERSION  = 8.0
MAJORVER = 8.0
MINORVER = 1

# The core is C converting to C++ file by file (CLAUDE.md, "C and C++"):
# core/x.c builds with $(CC), core/x.cpp with $(CXX), and a program with any
# C++ object links with $(CXX).
CC      ?= gcc
CXX     ?= g++
CSTD    ?= -std=c99 -pedantic -D_XOPEN_SOURCE=600
CXXSTD  ?= -std=c++17 -pedantic
WARN    ?= -Wall
# gcc 14 and clang 16 turned these into errors; keep older compilers strict
# about them too, so a build that only runs here does not break CI. All but
# return-type are C-only names (C++ has always rejected those constructs).
STRICT  ?= -Werror=implicit-function-declaration -Werror=implicit-int -Werror=int-conversion -Werror=incompatible-pointer-types -Werror=return-type
CXXSTRICT ?= -Werror=return-type
OPT     ?= -g -O2
DEFS     = -DNOERRNO -DNON_UNIX_STDIO -DAUTO -DCVODE_YES -DHAVEDLL \
           -DMYSTR1=$(MAJORVER) -DMYSTR2=$(MINORVER)
# what `xppautX --version` prints: the release tag (release.yml sets
# XPP_VERSION), else git describe
XPPAUTX_VERSION ?= $(or $(XPP_VERSION),$(shell git describe --tags --always 2>/dev/null),dev)
# -I. is needed because fftn.c does "#include __FILE__"
INCS     = -I. -Icore -Icore/bitmaps $(X11_INC)
CFLAGS  ?= $(CSTD) $(WARN) $(STRICT) $(OPT) $(DEFS) $(INCS) -fcommon
# no -fcommon: C++ has no tentative definitions
CXXFLAGS ?= $(CXXSTD) $(WARN) $(CXXSTRICT) $(OPT) $(DEFS) $(INCS)
LDFLAGS ?= $(X11_LIB) -fcommon
LIBS     = -lX11 -lm -ldl

# Native Windows (MinGW-w64 gcc, from Git Bash or MSYS2): only the X11-free
# targets build there: make server cli
ifeq ($(OS),Windows_NT)
# (a make built elsewhere may default to its builder's compiler paths)
ifeq ($(origin CC),default)
CC       = gcc
endif
ifeq ($(origin CXX),default)
CXX      = g++
endif
CSTD     = -std=gnu99
CXXSTD   = -std=gnu++17
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
WERROR_FLAGS = -Werror=unused-result -Werror=format-overflow -Werror=unused-variable   -Werror=misleading-indentation -Werror=unused-but-set-variable -Werror=format-security   -Werror=maybe-uninitialized -Werror=stringop-truncation -Werror=restrict -Werror=format   -Werror=tautological-compare -Werror=stringop-overflow   -Werror=aggressive-loop-optimizations -Werror=use-after-free -Werror=array-bounds   -Werror=format-truncation
STRICT += $(WERROR_FLAGS)
CXXSTRICT += $(WERROR_FLAGS)
endif

# For the legacy X11 xppaut target only: macOS/XQuartz users, pass
# X11_INC=-I/opt/X11/include X11_LIB=-L/opt/X11/lib (not needed for xppautx)
X11_INC ?=
X11_LIB ?=

SRCDIR   = core
BUILDDIR = build/obj

# Source lists name files without an extension: $(call src,a b) finds
# core/a.c or core/a.cpp, so converting a file is `git mv x.c x.cpp` alone.
ALL_SOURCES := $(wildcard $(SRCDIR)/*.c $(SRCDIR)/*.cpp)
$(foreach f,$(filter $(basename $(filter %.cpp,$(ALL_SOURCES))),$(basename $(filter %.c,$(ALL_SOURCES)))),$(error both $(f).c and $(f).cpp exist))
src = $(foreach f,$(1),$(or $(filter $(SRCDIR)/$(f).c $(SRCDIR)/$(f).cpp,$(ALL_SOURCES)),$(error no $(SRCDIR)/$(f).c or .cpp)))
# core/x.c or core/x.cpp -> $(BUILDDIR)/x.o
obj = $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%.o,$(basename $(1)))
# the linker for a program built from these sources: $(CXX) when one of
# them is C++ (it brings the C++ runtime), else $(CC)
link = $(if $(filter %.cpp,$(1)),$(CXX),$(CC))

# Sources that need X11 (the front end). Everything else is libxppcore.
UI_SOURCES := $(call src, abort aniwin aplotwin auto_x11 \
  browse calc choice_box color dialog_box eig_list \
  ggets graphics_x11 init_conds kinescope main many_pops \
  menu menudrive pop_list rubber txtread ui_x11 \
  xppaut_main)
# sbml2xpp.c needs libsbml and is not part of the upstream build.
SERVER_SOURCES := $(call src, ui_json xppautx_main xpp_http xpp_inbox)
CORE_SOURCES := $(filter-out $(UI_SOURCES) $(SERVER_SOURCES) $(SRCDIR)/sbml2xpp.%,$(ALL_SOURCES))
# the pages xppautX serves, compiled in: the classic front end at /, the new
# one at /v2/ (web2/dist, built from web2/src and committed: web2/build.mjs)
WEB_FILES := web/index.html web/xpp-client.js web/xpp-client.css
WEB2_FILES := web2/dist/index.html web2/dist/app.js web2/dist/app.css web2/dist/inter.woff2 web2/dist/inter-OFL.txt
SERVER_OBJECTS := $(call obj,$(SERVER_SOURCES)) $(BUILDDIR)/web_assets.o
$(BUILDDIR)/xppautx_main.o: CFLAGS += -DXPPAUTX_VERSION='"$(XPPAUTX_VERSION)"'
$(BUILDDIR)/xppautx_main.o: CXXFLAGS += -DXPPAUTX_VERSION='"$(XPPAUTX_VERSION)"'
SOURCES := $(CORE_SOURCES) $(UI_SOURCES)
OBJECTS := $(call obj,$(SOURCES))
CORE_OBJECTS := $(call obj,$(CORE_SOURCES))
# the linkers of the X11 xppaut and of xppautX
LINK_X11 := $(call link,$(SOURCES))
LINK_X := $(call link,$(SERVER_SOURCES) $(CORE_SOURCES))
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
	@$(LINK_X11) -flto=auto -fcommon -o $(BUILDDIR)/xppaut $(OBJECTS) $(LDFLAGS) $(LIBS) 2> $(BUILDDIR)/lto.log || { cat $(BUILDDIR)/lto.log; exit 1; }
	@$(LINK_X) -flto=auto -fcommon -o $(BUILDDIR)/xppautX$(EXE) $(SERVER_OBJECTS) $(CORE_OBJECTS) -lm $(DLLIB) $(NETLIBS) 2>> $(BUILDDIR)/lto.log || { cat $(BUILDDIR)/lto.log; exit 1; }
	@if grep -A4 'lto-type-mismatch' $(BUILDDIR)/lto.log; then echo "ltocheck: types differ across files"; exit 1; fi
# AddressSanitizer + UndefinedBehaviorSanitizer (and LeakSanitizer, part of
# ASan on Linux): both programs built into build/asan, the ones in the tree
# left alone. tools/asancheck.sh builds them and runs the checks.
ifeq ($(ASAN),1)
SANITIZE := -fsanitize=address,undefined -fno-omit-frame-pointer
# -O1 and the instrumentation blur gcc's value ranges: these two then warn
# about code the normal (WERROR) build proves safe
OPT := -g -O1 $(SANITIZE) -Wno-format-overflow -Wno-restrict
endif
.PHONY: asan asan-link
asan:
	@$(MAKE) BUILDDIR=build/asan ASAN=1 asan-link
asan-link: $(BUILDDIR)/xppautX$(EXE) $(if $(filter Windows_NT,$(OS)),,$(BUILDDIR)/xppaut)
$(BUILDDIR)/xppautX$(EXE): $(SERVER_OBJECTS) $(CORELIB)
	$(LINK_X) $(SANITIZE) -o $@ $(SERVER_OBJECTS) $(CORELIB) -lm $(DLLIB) $(NETLIBS)
$(BUILDDIR)/xppaut: $(OBJECTS)
	$(LINK_X11) $(SANITIZE) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

# one X11-free program: browser front end, --server protocol and -silent batch
xppautx: xppautX$(EXE)

xppaut: $(OBJECTS)
	$(LINK_X11) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

$(CORELIB): $(CORE_OBJECTS)
	ar rcs $@ $(CORE_OBJECTS)

xppautX$(EXE): $(SERVER_OBJECTS) $(CORELIB)
	$(LINK_X) $(LDSTATIC) -o $@ $(SERVER_OBJECTS) $(CORELIB) -lm $(DLLIB) $(NETLIBS)

# unit tests over libxppcore, for pure code that an end-to-end run would only
# report as a puzzling difference somewhere else. tests/README.md says more.
# a test is C or C++ (tests/test_x.c or .cpp)
TEST_SOURCES := $(wildcard tests/test_*.c tests/test_*.cpp)
TEST_OBJECTS := $(patsubst tests/%,$(BUILDDIR)/tests/%.o,$(basename $(TEST_SOURCES)))
TEST_BINS := $(TEST_OBJECTS:.o=$(EXE))
LINK_TESTS := $(call link,$(CORE_SOURCES) $(TEST_SOURCES))
.SECONDARY: $(TEST_OBJECTS)

test: $(TEST_BINS)
	@fail=0; for t in $(TEST_BINS); do ./$$t || fail=1; done; \
	  if [ $$fail -eq 0 ]; then echo "unit tests: all passed"; \
	  else echo "unit tests: FAILURES"; exit 1; fi

$(TEST_BINS): %$(EXE): %.o $(CORELIB)
	$(LINK_TESTS) $(SANITIZE) -o $@ $< $(CORELIB) -lm $(DLLIB)

$(BUILDDIR)/tests/%.o: tests/%.c | $(BUILDDIR)/tests
	$(CC) $(CFLAGS) -Itests -MMD -MP -c $< -o $@

$(BUILDDIR)/tests/%.o: tests/%.cpp | $(BUILDDIR)/tests
	$(CXX) $(CXXFLAGS) -Itests -MMD -MP -MF $(@:.o=.cpp.d) -c $< -o $@

$(BUILDDIR)/tests:
	mkdir -p $@

$(BUILDDIR)/embed$(EXE): tools/embed.c | $(BUILDDIR)
	$(CC) -O2 -o $@ $<

$(BUILDDIR)/web_assets.c: $(BUILDDIR)/embed$(EXE) $(WEB_FILES) $(WEB2_FILES)
	$(BUILDDIR)/embed$(EXE) $@ $(WEB_FILES) --prefix=/v2/ $(WEB2_FILES)

$(BUILDDIR)/web_assets.o: $(BUILDDIR)/web_assets.c
	$(CC) -O2 -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.cpp.d) -c $< -o $@

$(BUILDDIR):
	mkdir -p $@

# Dependency files: x.d for core/x.c, x.cpp.d for core/x.cpp, and only
# those of existing sources are read, so after `git mv x.c x.cpp` the
# stale x.d, which names core/x.c, does not stop the build.
depfiles = $(patsubst %.c,%.d,$(patsubst %.cpp,%.cpp.d,$(1)))
-include $(call depfiles,$(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES) $(SERVER_SOURCES)))
-include $(call depfiles,$(patsubst tests/%,$(BUILDDIR)/tests/%,$(TEST_SOURCES)))

clean:
	rm -rf $(BUILDDIR) xppaut libxppcore.a xppautX xppautX.exe

.PHONY: x11free
x11free:
	tools/x11free.sh -v
