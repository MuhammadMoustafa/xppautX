/* CSV exports beside the old whitespace formats (W26, issue #42): a
   header row of names, "\n" line ends, full precision, that
   pandas.read_csv and MATLAB readtable read with no options. See
   csv_export.h. */
#include "csv_export.h"
#include "xpp_io.h"
#include "diagram.h"
#include "autevd.h"
#include "auto_nox.h"
#include "auto_settings.h"
#include "pop_list.h"
#include "xpp_ui.h"
#include <string>
#include <cmath>
#include <cstdio>
#include <cstring>

/* the core's own globals that have no header of their own (diagram.cpp,
   auto_settings.cpp precedent) */
extern "C" {
extern DIAGRAM *bifd;
extern int NODE;
extern int NAutoPar;
extern int AutoPar[8];
extern char upar_names[MAXPAR][XPP_NAME_MAX + 1];
}

namespace {

/* err_msg (xpp_ui.h) takes char * and does not write through it, the
   historical C dialog API (diagram.cpp/browse_data.cpp precedent) */
char *str(const char *s) { return const_cast<char *>(s); }

/* a CSV field, quoted only if it needs it (a model name cannot contain a
   comma, quote or newline, but this is the one place that writes names a
   user typed, so it is safe rather than assuming) */
std::string csv_field(const char *s)
{
    if (!s) return std::string();
    bool needs = false;
    for (const char *p = s; *p; p++)
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') { needs = true; break; }
    if (!needs) return std::string(s);
    std::string q = "\"";
    for (const char *p = s; *p; p++) {
        if (*p == '"') q += '"';
        q += *p;
    }
    q += '"';
    return q;
}

/* the model parameter name of the diagram point's active parameter icp
   (AutoPar[icp] is the model's own parameter index), or "" */
std::string par_name(int icp)
{
    /* AutoPar[icp] (icp: AUTO's own parameter index, d->icp1/icp2) is the
       model's own parameter index (auto_settings.cpp auto_par_name's
       rule, reimplemented here: that one has internal linkage) */
    if (icp < 0 || icp >= NAutoPar) return "";
    int p = AutoPar[icp];
    if (p < 0 || p >= NUPAR) return "";
    return upar_names[p];
}

/* get_bif_type's SEQ/UEQ/SPER/UPER (autevd.cpp; also a run's "ty" in
   docs/protocol.md's `info`): 1/2 a steady state's eigenvalues, 3/4 a
   periodic orbit's Floquet multipliers; odd stable, even unstable */
/* AUTO's ibr and ntot without their sign (auto_f2c.h's abs macro rules
   out std::abs here) */
int unsigned_of(int v) { return v < 0 ? -v : v; }
bool point_is_periodic(int type) { return type == 3 || type == 4; }
bool point_is_stable(int type) { return type == 1 || type == 3; }

} // namespace

int csv_export_diagram(const char *filename)
{
    if (bifd == NULL || bifd->next == NULL) return 0; /* nothing recorded */
    xpp::Writer w(filename);
    if (!w) {
        err_msg(str("Can't open file"));
        return 0;
    }
    FILE *fp = w.file();
    fprintf(fp, "branch,point,type,label,stability,f2,param1_name,param1,param2_name,param2,period");
    for (int i = 0; i < NODE; i++) fprintf(fp, ",%s_max", uvar_names[i]);
    for (int i = 0; i < NODE; i++) fprintf(fp, ",%s_min", uvar_names[i]);
    fprintf(fp, "\n");
    /* bifd is the first stored point itself (edit_start fills it in
       place), not a sentinel before one: write_info_out/write_pts start
       the same way */
    for (const DIAGRAM *d = bifd; d != NULL; d = d->next) {
        int type = get_bif_type(d->ibr, d->ntot, d->lab);
        char symb[3];
        get_bif_sym(symb, d->itp);
        const char *sym = symb;
        while (*sym == ' ') sym++;
        double par1 = d->par[d->icp1];
        double par2 = d->icp2 < NAutoPar ? d->par[d->icp2] : par1;
        /* AUTO signs ibr and ntot by stability, which has its own column */
        fprintf(fp, "%d,%d,%s,%d,%s,%d,%s,%s,%s,%s,%s", unsigned_of(d->ibr), unsigned_of(d->ntot), csv_field(sym).c_str(), d->lab,
                point_is_stable(type) ? "stable" : "unstable", d->flag2, csv_field(par_name(d->icp1).c_str()).c_str(),
                xpp::number(par1).c_str(), csv_field(par_name(d->icp2).c_str()).c_str(), xpp::number(par2).c_str(),
                xpp::number(d->per).c_str());
        for (int i = 0; i < NODE; i++) fprintf(fp, ",%s", xpp::number(d->uhi[i]).c_str());
        for (int i = 0; i < NODE; i++) fprintf(fp, ",%s", xpp::number(d->ulo[i]).c_str());
        fprintf(fp, "\n");
    }
    if (!w.commit()) {
        err_msg(str("Can't open file"));
        return 0;
    }
    return 1;
}

int csv_export_diagram_eigenvalues(const char *filename)
{
    if (bifd == NULL || bifd->next == NULL) return 0;
    xpp::Writer w(filename);
    if (!w) {
        err_msg(str("Can't open file"));
        return 0;
    }
    FILE *fp = w.file();
    fprintf(fp, "branch,point,index,re,im,kind\n");
    for (const DIAGRAM *d = bifd; d != NULL; d = d->next) {
        int type = get_bif_type(d->ibr, d->ntot, d->lab);
        const char *kind = point_is_periodic(type) ? "multiplier" : "eigenvalue";
        for (int i = 0; i < NODE; i++)
            fprintf(fp, "%d,%d,%d,%s,%s,%s\n", unsigned_of(d->ibr), unsigned_of(d->ntot), i, xpp::number(d->evr[i]).c_str(),
                    xpp::number(d->evi[i]).c_str(), kind);
    }
    if (!w.commit()) {
        err_msg(str("Can't open file"));
        return 0;
    }
    return 1;
}

int csv_export_diagram_pair(const char *filename)
{
    if (!csv_export_diagram(filename)) return 0;
    std::string eig(filename);
    size_t slash = eig.find_last_of("/\\");
    size_t dot = eig.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        eig.insert(dot, "_eig");
    else
        eig += "_eig.csv";
    csv_export_diagram_eigenvalues(eig.c_str());
    return 1;
}
