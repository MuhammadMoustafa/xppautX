# xppautX — build xppautX from core/. No X11: the legacy X11 front end
# was removed (issue #20); xppautX is the one program (its desktop window,
# browser mode, --server or -silent).
# Requires: gcc, make; for the window on Linux, WebKitGTK (optional, below)

VERSION  = 8.0
MAJORVER = 8.0
MINORVER = 1

# The core is C converting to C++ file by file (CLAUDE.md, "C and C++"):
# core/x.c builds with $(CC), core/x.cpp with $(CXX), and a program with any
# C++ object links with $(CXX).
CC      ?= gcc
CXX     ?= g++
CSTD    ?= -std=c99 -pedantic -D_XOPEN_SOURCE=600
CXXSTD  ?= -std=c++23 -pedantic
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
# and the commit, for the window's Help > About
XPPAUTX_COMMIT ?= $(or $(shell git rev-parse --short HEAD 2>/dev/null),unknown)
# -I. is needed because fftn.c does "#include __FILE__"
INCS     = -I. -Icore
CFLAGS  ?= $(CSTD) $(WARN) $(STRICT) $(OPT) $(DEFS) $(INCS) -fcommon
# no -fcommon: C++ has no tentative definitions
CXXFLAGS ?= $(CXXSTD) $(WARN) $(CXXSTRICT) $(OPT) $(DEFS) $(INCS)
LDFLAGS ?= -fcommon
LIBS     = -lm -ldl

# Native Windows (MinGW-w64 gcc, from Git Bash or MSYS2)
ifeq ($(OS),Windows_NT)
# (a make built elsewhere may default to its builder's compiler paths)
ifeq ($(origin CC),default)
CC       = gcc
endif
ifeq ($(origin CXX),default)
CXX      = g++
endif
CSTD     = -std=gnu99
CXXSTD   = -std=gnu++23
EXE      = .exe
DLLIB    =
LDSTATIC = -static
# -mwindows: a GUI-subsystem exe, so Explorer and a file association start
# it with no console window (xpp_win32.c's xpp_win32_attach_console()
# reattaches to a real one for the command-line modes; --server's pipes are
# untouched, W13b)
NETLIBS  = -lpthread -lws2_32 -mwindows
WINDRES ?= windres
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

# (rules come before `all` below: it stays the default goal)
.DEFAULT_GOAL := all

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

# sbml2xpp.c needs libsbml and is not part of the upstream build.
SERVER_SOURCES := $(call src, ui_json json_io json_prompts json_state json_windows json_auto json_ani xppautx_main xpp_http xpp_inbox xpp_window)
CORE_SOURCES := $(filter-out $(SERVER_SOURCES) $(SRCDIR)/sbml2xpp.%,$(ALL_SOURCES))
# the page xppautX serves, compiled in: web2 (web2/dist, built from
# web2/src and committed: web2/build.mjs)
WEB2_FILES := web2/dist/index.html web2/dist/app.js web2/dist/app.css web2/dist/manual.json web2/dist/inter.woff2 \
  web2/dist/inter-greek.woff2 web2/dist/inter-OFL.txt
SERVER_OBJECTS := $(call obj,$(SERVER_SOURCES)) $(BUILDDIR)/web_assets.o
$(BUILDDIR)/xppautx_main.o: CFLAGS += -DXPPAUTX_VERSION='"$(XPPAUTX_VERSION)"' -DXPPAUTX_COMMIT='"$(XPPAUTX_COMMIT)"'
$(BUILDDIR)/xppautx_main.o: CXXFLAGS += -DXPPAUTX_VERSION='"$(XPPAUTX_VERSION)"' -DXPPAUTX_COMMIT='"$(XPPAUTX_COMMIT)"'

