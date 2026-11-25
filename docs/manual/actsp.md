# ActSp Special Behavior

[Manual home](../manual.md) · [Configuration reference](reference.md)

## Contents

- [Overview and installation](#overview-and-installation)
- [Drone behavior](#drone-behavior)
- [Advanced garrison](#advanced-garrison)

## Special behavior submodule

### Overview and installation

ActSp is an optional companion DLL that adds patrol modes for native aircraft spawns, vehicle guards for buildings, and independent occupant fire for garrisons.

To build it from source, run `modules\ActSp\build.cmd` from the repository root. Place the resulting `build/ActSp.dll` beside `gamemd.exe` and append `-i=ActSp.dll` to the SyringeEx launch configuration. When using these extensions together, load `Starfall.dll`, `ActSp.dll`, Ares, and Phobos in that order.

Add the fields below to `rulesmd.ini`. ActSp reads the engine's merged rules. All multiplayer peers need the same ActSp version and rules. [ActSp module guide](../../modules/ActSp/README.md)

### Drone behavior

#### Aircraft spawns

Put `Drone.*` fields in the carrier type's section. Aircraft modes require native spawns and slots. Launching without an attack target requires `Spawner=yes` on the carrier's current launcher, including its elite weapon when applicable. Missile spawns use their native behavior.

`Native` follows native launch behavior. `Guard` uses a fixed map cell. `Follow` guards a map-axis offset from the carrier. `Orbit` updates waypoints around the carrier and staggers its slots.

`Drone.GuardRange` sets the acquisition radius around the guard cell. It defaults to `5` and accepts `1..32` cells. The carrier's assigned target takes priority. Otherwise drones choose the nearest eligible enemy within the radius. When out of ammo, aircraft use their native return and reload behavior.

```ini
[CarrierTypeID]
Drone.Mode=(Native, Guard, Follow, or Orbit)
; Selects the carrier's drone behavior.
; Default: Native

Drone.GuardCell=(x,y integer map cells)
; Sets the initial fixed station for Guard mode.
; Default: omitted

Drone.Offset=(x,y integer map cells)
; Sets the map-axis offset used by Follow mode.
; Default: 2,0; each axis: -32..32

Drone.Radius=(integer cells)
; Sets the orbit radius in Orbit mode.
; Default: 3; range: 1..32

Drone.Period=(integer frames)
; Sets the orbit period in Orbit mode.
; Default: 180; range: 30..3600

Drone.GuardRange=(integer cells)
; Sets the acquisition radius around the guard station.
; Default: 5; range: 1..32
```

```ini
[CARRIER]
Drone.Mode=Orbit
Drone.Radius=3
Drone.Period=180
```

#### Building ground guards

Set `Drone.Mode=Guard` and `Drone.GroundUnit` in a building type's section. Use a registered ground vehicle type with normal ground movement. Each building manages one vehicle guard. If the building has a native spawn manager, its aircraft slots take priority.

```ini
[BuildingTypeID]
Drone.GroundUnit=(registered ground vehicle type ID)
; Selects the vehicle managed by a Guard-mode building.
; Default: omitted

Drone.GroundRespawnDelay=(integer frames)
; Sets the replacement delay after the guard is lost or the building changes owner.
; Default: 300; range: 0..1000000
```

Example fragment for a building type section:

```ini
Drone.Mode=Guard
Drone.GroundUnit=MTNK
Drone.GroundRespawnDelay=300
Drone.GuardRange=5
```

Once assigned a guard cell, the building deploys the vehicle from a passable empty cell next to its foundation. It waits if all exits are blocked. The guard uses its own pathfinding and weapons. `Drone.GroundRespawnDelay` is measured in game frames and defaults to `300` (`0..1000000`). On capture, the previous owner keeps the old guard and the new owner gets a replacement after the delay. If the building is sold or destroyed, a surviving guard becomes an ordinary unit. The guard assignment and replacement timer survive save/load. [Ground outpost example](../../examples/GrizzlyGroundOutpost/README.md)

#### Guard cell orders

Set `Drone.GuardCell=x,y` in the type section for an initial guard cell. During play, select a friendly `Guard` building and use the normal rally point control to assign a cell shared by its spawn slots. Choose a reachable cell for a ground vehicle.

With Starfall and ActSp loaded, Lua can call `unit:setDroneGuardCell({x=84, y=71})` or pass `ctx.point`. The method accepts integer coordinates in the playable map and returns `true` if the order is accepted. It returns `false` if ActSp is absent or the unit cannot accept the order. The most recent rally or Lua order wins. A Lua order updates the guard cell while normal factory production uses the factory's rally point. Guard cells survive save/load.

### Advanced garrison

<a id="actsp-bettergarrison"></a>

Set `BetterGarrison.Enabled=yes` in `[General]` to enable the feature for buildings that allow infantry to garrison and fire. Set `BetterGarrison.Enabled=no` in a building type's section to keep its original garrison weapons.

Set any of the three adjustment fields in a building type's section to override that field for all buildings of the type. Omitted fields inherit from `[General]`. The following example gives Battle Bunker occupants 50% more damage, two extra cells of range, and a 1.25× fire rate.

```ini
[General]
BetterGarrison.Enabled=yes

[NABNKR]
BetterGarrison.DamageMultiplier=1.50
BetterGarrison.RangeBonus=2
BetterGarrison.FireRateMultiplier=1.25
```

Occupants use their transport weapons according to `OpenTransportWeapon` and `FireInTransport` and reload independently. The longest-serving current occupant's weapon determines which targets can be ordered. Each occupant prioritizes the ordered target when able and otherwise acquires eligible hostile targets. Click an eligible neutral object to attack it manually. Hold Ctrl and left-click eligible empty ground to force-fire. The pointer shows a stationary attack when a garrison can fire at the target. Range is measured from the building's center, and projectiles launch from the assigned garrison ports.

```ini
[General]
BetterGarrison.Enabled=(boolean)
; Enables advanced garrison fire.
; Default: no

BetterGarrison.DamageMultiplier=(decimal multiplier)
; Multiplies occupant weapon damage.
; Default: 1.20. Accepted range: 0..100.

BetterGarrison.RangeBonus=(integer cells)
; Adds this many cells to weapon range for acquisition and firing.
; Default: 1. Clamped to -128..128.

BetterGarrison.FireRateMultiplier=(number)
; Divides full reload time by this multiplier, rounded up to at least one game frame.
; Default: 1.0. Accepted range: 0.01..100.

```
