#ifndef _lunch_new_h_
#define _lunch_new_h_


#include <stdio.h>
#include "form_ode.h"
#ifdef __cplusplus
extern "C" {
#endif



void file_inf(void);
void ps_write_pars(FILE *fp);
void do_info(FILE *fp);
int read_lunch(FILE *fp);
void write_lunch(FILE *fp);
void do_lunch(int f);
void dump_eqn(FILE *fp);
void io_numerics(int f, FILE *fp);
void io_parameter_file(const char *fn, int flag);
void io_ic_file(const char *fn, int flag);
void io_parameters(int f, FILE *fp);
void io_exprs(int f, FILE *fp);
void io_graph(int f, FILE *fp);


#ifdef __cplusplus
}

#include <string>
#include <string_view>
/* a number of a set file (f READEM), or z written with its name ss */
void io_int(int *i, FILE *fp, int f, std::string_view ss);
void io_double(double *z, FILE *fp, int f, std::string_view ss);
/* one line of a set file into s, whole (f READEM), or s written as one */
void io_string(std::string &s, FILE *fp, int f);
/* the values panel's Save/Load of .par and .ic (docs/protocol.md
   "values"), through io_parameter_file/io_ic_file: name empty asks for
   one like Save data does, given skips the ask */
void save_parameter_file(std::string name);
void save_ic_file(std::string name);
void load_parameter_file(std::string name);
void load_ic_file(std::string name);
/* the model's internal sets (name, whether -silent runs it, what it
   sets), its parameters' values in the file and its initial conditions,
   those asked for, each under its # heading, into name (-silent's
   -qsets/-qpars/-qics, the protocol's `values` `query`) */
void write_values_query(const char *name, bool sets, bool pars, bool ics);
#endif
#endif
