#ifndef DUSK_PSP_STARTUP_ROUTE_CAPTURE_HPP
#define DUSK_PSP_STARTUP_ROUTE_CAPTURE_HPP

namespace dusk::psp::game {

bool startup_route_capture_enabled();
bool capture_startup_route_frame(const char* leaf);
bool complete_startup_route_capture();

}  // namespace dusk::psp::game

#endif
