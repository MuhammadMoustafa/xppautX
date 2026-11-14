/* xpp_cpu.h's physical_cores on Windows: the processor topology of xpp_win32.cpp. */
#include "xpp_cpu.h"
#include "xpp_win32.h"

namespace xpp::cpu {

std::optional<unsigned> physical_cores() { return win32::processor_core_count(); }

}
