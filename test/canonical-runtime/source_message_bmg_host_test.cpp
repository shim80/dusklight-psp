#include "dusk/psp/source_message_bmg.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<unsigned char> bytes{
        std::istreambuf_iterator<char>(input), {}};
    dusk::psp::message::SourceBmgDatabase db;
    dusk::psp::message::SourceMessageText message = {};
    if (bytes.empty() || !db.initialize(bytes.data(), bytes.size()) ||
        !db.extract(0x86, &message)) return 1;
    const std::string expected =
        "You got a Piece of Heart!\n"
        "Collect 5 pieces to form a new\n"
        "Heart Container and increase\n"
        "your life energy!";
    if (message.text != expected || message.pause_count != 1 ||
        message.pauses[0].frames != 10 || message.pauses[0].offset != 26 ||
        message.type_count != 1 || message.highlight_count != 2 ||
        message.unknown_controls != 0 || message.truncated) return 3;
    if (message.highlights[0].begin != 10 ||
        message.highlights[0].end != 24 ||
        message.highlights[1].begin == message.highlights[1].end) return 4;
    dusk::psp::message::SourceMessageRuntime runtime;
    if (!runtime.begin(&db, 0x86) || runtime.revealed_characters() != 26 ||
        !runtime.tick(false) || !runtime.active() || runtime.acknowledged()) return 5;
    for (int i = 0; i < 10; ++i) if (!runtime.tick(false)) return 6;
    if (runtime.revealed_characters() <= 26) return 7;
    // First press reveals the remainder, second press closes.
    if (!runtime.tick(true) || !runtime.active() || !runtime.awaiting_confirm() ||
        runtime.acknowledged() || !runtime.tick(true) || runtime.active() ||
        !runtime.acknowledged()) return 8;
    std::printf(
        "SOURCE_MESSAGE_BMG_HOST_OK id=0x%X chars=%u pauses=%u "
        "highlights=%u pause_frames=10 exact_text=1 input_gated=1\n",
        message.id, message.length, message.pause_count,
        message.highlight_count);
    return 0;
}
