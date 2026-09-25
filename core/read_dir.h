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
int cmpstringp(const void *p1, const void *p2);
int get_fileinfo(const char *wild, const char *direct, FILEINFO *ff);
int fil_count(const char *direct, int *ndir, int *nfil, const char *wild, int *mld, int *mlf);
int change_directory(const char *path);
int get_directory(char *direct);
int IsDirectory(const char *root, const char *path);
void MakeFullPath(const char *root, const char *filename, char *pathname);
int wild_match(const char *string, const char *pattern);



#ifdef __cplusplus
}
#endif
#endif
