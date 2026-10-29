#pragma once

/* W308's typed commands. The legacy menu/protocol tables remain until
   their resource cards move them; a command belongs to exactly one table. */
#include "xpp_error.h"
#include <string>
#include <string_view>

namespace xpp { struct Session; }
namespace xpp::command {

/* The registered command named by a JSON object or a server text line;
   empty when it is not registered. Safe on the inbox's reader thread. */
std::string_view name(const char *line);
char kind(const char *line);
/* Append registry rows to an existing JSON array (including a comma
   before each row). hello and help use the same metadata renderer. */
void append_commands(std::string &out);
/* Server text into a JSON object. Only this entry point accepts text;
   bound calls and recordings still supply JSON to run. */
Result<std::string> tokenize(const char *line, const Place &place);
/* paths_allowed: the line is --server's own input, the one origin a path argument may come from (not the page, HTTP, a recording or --silent's script). */
/* Validate all arguments before running. A success is the declared result
   as JSON; an error retains the command/recording's original place. */
Result<std::string> run(Session &s, const char *line, const Place &place, bool paths_allowed);
bool is_step(const char *line);
/* Imported/model-defined XPPAUT buttons use the same handlers. */
Result<> window_button(Session &s,int button,const Place &place);
Result<> nullcline_button(Session &s,int button,const Place &place);
Result<> dirfield_button(Session &s,int button,const Place &place);
Result<> run_button(Session &s,const std::string &line,const Place &place);
Result<> check_shape(const char *line,const Place &place);
/* One terminal event, using the common error renderer. Invalid ids are
   never echoed as nested data. */
/* job: the number of the job this done ends (xpp::job::number()), 0 for a command that is no job's own, such as a control
   command answered while a job runs */
std::string done(const char *line, const Result<std::string> &result, bool stopped, unsigned long job = 0);

} // namespace xpp::command
