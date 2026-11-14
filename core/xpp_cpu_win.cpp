/* xpp_cpu.h's physical_cores on Windows: the processor topology of xpp_win.cpp. */
#include "xpp_cpu.h"
#include "xpp_win.h"

namespace xpp::cpu {

std::optional<unsigned> physical_cores() { return win::processor_core_count(); }

}
