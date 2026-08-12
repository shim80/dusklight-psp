#ifndef DUSK_PSP_PRESENTATION_PROFILE_HPP
#define DUSK_PSP_PRESENTATION_PROFILE_HPP

#include <cstdint>
#include <cstring>

namespace dusk::psp::presentation {

enum class Profile : std::uint8_t {
    Game,
    Debug,
    OpaqueOnly,
    Invalid,
};

inline Profile parse(const char* name) {
    if (name == nullptr || std::strcmp(name, "game") == 0) {
        return Profile::Game;
    }
    if (std::strcmp(name, "debug") == 0) {
        return Profile::Debug;
    }
    if (std::strcmp(name, "opaque_only") == 0) {
        return Profile::OpaqueOnly;
    }
    return Profile::Invalid;
}

inline const char* name(Profile profile) {
    return profile == Profile::Game ? "game" :
           profile == Profile::Debug ? "debug" :
           profile == Profile::OpaqueOnly ? "opaque_only" : "invalid";
}

inline bool debug_visuals(Profile profile) {
    return profile == Profile::Debug;
}

inline bool opaque_only(Profile profile) {
    return profile == Profile::OpaqueOnly;
}

}  // namespace dusk::psp::presentation

#endif
