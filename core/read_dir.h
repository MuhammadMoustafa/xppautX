#ifndef _read_dir_h_
#define _read_dir_h_
#include "load_eqn.h" /* XPP_MAX_NAME */
#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  char **dirnames,**filenames;
  int nfiles,ndirs;
} FILEINFO;


void free_finfo(FILEINFO *ff);
int get_fileinfo(const char *wild, const char *direct, FILEINFO *ff);
int change_directory(const char *path);
/* direct: an XPP_MAX_NAME buffer */
int get_directory(char *direct);
/* the file selector's folder (XPP_MAX_NAME bytes, get_directory fills it) */
extern char cur_dir[XPP_MAX_NAME];
int wild_match(const char *string, const char *pattern);



#ifdef __cplusplus
}

#include <string>
#include <vector>

/* the working directory, whatever its length ("" when it cannot be had) */
std::string current_directory();
/* the folders in direct and its files that match wild, each list sorted;
   false (with a WARN) when direct cannot be read */
bool list_folder(const char *wild, const char *direct,
                 std::vector<std::string> &dirs, std::vector<std::string> &files);
#endif
#endif
