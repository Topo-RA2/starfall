# Persistence, Multiplayer, and Compatibility

[Manual home](../manual.md) · [Configuration reference](reference.md)

## Contents

- [Persistence, multiplayer, and compatibility](#persistence-multiplayer-and-compatibility)

## Persistence, multiplayer, and compatibility

### Save/load contract

Starfall stores extension data in the `Starfall.Passives` stream. The latest supported marker is `0x6B504653`, which preserves map-level native `Passengers.BySize` overrides used by transformation capacity checks. It is emitted when such overrides exist; other matches use the newest layout needed by their state. Marker `0x6A504653` stores ordinary research rounds and the shared rarity seed; earlier supported layouts remain readable. A newer marker requires a compatible reader. This document is not a full binary save-format specification; use the Writer/Reader/saveState/loadState implementation before editing save code.

The stream covers relevant passive counts/cooldowns/stacks and snapshots, grants and remaining durations, continuous state progress, extended health/stat changes, research ownership/points/offers/refreshes/building rewards, supported motion/animations, sequence/map context, Lua timers/state/RNG, and associated identifiers. Not every cache is serialized; derived presentation and lookup caches are rebuilt or cleared.

Loading validates and stages extension state before committing. An existing malformed extension stream is rejected rather than partially applied. Missing extension data is handled separately for older native saves. Some older Starfall layouts are supported, but no claim is made that every historical build or arbitrary changed configuration can load safely. Keep project/passive IDs stable and keep compatible scripts available for saved callbacks.

### Multiplayer contract

All peers must use the same Starfall DLL, gameplay rules, maps, scripts, and required animation/resources. Local UI selection sends commands that are revalidated in simulation; direct local input or drawing code must not mutate shared gameplay state.

Use `ctx:randomInt` and `ctx:randomPoint` for Lua randomness. Do not use external randomness, unordered iteration, clocks, files, or client-specific conditions for simulation decisions. Script errors retain earlier effects; identical error behavior is not a substitute for valid scripts.

The repository includes real two-client tests with frame-by-frame state comparisons, plus impaired-network and negative-divergence modes. These test specific scenarios and dependency combinations. They do not establish that every Lua example, arbitrary mod combination, long session, or multiplayer save/load path has been covered. Current recorded evidence is linked in the test section.

### Unit conversion and extension scope

For infantry, vehicles and aircraft, Starfall prefers the exported `ConvertToType_Phobos` interface. It uses built-in conversion when Phobos is absent, the export is missing, or the call returns `E_NOTIMPL` or `E_NOINTERFACE`. The built-in path checks its own type and instance restrictions before changing the unit. Other Phobos failures end the attempt. Building conversion uses the separate same-foundation path described above. Neither path requires Ares merely to perform conversion.

Mobile conversion requires different source/destination types of the same supported object class and matching Locomotor. It preserves identity, ownership, rank, passive state, proportional health, and extra maximum-health contribution where supported. A research conversion affects only current eligible infantry/vehicles, never buildings or future production. At least one successful conversion is required for a conversion-only purchase.

For mobile carriers, the native fallback uses the engine spawn manager for both custom aircraft and Ares `Missile.Custom=yes` missiles. When the child type stays the same, shared slots retain their aircraft, missions and running timers. Added slots start empty and regenerate after the new `SpawnRegenRate`. Future reloads use the new `SpawnReloadRate`. Shrinking retires trailing slots and their attached aircraft. Changing the child type creates a new group. Losing the carrier ability retires the entire group. Missiles already detached from their slots continue in flight.

The native fallback rejects slave managers and incompatible mind-control/parasite/chrono states. Cargo must fit the destination's passenger capacity and size constraints; occupied transports require compatible open-topped/gunner behavior. With Ares or Phobos loaded, destination `Passengers.BySize=no` counts each direct passenger as one slot; the default `yes` sums passenger sizes. `SizeLimit` applies to each passenger in either mode. Native map overrides are preserved in saves. Units must be alive, on-map, and valid for their destination terrain/state. Third-party type-attached data is not automatically compatible with every native conversion.

Mind-control immunity includes the supported native controller paths and the adapted Ares 3.0p1 permanent-control path. Unsupported entrypoints/versions are logged; this is not blanket compatibility with every Ares/Phobos build. Consult the current review records and test the exact loader/extensions used by a mod.
