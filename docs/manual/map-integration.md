# Map Triggers and Team Scripts

English reference for map authors, mod authors, AI assistants, and maintainers. Actions below require Starfall and a correctly configured native map trigger or team script.

[Manual home](../manual.md) · [Lua guide](lua.md) · [Configuration reference](reference.md)

## Contents

- [Quick start](#quick-start)
- [Map action numbers](#map-action-numbers)
- [Grant presets](#grant-presets)
- [Map Lua](#map-lua)
- [Map unit bindings](#map-unit-bindings)
- [Diagnostics and multiplayer](#diagnostics-and-multiplayer)

## Quick start

The following fragment awards 90 research points to the player in starting position 1. Add it as an action record to a configured trigger and tag. The record below is a fragment. Update the action count and use the trigger identifier from your map.

```ini
[Actions]
0F510005=1,9600,0,0,90,4475,0,0,A
```

Start the map in the selected position and fire the trigger. Check the research balance in the top-right research interface. The repository includes a [research points test map](../../tests/maps/Starfall_ResearchPoints.map) with trigger and tag setup.

### Map action numbers

Each example below is one action under an existing trigger's `[Actions]` entry. Replace the trigger ID, preserve the engine's action-record layout, and update the leading action count when merging multiple actions.

| Action | Parameter 3 | Parameter 4 | Purpose |
| --- | --- | --- | --- |
| `9600` | Positive point amount | House selector | Award research points. |
| `9601` | Grant preset number | Recipient selector | Apply one grant preset. |
| `9602` | Map-script preset number | House selector | Execute one map Lua callback. |

House selectors: `0` is trigger owner; `4475..4482` are player starting positions 1..8. Grant recipient selectors additionally accept `-1` for the trigger's associated object and `-2` for on-map units whose tags reference this trigger. For grants, house selectors select that house's current on-map units.

```ini
[Actions]
0F510005=1,9600,0,0,90,4475,0,0,A
0F510006=1,9601,0,0,1,4475,0,0,A
0F510007=1,9602,0,0,1,4475,0,0,A
```

These are three separate example trigger records, not a complete map. See [the research test map](../../tests/maps/Starfall_ResearchPoints.map) for event/tag setup.

Team script action `9601` takes only the preset number and affects current eligible team members, then advances to the next line:

```ini
[MyTeamScript]
0=9601,1
```

### Grant presets

**Fragment**, assuming `SF_KillHeal` is already registered:

```ini
[Starfall.PassiveGrants]
1=SF_FieldGrant

[SF_FieldGrant]
Passives=SF_KillHeal
Duration=450
TargetTypes=MTNK,HTNK
```

`Passives` is required. `Duration` defaults to permanent `-1`; positive values are frames. Optional `TargetTypes` is a list of concrete native type IDs, not the research `@Category` syntax. Omission allows all otherwise eligible types, including buildings. Grants affect currently living, on-map recipients, not future units.

The preset fields use this format.

```ini
[Starfall.PassiveGrants]
1=(grant preset ID)
; Registers a preset number for map and team action 9601.

[GrantPresetID]
Passives=(comma-separated passive IDs)
; Registered passives granted to eligible recipients.
; Required

Duration=(integer game frames)
; Duration of the grant.
; Default: -1, permanent

TargetTypes=(comma-separated native type IDs)
; Restricts eligible recipients to these concrete types.
; Default: omitted, all eligible types
```

## Map Lua

Map action `9602` executes a registered Lua preset for a selected house. Define the preset, place its Lua module in the game root, and use the preset number in parameter 3. See the complete [map Lua preset](lua.md#map-lua-preset) for configuration, a callback example, context values and save/load behavior.

## Map unit bindings

A map unit-type section can add `Starfall.Passives` bindings for that map. Register the passive and define its section in an available Starfall configuration source. Map-specific unit bindings are saved.

```ini
[UnitTypeID]
Starfall.Passives=(comma-separated passive IDs)
; Adds registered passive bindings for this unit type on the map.
```

When merging, keep existing bindings and append new IDs. See the [passive registration example](setup.md#registries-and-a-complete-passive-example).

## Diagnostics and multiplayer

Check `Starfall.log` when a preset, callback, owner, or configuration entry cannot be resolved. Confirm the native trigger has an event, tag, correct action count and intended owner. For `9601`, confirm the preset number, registered passive IDs, recipient selector and eligible on-map recipients. For `9602`, confirm the module and exported function are available.

Use identical Starfall DLLs, maps, configuration and Lua files across multiplayer peers. Map and team actions execute through the synchronized game simulation. See the [Lua multiplayer rules](lua.md#timers-state-and-synchronized-randomness).
