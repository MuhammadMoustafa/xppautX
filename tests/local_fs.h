#pragma once
/* The disk for a test that loads a model from a file: the program's start makes one and passes it to the
   load (xpp_fs.h); a test makes it here, once. */
#include "xpp_fs_local.h"

inline xpp::FileSystem &local_fs()
{
    static xpp::LocalFileSystem fs;
    return fs;
}
