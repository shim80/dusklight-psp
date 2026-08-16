#include "dusk/psp/startup_name_entry.hpp"

#include <cassert>
#include <cstring>
#include <cstdio>

int main() {
    using dusk::psp::startup::NameEntryKind;
    using dusk::psp::startup::NameEntryRuntime;

    NameEntryRuntime entry;
    entry.initialize(NameEntryKind::Player, "Link");
    assert(std::strcmp(entry.name(), "Link") == 0);
    assert(!entry.confirmed());

    entry.tick({.left = true});
    assert(entry.cursor_column() == 12);
    entry.tick({.down = true});
    entry.tick({.down = true});
    assert(entry.cursor_row() == 2);
    entry.tick({.confirm = true});
    assert(entry.confirmed());

    entry.initialize(NameEntryKind::Horse, "Epona");
    entry.tick({.erase = true});
    assert(std::strcmp(entry.name(), "Epon") == 0);
    entry.tick({.toggle_case = true});
    entry.tick({.right = true});
    entry.tick({.confirm = true});
    assert(std::strcmp(entry.name(), "Eponb") == 0);
    entry.tick({.finish = true});
    assert(entry.confirmed());

    entry.initialize(NameEntryKind::Player, "");
    entry.tick({.finish = true});
    assert(!entry.confirmed());
    assert(NameEntryRuntime::grid_character(0, 0, false) == 'A');
    assert(NameEntryRuntime::grid_character(1, 12, true) == 'z');
    assert(NameEntryRuntime::grid_character(2, 9, false) == '9');

    std::puts(
        "STARTUP_NAME_ENTRY_HOST_OK defaults=Link,Epona "
        "grid=letters,digits,space,delete,end");
    return 0;
}
