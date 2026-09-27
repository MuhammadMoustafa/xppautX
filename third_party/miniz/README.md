# miniz

`miniz.c` and `miniz.h` of Rich Geldreich's miniz, the amalgamated files of
release 3.0.2 (https://github.com/richgel999/miniz/releases/tag/3.0.2,
`miniz-3.0.2.zip`), unchanged. License: LICENSE (MIT).

Deflate/inflate, CRC-32 and zip archives in one C file, so xppautX needs no
system zlib (Windows has none). The Makefile builds it as its own object,
`miniz.o`, in the core library, with `MINIZ_NO_STDIO` (every file goes
through core/xpp_io and core/xpp_files) and `MINIZ_NO_TIME` (a zip written
twice is the same bytes). Only core/xpp_zip.cpp includes it: the rest of the
core uses that module's gzip and in-memory zip API (docs/roadmap.md W52; the
data formats, and the session file W57). It allocates its working memory
with the C library's malloc/free and hands none of it out (core/xpp_mem.h).
