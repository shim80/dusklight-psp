# Closed Link scenario host regressions

## Result

The existing targeted host guards for `link_idle_full_cycle` and
`link_ground_contact` were rebuilt and executed against the current canonical
Link packages. No PPSSPP run and no ten-scenario reacquisition was performed.

Results:

- `IDLE_FOOT_SLIP_HOST_OK`: 180 frames, zero actor-world drift, visual glide
  false;
- `LINK_GROUNDING_PARITY_OK`: 766 frames, four negative scenarios, zero
  allocations during update;
- `LINK_GROUNDING_REFERENCE_OK`: 47 sole vertices per side and the real
  18–21 / 23–26 leg chains;
- `LINK_ROOT_ANCHOR_REFERENCE_OK`: 586 frames, zero vertical anchor error,
  stable actor origin, collision/model origin parity true, zero allocations
  during update.

These tests preserve the two already closed Link scenarios while causal work
continues on the eight mobile scenarios. They do not prove the unresolved
moving grounding behavior and are not used to hide its divergence.
