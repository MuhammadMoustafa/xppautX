#ifndef _csv_export_h_
#define _csv_export_h_
#ifdef __cplusplus
extern "C" {
#endif

/* CSV exports pandas.read_csv and MATLAB readtable read with no options:
   a header row of names, "\n" line ends, full precision (W26, issue #42).
   Beside the old whitespace formats (Write pts/diagram.dat, All info's
   allinfo.dat, Save/Load diagram): a new export, not a replacement.

   Writes filename as CSV, one row per stored diagram point: branch,
   point, type (the symbol get_bif_sym gives: EP, HB, LP, ... or empty),
   label, stability ("stable"/"unstable"), the point's curve kind (f2, as
   write_pts/write_info_out carry it), the active parameter(s) (named,
   value), the period and the plotted/state values write_info_out also
   carries (every variable's max and min over the point, named by the
   model's own variable names) -- everything write_info_out's allinfo.dat
   carries except the eigenvalues/multipliers, which
   csv_export_diagram_eigenvalues below carries in a second file, not
   packed into cells. 1 on success (the diagram had points and the file
   was written; a failure to open logs err_msg itself), 0 when there was
   nothing to write. */
int csv_export_diagram(const char *filename);

/* Writes filename as CSV keyed by branch and point: one row per
   eigenvalue (a steady state) or Floquet multiplier (a periodic orbit) of
   every stored diagram point -- branch, point, index, re, im, kind
   ("eigenvalue"/"multiplier"). The values are the diagram's own copy of
   what core/auto_stability.cpp computed for that point when it was
   stored (autevd.cpp addbif, auto_stability_for), the one source of them;
   zeros mean "not computed", as elsewhere. 1 on success, 0 when there was
   nothing to write. */
int csv_export_diagram_eigenvalues(const char *filename);

/* Both of the above from one file dialog answer: filename for the
   diagram, and filename with "_eig" inserted before its extension (or
   appended, no extension) for the eigenvalues. Returns 1 if the diagram
   file was written (the eigenvalues file follows unconditionally from
   the same data). */
int csv_export_diagram_pair(const char *filename);

#ifdef __cplusplus
}
#endif
#endif
