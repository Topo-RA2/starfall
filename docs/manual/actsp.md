# ActSp Special Behavior

[Manual home](../manual.md) · [Configuration reference](reference.md)

## Contents

- [Overview and installation](#overview-and-installation)
- [Drone behavior](#drone-behavior)
- [Advanced garrison](#advanced-garrison)
- [Raised platform](#raised-platform)

## Special behavior submodule

### Overview and installation

ActSp is built into `Starfall.dll`. It adds patrol modes for native aircraft spawns, vehicle guards for buildings, independent occupant fire for garrisons, and constructible raised platforms. Each feature uses its rules settings to determine where it applies.

Run `build.cmd` from the repository root to build `build/Starfall.dll`. Place it beside `gamemd.exe` and load it with SyringeEx. When upgrading from a two-DLL installation, move the old `ActSp.dll` out of the game directory and remove `-i=ActSp.dll` from the launch configuration.

Add the fields below to `rulesmd.ini`. ActSp reads the engine's merged rules. All multiplayer peers need the same `Starfall.dll` and rules. [ActSp module guide](../../modules/ActSp/README.md)

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

Lua can call `unit:setDroneGuardCell({x=84, y=71})` or pass `ctx.point`. The method accepts integer coordinates in the playable map. It returns `true` if the unit accepts the order and `false` otherwise. The most recent rally or Lua order wins. A Lua order updates the guard cell while normal factory production uses the factory's rally point. Guard cells survive save/load.

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

### Raised platform

Set `HighPlatform.Enabled=yes` on a building type with the custom 6×6 ring foundation in the example. The platform occupies the 20 perimeter cells. ActSp raises the interior 4×4 deck by four terrain levels. Both the deck and the perimeter must be unoccupied level clear terrain or road.

Other buildings can be placed with their entire foundation on the deck, subject to the normal build radius. Units placed on the deck can travel across it. The perimeter is closed to ground movement and construction. A ground attacker below the deck cannot acquire or fire on a grounded deck target with a projectile that has `SubjectToCliffs=yes`. Force-firing at a deck cell follows the same rule. Selling or destroying the platform reopens the perimeter, restores the original terrain, destroys the buildings on the deck, and drops ground units to their deaths.

Click a visible retaining wall to select the high platform or issue an attack against it. Click occupants on the deck to select or target them individually. Wall attacks use the controller as the target and follow the weapon's normal rules.

Select your ground infantry or vehicles and click a wall of your high platform with the Enter cursor. They walk to a reachable cell outside the perimeter and transfer to free space on the deck. They wait outside while the deck is full. Stop or give them another order to cancel. To bring down your deck units, select the platform and press the Deploy key (D by default) or use the Deploy cursor on the platform. Units wait on the deck if all suitable ground cells are occupied.

`HighPlatform.Enter` defaults to `yes`. Set it to `no` in the platform's building type section to close its entrance, or in an infantry or vehicle type section to keep that type from entering any high platform. The setting only controls entry. Deploy can still bring down units on the deck.

The platform uses the building's normal SHP and palette. Set `CanHideThings=no` in its art section to keep the platform from marking deck occupants as hidden behind a building. The [concrete platform example](../../examples/HighPlatform/README.md) supplies the art and rules for a buildable Soviet platform.
