#ifndef DUSK_PSP_CANONICAL_STARTUP_LOADER_HPP
#define DUSK_PSP_CANONICAL_STARTUP_LOADER_HPP

#include "dusk/psp/canonical_assets.hpp"
#include "dusk/psp/playable_package.hpp"
#include "dusk/psp/room_package.hpp"
#include "dusk/psp/startup_camera_track.hpp"
#include "dusk/psp/startup_package.hpp"
#include "dusk/psp/startup_ui_package.hpp"

#include <cstdint>

namespace dusk::psp::game {

struct OwnedStartupSequence {
    std::uint8_t* bytes;
    std::uint32_t size;
    startup::PackageView view;
};

struct OwnedStartupUi {
    std::uint8_t* bytes;
    std::uint32_t size;
    startup::UiPackageView view;
};

struct OwnedStartupRoomPackage {
    std::uint8_t* bytes;
    std::uint32_t size;
    room::PackageView view;
};

struct OwnedStartupAnimation {
    std::uint8_t* bytes;
    std::uint32_t size;
    playable::PackageView view;
};

struct CanonicalStartupPackages {
    OwnedStartupSequence sequence;
    OwnedStartupUi logos_ui;
    OwnedStartupUi title_ui;
    OwnedStartupUi file_select_ui;
    OwnedStartupRoomPackage title_room_model;
    OwnedStartupRoomPackage title_room_textures;
    OwnedStartupRoomPackage title_logo_model;
    OwnedStartupRoomPackage title_logo_textures;
    OwnedStartupAnimation title_logo_animation;
};

struct CanonicalIntroPackages {
    OwnedStartupRoomPackage rusl_wide_model;
    OwnedStartupRoomPackage rusl_closeup_model;
    OwnedStartupRoomPackage rusl_textures;
    OwnedStartupAnimation link_wide_animation;
    OwnedStartupAnimation link_closeup_animation;
};

enum class CanonicalStartupLoadError : std::uint8_t {
    Ok = 0,
    InvalidOutput,
    AssetContract,
    Path,
    FileSize,
    Allocation,
    Read,
    SequencePackage,
    LogosUiPackage,
    TitleUiPackage,
    FileSelectUiPackage,
    TitleRoomModelPackage,
    TitleRoomTexturePackage,
    TitleLogoModelPackage,
    TitleLogoTexturePackage,
    TitleLogoAnimationPackage,
    Demo01RuslWideModelPackage,
    Demo01RuslCloseupModelPackage,
    Demo01RuslTexturePackage,
    Demo01LinkWideAnimationPackage,
    Demo01LinkCloseupAnimationPackage,
};

CanonicalStartupLoadError load_canonical_startup_packages(
    CanonicalStartupPackages* packages);
CanonicalStartupLoadError load_canonical_file_select_ui(
    OwnedStartupUi* package);
CanonicalStartupLoadError load_canonical_intro_packages(
    CanonicalIntroPackages* packages);
void unload_canonical_startup_ui(OwnedStartupUi* package);
void unload_canonical_intro_packages(CanonicalIntroPackages* packages);
void unload_canonical_startup_packages(CanonicalStartupPackages* packages);
const char* canonical_startup_load_error_name(CanonicalStartupLoadError error);

}  // namespace dusk::psp::game

#endif
