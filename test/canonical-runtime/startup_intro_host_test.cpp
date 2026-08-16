#include "dusk/psp/startup_intro.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace {

bool near(float value, float expected) {
    return std::fabs(value - expected) < 0.0005f;
}

void require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

}  // namespace

int main() {
    using dusk::psp::startup::NewGameIntroPhase;
    using dusk::psp::startup::NewGameIntroRuntime;

    NewGameIntroRuntime intro;
    intro.initialize(true);
    require(intro.phase() == NewGameIntroPhase::Wide);
    require(std::strstr(intro.message(), "world intersects") != nullptr);
    const auto* shot = intro.shot();
    require(shot != nullptr);
    require(shot->source_message_index == 1512);
    require(shot->source_message_id == 3002);
    require(shot->source_timeline_frame == 270);
    require(shot->link_body_bck_id == 3);
    require(shot->rusl_model_id == 47);
    require(shot->rusl_body_bck_id == 18);
    require(shot->rusl_btk_id == 31);
    require(shot->rusl_btp_id == 41);
    require(near(shot->link_animation_frame, 30.0f));
    require(near(shot->rusl_animation_frame, 30.0f));
    require(near(shot->camera_eye.x, -16537.84375f));
    require(near(shot->camera_center.z, -4432.334961f));
    require(near(shot->camera_fov, 30.128679f));
    require(near(shot->actor_translation.x, -17320.0f));
    require(near(shot->actor_rotation_degrees.y, -115.0f));
    intro.tick({.advance = true});
    require(intro.phase() == NewGameIntroPhase::Closeup);
    require(std::strstr(intro.message(), "hour of twilight") != nullptr);
    shot = intro.shot();
    require(shot != nullptr);
    require(shot->source_message_index == 1514);
    require(shot->source_message_id == 3004);
    require(shot->source_timeline_frame == 482);
    require(shot->link_body_bck_id == 4);
    require(shot->rusl_body_bck_id == 19);
    require(near(shot->camera_eye.x, -17422.097656f));
    require(near(shot->camera_center.y, 19.296864f));
    require(near(shot->camera_fov, 23.482006f));
    intro.tick({.advance = true});
    require(intro.complete());
    require(intro.shot() == nullptr);

    intro.initialize(true);
    intro.tick({.skip = true});
    require(intro.complete());

    intro.initialize(true);
    for (std::uint32_t frame = 0;
         frame < NewGameIntroRuntime::kMaximumPhaseFrames; ++frame) {
        intro.tick({});
    }
    require(intro.phase() == NewGameIntroPhase::Closeup);

    intro.initialize(false);
    require(intro.complete());
    std::puts(
        "STARTUP_INTRO_HOST_OK phases=wide,closeup,complete "
        "advance=input_or_270_frames source=demo01_01.stb "
        "actors=Link,Rusl cameras=wide,rusl_closeup");
    return 0;
}