# The desktop window (core/xpp_window.cpp, docs/roadmap.md W13a): the
# vendored third_party/webview, built as its own object, on WebView2
# (Windows: the WebView2 SDK headers in third_party/webview2 and the
# library's own loader, so no WebView2Loader.dll), WKWebView (macOS) or
# WebKitGTK (Linux). On Linux only when pkg-config finds webkit2gtk-4.1:
# building never requires it, and without it xppautX is browser-only.
# WINDOW=0 builds browser-only anywhere; the sanitizer build always is (the web
# view is not our code).
WEBVIEW_DIR = third_party/webview
ifeq ($(ASAN),1)
WINDOW := 0
endif
ifeq ($(OS),Windows_NT)
WINDOW ?= 1
WINDOW_CFLAGS = -isystem third_party/webview2/include
WINDOW_LIBS = -lole32 -lshell32 -lshlwapi -luser32 -lcomdlg32 -ladvapi32 -lversion
else ifeq ($(shell uname -s 2>/dev/null),Darwin)
# opt-in (WINDOW=1) until CI's macOS job has built and run it
WINDOW ?= 0
WINDOW_LIBS = -framework Cocoa -framework WebKit
else
ifndef WINDOW
WINDOW := $(shell pkg-config --exists webkit2gtk-4.1 gtk+-3.0 2>/dev/null && echo 1 || echo 0)
endif
ifeq ($(WINDOW),1)
# -isystem: warnings in GTK's headers are not ours
WINDOW_CFLAGS := $(patsubst -I%,-isystem %,$(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.1))
WINDOW_LIBS := $(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.1)
# the GTK window's icon (xpp_window.cpp, W13b): the installed hicolor theme
# icon by name when tools/associate/install-linux.sh has put one there, else
# this fallback, embedded so an unpacked-but-not-installed build still has
# one. assets/icons/hicolor/256x256/apps/xppautx.png is tools/make_icons.py's.
SERVER_OBJECTS += $(BUILDDIR)/icon_assets.o
$(BUILDDIR)/xpp_window.o: CXXFLAGS += -DXPP_ICON_ASSET
endif
endif
ifeq ($(WINDOW),1)
SERVER_OBJECTS += $(BUILDDIR)/webview.o
$(BUILDDIR)/xpp_window.o: CXXFLAGS += -DXPP_WINDOW -isystem $(WEBVIEW_DIR)/include $(WINDOW_CFLAGS)
else
WINDOW_LIBS =
endif
# the library, unchanged: its warnings are not ours (-isystem), nor is LTO
$(BUILDDIR)/webview.o: $(WEBVIEW_DIR)/src/webview.cc $(BUILDDIR)/toolchain.stamp | $(BUILDDIR)
	$(CXX) $(CXXSTD) $(filter-out -flto% -ffat-lto-objects,$(OPT)) -DWEBVIEW_STATIC -isystem $(WEBVIEW_DIR)/include $(WINDOW_CFLAGS) -c $< -o $@

# Windows: the icon (resource 32512, IDI_APPLICATION's number, which the
# web view's window takes) and the version block, from assets/xppautx.rc
ifeq ($(OS),Windows_NT)
SERVER_OBJECTS += $(BUILDDIR)/xppautx_res.o
$(BUILDDIR)/xppautx_res.o: assets/xppautx.rc assets/icon.ico $(BUILDDIR)/version.stamp | $(BUILDDIR)
	@echo '#define XPPAUTX_VERSION_STR "$(XPPAUTX_VERSION)"' > $(BUILDDIR)/version_rc.h
	$(WINDRES) -I$(BUILDDIR) -O coff -i $< -o $@
endif
# the version is an input of xppautx_main.o: the stamp is rewritten only when
# it changes, so --version never names an older commit than the build's
$(BUILDDIR)/xppautx_main.o: $(BUILDDIR)/version.stamp
CORE_OBJECTS := $(call obj,$(CORE_SOURCES))
# the linker of xppautX
LINK_X := $(call link,$(SERVER_SOURCES) $(CORE_SOURCES))
# per build directory, so a MinGW build does not replace the Linux library
CORELIB := $(BUILDDIR)/libxppcore.a

.PHONY: all clean lib objects ltocheck lto-link xppautx test FORCE
all: xppautx
lib: $(CORELIB)

# every object, nothing linked (tools/warnings.sh)
objects: $(CORE_OBJECTS) $(SERVER_OBJECTS)

# Types that disagree across files, such as an extern whose array bound is
# not its definition's: an LTO link reports them (-Wlto-type-mismatch),
# where a normal build cannot see them. Built and linked in build/lto, so
# the program in the tree is left alone.
ltocheck:
	@$(MAKE) -s BUILDDIR=build/lto OPT="-O1 -flto=auto -ffat-lto-objects" lto-link
lto-link: $(CORE_OBJECTS) $(SERVER_OBJECTS)
	@$(LINK_X) -flto=auto -fcommon -o $(BUILDDIR)/xppautX$(EXE) $(SERVER_OBJECTS) $(CORE_OBJECTS) -lm $(DLLIB) $(NETLIBS) $(WINDOW_LIBS) 2> $(BUILDDIR)/lto.log || { cat $(BUILDDIR)/lto.log; exit 1; }
	@if grep -A4 'lto-type-mismatch' $(BUILDDIR)/lto.log; then echo "ltocheck: types differ across files"; exit 1; fi
# AddressSanitizer + UndefinedBehaviorSanitizer (and LeakSanitizer, part of
# ASan on Linux): built into build/asan, the program in the tree left
# alone. tools/asancheck.sh builds it and runs the checks.
ifeq ($(ASAN),1)
SANITIZE := -fsanitize=address,undefined -fno-omit-frame-pointer
# -O1 and the instrumentation blur gcc's value ranges: these two then warn
# about code the normal (WERROR) build proves safe
OPT := -g -O1 $(SANITIZE) -Wno-format-overflow -Wno-restrict
endif
.PHONY: asan asan-link
asan:
	@$(MAKE) BUILDDIR=build/asan ASAN=1 asan-link
asan-link: $(BUILDDIR)/xppautX$(EXE)
$(BUILDDIR)/xppautX$(EXE): $(SERVER_OBJECTS) $(CORELIB)
	$(LINK_X) $(SANITIZE) -o $@ $(SERVER_OBJECTS) $(CORELIB) -lm $(DLLIB) $(NETLIBS) $(WINDOW_LIBS)

