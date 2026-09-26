#ifndef _read_dir_h_
#define _read_dir_h_
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
int wild_match(const char *string, const char *pattern);



#ifdef __cplusplus
}
#endif
#endif
