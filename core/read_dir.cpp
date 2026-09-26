/* The file selector's directory listing (get_fileinfo), the current
   directory, and Unix-style wildcards (wild_match). */
#include "read_dir.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_io.h"
#include "load_eqn.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

/* Every buffer that holds a path name (cur_dir, the callers' of
   get_directory) is XPP_MAX_NAME bytes. */
#define MAXPATHLEN XPP_MAX_NAME

char cur_dir[MAXPATHLEN];

namespace {

/* the working directory, whatever its length ("" when it cannot be had) */
std::string working_directory()
{
  std::vector<char> buf(1024);
  for(;;){
    if(getcwd(buf.data(), buf.size()) != nullptr) return buf.data();
    if(errno != ERANGE) return {};
    buf.resize(buf.size() * 2);
  }
}

bool is_directory(std::string_view root, const char *name)
{
  std::string full(root);
  full += '/';
  full += name;
  struct stat statbuf;
  if(stat(full.c_str(), &statbuf)) /* some error: not a directory */
    return false;
  return (statbuf.st_mode & S_IFDIR) != 0;
}

/* FILEINFO is C API (the JSON front end's file selector reads it and
   free_finfo frees it): a raw xpp_malloc'd array of xpp_strdup'd names */
char **c_strings(const std::vector<std::string> &names)
{
  char **out = static_cast<char **>(xpp_malloc(names.size() * sizeof(char *)));
  for(size_t i = 0; i < names.size(); i++)
    out[i] = xpp_strdup(names[i].c_str());
  return out;
}

} // namespace

void free_finfo(FILEINFO *ff)
{
  for(int i = 0; i < ff->ndirs; i++)
    xpp_free(ff->dirnames[i]);
  xpp_free(ff->dirnames);
  for(int i = 0; i < ff->nfiles; i++)
    xpp_free(ff->filenames[i]);
  xpp_free(ff->filenames);
}

/* The directories of direct and its files that match wild, each list
   sorted. 0 (ff untouched) when direct cannot be read. */
int get_fileinfo(const char *wild, const char *direct, FILEINFO *ff)
{
  DIR *dirp = opendir(direct);
  if(dirp == nullptr){
    xpp_log(XPP_LOG_WARN, " %s is not a directory \n", direct);
    return 0;
  }
  std::vector<std::string> dirs, files;
  for(struct dirent *dp = readdir(dirp); dp != nullptr; dp = readdir(dirp)){
    if(is_directory(direct, dp->d_name))
      dirs.emplace_back(dp->d_name);
    else if(wild_match(dp->d_name, wild))
      files.emplace_back(dp->d_name);
  }
  closedir(dirp);
  std::sort(dirs.begin(), dirs.end());
  std::sort(files.begin(), files.end());
  ff->ndirs = static_cast<int>(dirs.size());
  ff->nfiles = static_cast<int>(files.size());
  ff->dirnames = c_strings(dirs);
  ff->filenames = c_strings(files);
  return 1;
}


int change_directory(const char *path)
{
    if (path == NULL) {
	*cur_dir = '\0';
	return (0);
    }
    if (chdir(path) == -1) {
	xpp_log(XPP_LOG_WARN, "Can't go to directory %s\n", path);
	return (1);
    }
    if (get_directory(cur_dir) != 0) /* get cwd */
	return (0);
    else
	return (1);
}

/* direct holds MAXPATHLEN bytes (every caller's buffer is XPP_MAX_NAME);
   a longer directory is cut, with a WARN, where getcwd(direct,1024)
   used to overflow it */
int get_directory(char *direct)
{
    std::string cwd = working_directory();
    if (cwd.empty()) {
	xpp_log(XPP_LOG_WARN, "%s\n", "Can't get current directory");
	*direct = '\0';
	return 0;
    }
    xpp_strlcpy(direct, cwd.c_str(), MAXPATHLEN);
    return 1;
}

/* wildmatch.c - Unix-style command line wildcards

   This procedure is in the public domain.

   After that, it is just as if the operating system had expanded the
   arguments, except that they are not sorted.	The program name and all
   arguments that are expanded from wildcards are lowercased.

   Syntax for wildcards:
   *		Matches zero or more of any character (except a '.' at
		the beginning of a name).
   ?		Matches any single character.
   [r3z]	Matches 'r', '3', or 'z'.
   [a-d]	Matches a single character in the range 'a' through 'd'.
   [!a-d]	Matches any single character except a character in the
		range 'a' through 'd'.

   The period between the filename root and its extension need not be
   given explicitly.  Thus, the pattern `a*e' will match 'abacus.exe'
   and 'axyz.e' as well as 'apple'.  Comparisons are not case sensitive.

   The wild_match code was written by Rich Salz, rsalz@bbn.com,
   posted to net.sources in November, 1986.

   The code connecting the two is by Mike Slomin, bellcore!lcuxa!mike2,
   posted to comp.sys.ibm.pc in November, 1988.

   Major performance enhancements and bug fixes, and source cleanup,
   by David MacKenzie, djm@ai.mit.edu. */

/* Shell-style pattern matching for ?, \, [], and * characters.
   I'm putting this replacement in the public domain.

   Written by Rich $alz, mirror!rs, Wed Nov 26 19:03:17 EST 1986. */

/* The character that inverts a character class; '!' or '^'. */
#define INVERT '!'


static int star(const char *string, const char *pattern);

/* Return nonzero if `string' matches Unix-style wildcard pattern
   `pattern'; zero if not. */

int wild_match(const char *string, const char *pattern)
{
    int		    prev;	/* Previous character in character class. */
    int		    matched;	/* If 1, character class has been matched. */
    int		    reverse;	/* If 1, character class is inverted. */

    for (; *pattern; string++, pattern++)
	switch (*pattern) {
	case '\\':
	    /* Literal match with following character; fall through. */
	    pattern++;
	default:
	    if (*string != *pattern)
		return 0;
	    continue;
	case '?':
	    /* Match anything. */
	    if (*string == '\0')
		return 0;
	    continue;
	case '*':
	    /* Trailing star matches everything. */
	    return *++pattern ? star(string, pattern) : 1;
	case '[':
	    /* Check for inverse character class. */
	    reverse = pattern[1] == INVERT;
	    if (reverse)
		pattern++;
	    for (prev = 256, matched = 0; *++pattern && *pattern != ']';
		 prev = *pattern)
		if (*pattern == '-'
		    ? *string <= *++pattern && *string >= prev
		    : *string == *pattern)
		    matched = 1;
	    if (matched == reverse)
		return 0;
	    continue;
	}

    return *string == '\0';
}

static int
star(const char *string, const char *pattern)
{
    while (wild_match(string, pattern) == 0)
	if (*++string == '\0')
	    return 0;
    return 1;
}