# the one X11-free program: browser front end, --server protocol and -silent batch
xppautx: xppautX$(EXE)

$(CORELIB): $(CORE_OBJECTS)
	ar rcs $@ $(CORE_OBJECTS)

xppautX$(EXE): $(SERVER_OBJECTS) $(CORELIB)
	$(LINK_X) $(LDSTATIC) -o $@ $(SERVER_OBJECTS) $(CORELIB) -lm $(DLLIB) $(NETLIBS) $(WINDOW_LIBS)

# macOS: xppautX.app, a bundle Finder and LaunchServices know as the .ode
# opener (tools/associate/Info.plist.in's CFBundleDocumentTypes), from the
# built binary, assets/icon.icns (tools/make_icons.py) and that template
# (W13b; untested -- no macOS machine has built or run this bundle yet).
.PHONY: app
APP_DIR := xppautX.app
app: xppautx assets/icon.icns
	@mkdir -p $(APP_DIR)/Contents/MacOS $(APP_DIR)/Contents/Resources
	cp xppautX $(APP_DIR)/Contents/MacOS/xppautX
	cp assets/icon.icns $(APP_DIR)/Contents/Resources/icon.icns
	sed 's/@XPPAUTX_VERSION@/$(XPPAUTX_VERSION)/g' tools/associate/Info.plist.in > $(APP_DIR)/Contents/Info.plist
	@echo "app: wrote $(APP_DIR) (XPPAUTX_VERSION=$(XPPAUTX_VERSION))"

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

$(BUILDDIR)/tests/%.o: tests/%.c $(BUILDDIR)/toolchain.stamp | $(BUILDDIR)/tests
	$(CC) $(CFLAGS) -Itests -MMD -MP -c $< -o $@

$(BUILDDIR)/tests/%.o: tests/%.cpp $(BUILDDIR)/toolchain.stamp | $(BUILDDIR)/tests
	$(CXX) $(CXXFLAGS) -Itests -MMD -MP -MF $(@:.o=.cpp.d) -c $< -o $@

$(BUILDDIR)/tests:
	mkdir -p $@

$(BUILDDIR)/embed$(EXE): tools/embed.c | $(BUILDDIR)
	$(CC) -O2 -o $@ $<

$(BUILDDIR)/web_assets.c: $(BUILDDIR)/embed$(EXE) $(WEB2_FILES)
	$(BUILDDIR)/embed$(EXE) $@ $(WEB2_FILES)

$(BUILDDIR)/web_assets.o: $(BUILDDIR)/web_assets.c
	$(CC) -O2 -c $< -o $@

$(BUILDDIR)/embed_bytes$(EXE): tools/embed_bytes.c | $(BUILDDIR)
	$(CC) -O2 -o $@ $<

$(BUILDDIR)/icon_assets.c: $(BUILDDIR)/embed_bytes$(EXE) assets/icons/hicolor/256x256/apps/xppautx.png
	$(BUILDDIR)/embed_bytes$(EXE) $@ xpp_icon_png assets/icons/hicolor/256x256/apps/xppautx.png

$(BUILDDIR)/icon_assets.o: $(BUILDDIR)/icon_assets.c
	$(CC) -O2 -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(BUILDDIR)/toolchain.stamp | $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(BUILDDIR)/toolchain.stamp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.cpp.d) -c $< -o $@

$(BUILDDIR):
	mkdir -p $@

$(BUILDDIR)/version.stamp: FORCE | $(BUILDDIR)
	@echo '$(XPPAUTX_VERSION) $(XPPAUTX_COMMIT)' | cmp -s - $@ || echo '$(XPPAUTX_VERSION) $(XPPAUTX_COMMIT)' > $@

# the compilers and flags are an input of every object: a new gcc (objects,
# LTO bytecode) or a changed -std rebuilds them all instead of mixing
$(BUILDDIR)/toolchain.stamp: FORCE | $(BUILDDIR)
	@{ $(CC) --version | head -1; $(CXX) --version | head -1; echo '$(CFLAGS)'; echo '$(CXXFLAGS)'; echo 'WINDOW=$(WINDOW) $(WINDOW_CFLAGS)'; } > $@.tmp; 	if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi

FORCE:

# Dependency files: x.d for core/x.c, x.cpp.d for core/x.cpp, and only
# those of existing sources are read, so after `git mv x.c x.cpp` the
# stale x.d, which names core/x.c, does not stop the build.
depfiles = $(patsubst %.c,%.d,$(patsubst %.cpp,%.cpp.d,$(1)))
-include $(call depfiles,$(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES) $(SERVER_SOURCES)))
-include $(call depfiles,$(patsubst tests/%,$(BUILDDIR)/tests/%,$(TEST_SOURCES)))

clean:
	rm -rf $(BUILDDIR) libxppcore.a xppautX xppautX.exe
