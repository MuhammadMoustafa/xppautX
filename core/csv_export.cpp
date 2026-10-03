#include "browse.h"
/* CSV exports beside the old whitespace formats (W26, issue #42): a
   header row of names, "\n" line ends, full precision, that
   pandas.read_csv and MATLAB readtable read with no options. See
   csv_export.h. */
#include "model.h"
#include "session.h"
#include "csv_export.h"
#include "data_formats.h"
#include "xpp_ui.h"
#include "diagram.h"
#include "autevd.h"
#include "form_ode.h"
#include <string>

namespace {

/* the model parameter name of the diagram point's active parameter icp
   (AUTO's own parameter index, d->icp1/icp2), or "" */
const char *par_name(const xpp::Session &s, int icp)
{
    const char *p = auto_par_name(s, icp);
    return p ? p : "";
}

/* AUTO's ibr and ntot without their sign (auto_f2c.h's abs macro rules
   out std::abs here) */
int unsigned_of(int v) { return v < 0 ? -v : v; }

xpp::Result<bool> write_table(const xpp::DataTable &table, const char *filename)
{
    const xpp::DataFormat &csv = *xpp::data_format_named("csv");
    xpp::Result<> opened;
    xpp::Writer w = xpp::open_writer_asking(filename, csv.binary, &opened);
    if (!opened) return std::unexpected(opened.error());
    if (!w) return false;
    if (!csv.write(table, w)) {
        xpp::abort_save(w);
        return xpp::fail_reading("CSV export", "cannot be written", filename);
    }
    if (const xpp::Result<> saved = xpp::commit_save(w); !saved) return std::unexpected(saved.error());
    return true;
}

} // namespace

xpp::Result<bool> csv_export_diagram(const xpp::Session &s, const char *filename)
{
    const xpp::Model &m = s.model();
    if (!xpp::save_ready(s.diagram.points.size() >= 2)) return false; /* nothing recorded */
    xpp::DataTable table;
    table.names = {"branch", "point", "type", "label", "stability", "f2", "param1_name", "param1", "param2_name", "param2", "period"};
    for (int i = 0; i < m.node; i++) table.names.push_back(xpp::format("{}_max", m.uvar_names[i]));
    for (int i = 0; i < m.node; i++) table.names.push_back(xpp::format("{}_min", m.uvar_names[i]));
    table.fields.resize(table.names.size());
    /* the first point is a stored point itself (edit_start fills it in
       place), not a sentinel before one: write_info_out/write_pts start
       the same way */
    for (const DiagramPoint &p : s.diagram.points) {
        const xpp::DIAGRAM *d = &p.d;
        int type = xpp::get_bif_type(d->ibr, d->ntot, d->lab);
        const char *sym = xpp::auto_bif_sym(d->itp);
        while (*sym == ' ') sym++;
        double par1 = d->par[d->icp1];
        double par2 = d->icp2 < s.auto_state.npar ? d->par[d->icp2] : par1;
        /* AUTO signs ibr and ntot by stability, which has its own column */
        std::vector<std::string> row = {xpp::format("{}", unsigned_of(d->ibr)), xpp::format("{}", unsigned_of(d->ntot)),
            sym, xpp::format("{}", d->lab), xpp::point_is_stable(type) ? "stable" : "unstable",
            xpp::format("{}", d->flag2), par_name(s, d->icp1), xpp::number(par1), par_name(s, d->icp2),
            xpp::number(par2), xpp::number(d->per)};
        for (int i = 0; i < m.node; i++) row.push_back(xpp::number(d->uhi[i]));
        for (int i = 0; i < m.node; i++) row.push_back(xpp::number(d->ulo[i]));
        for (std::size_t i = 0; i < row.size(); i++) table.fields[i].push_back(std::move(row[i]));
    }
    return write_table(table, filename);
}

xpp::Result<bool> csv_export_diagram_eigenvalues(const xpp::Session &s, const char *filename)
{
    if (!xpp::save_ready(s.diagram.points.size() >= 2)) return false;
    xpp::DataTable table;
    table.names = {"branch", "point", "index", "re", "im", "kind"};
    table.fields.resize(table.names.size());
    for (const DiagramPoint &p : s.diagram.points) {
        const xpp::DIAGRAM *d = &p.d;
        int type = xpp::get_bif_type(d->ibr, d->ntot, d->lab);
        const char *kind = xpp::point_is_periodic(type) ? "multiplier" : "eigenvalue";
        for (int i = 0; i < s.model().node; i++) {
            const std::vector<std::string> row = {xpp::format("{}", unsigned_of(d->ibr)), xpp::format("{}", unsigned_of(d->ntot)),
                xpp::format("{}", i), xpp::number(d->evr[i]), xpp::number(d->evi[i]), kind};
            for (std::size_t j = 0; j < row.size(); j++) table.fields[j].push_back(row[j]);
        }
    }
    return write_table(table, filename);
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
