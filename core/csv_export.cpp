/* CSV exports beside the old whitespace formats (W26, issue #42): a
   header row of names, "\n" line ends, full precision, that
   pandas.read_csv and MATLAB readtable read with no options. See
   csv_export.h. */
#include "model.h"
#include "session.h"
#include "csv_export.h"
#include "xpp_io.h"
#include "diagram.h"
#include "autevd.h"
#include "form_ode.h"
#include <string>

namespace {

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
   (AUTO's own parameter index, d->icp1/icp2), or "" */
const char *par_name(const xpp::Session &s, int icp)
{
    const char *p = auto_par_name(s, icp);
    return p ? p : "";
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

xpp::Result<bool> csv_export_diagram(const xpp::Session &s, const char *filename)
{
    const xpp::Model &m = s.model();
    if (s.diagram.points.size() < 2) return false; /* nothing recorded */
    xpp::Writer w(filename);
    if (!w) return xpp::fail("CSV export", "Can't open file");
    w.print("branch,point,type,label,stability,f2,param1_name,param1,param2_name,param2,period");
    for (int i = 0; i < m.node; i++) w.print(",{}_max", m.uvar_names[i]);
    for (int i = 0; i < m.node; i++) w.print(",{}_min", m.uvar_names[i]);
    w.print("\n");
    /* the first point is a stored point itself (edit_start fills it in
       place), not a sentinel before one: write_info_out/write_pts start
       the same way */
    for (const DiagramPoint &p : s.diagram.points) {
        const DIAGRAM *d = &p.d;
        int type = get_bif_type(d->ibr, d->ntot, d->lab);
        const char *sym = auto_bif_sym(d->itp);
        while (*sym == ' ') sym++;
        double par1 = d->par[d->icp1];
        double par2 = d->icp2 < s.auto_state.npar ? d->par[d->icp2] : par1;
        /* AUTO signs ibr and ntot by stability, which has its own column */
        w.print("{},{},{},{},{},{},{},{},{},{},{}", unsigned_of(d->ibr), unsigned_of(d->ntot), csv_field(sym), d->lab,
                point_is_stable(type) ? "stable" : "unstable", d->flag2, csv_field(par_name(s, d->icp1)),
                xpp::number(par1), csv_field(par_name(s, d->icp2)), xpp::number(par2), xpp::number(d->per));
        for (int i = 0; i < m.node; i++) w.print(",{}", xpp::number(d->uhi[i]));
        for (int i = 0; i < m.node; i++) w.print(",{}", xpp::number(d->ulo[i]));
        w.print("\n");
    }
    if (!w.commit()) return xpp::fail("CSV export", "Can't open file");
    return true;
}

xpp::Result<bool> csv_export_diagram_eigenvalues(const xpp::Session &s, const char *filename)
{
    if (s.diagram.points.size() < 2) return false;
    xpp::Writer w(filename);
    if (!w) return xpp::fail("CSV export", "Can't open file");
    w.print("branch,point,index,re,im,kind\n");
    for (const DiagramPoint &p : s.diagram.points) {
        const DIAGRAM *d = &p.d;
        int type = get_bif_type(d->ibr, d->ntot, d->lab);
        const char *kind = point_is_periodic(type) ? "multiplier" : "eigenvalue";
        for (int i = 0; i < s.model().node; i++)
            w.print("{},{},{},{},{},{}\n", unsigned_of(d->ibr), unsigned_of(d->ntot), i, xpp::number(d->evr[i]),
                    xpp::number(d->evi[i]), kind);
    }
    if (!w.commit()) return xpp::fail("CSV export", "Can't open file");
    return true;
}

xpp::Result<bool> csv_export_diagram_pair(const xpp::Session &s, const char *filename)
{
    const xpp::Result<bool> written = csv_export_diagram(s, filename);
    if (!written || !*written) return written;
    std::string eig(filename);
    size_t slash = eig.find_last_of("/\\");
    size_t dot = eig.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        eig.insert(dot, "_eig");
    else
        eig += "_eig.csv";
    return csv_export_diagram_eigenvalues(s, eig.c_str());
}
