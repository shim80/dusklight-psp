# 195 — Source door and room handoff summary

Classification: `P1_DOOR_HANDOFF_EVIDENCE_SUMMARY`

Prior P1 validation established that the port can execute source-derived door behavior and perform real room replacement rather than leaving a gray background behind an open door.

The intended architecture separates:

- source event/cut dependencies;
- original actor animation/collision lifetime;
- Link event movement/animation;
- transactional room resource replacement;
- return to a clean ordinary locomotion pose.

This remains the pattern for chests, cinematics and later dungeon transitions: no stale actor/collision/render handles may survive a room lifetime boundary.
