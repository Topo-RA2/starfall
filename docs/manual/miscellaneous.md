# Miscellaneous

[Manual home](../manual.md) · [Configuration reference](reference.md)

## Contents

- [ActionSequence superweapons](#actionsequence-superweapons)

## ActionSequence superweapons

`Type=ActionSequence` is provided by Starfall and can run without Ares/Phobos. Each native registered superweapon entry retains its own sidebar identity and recharge. Native buildings grant it through `SuperWeapon` or `SuperWeapon2`.

```ini
[SuperWeaponID]
Type=(ActionSequence)
; ActionSequence.

Starfall.Actions=(list)
; Required nonempty ordered action-section list; duplicate steps remain steps.

Starfall.Target=(Point)
; Point only; default point behavior.

RechargeTime=(number)
; Native game minutes, not Starfall frames.

```

Direct INI steps support positional actions targeting `Point` (or omitted target), money/research-point actions targeting `Owner` (or omitted target), and `RunScript` with no explicit target. SpawnUnit and SpawnBuilding owner may use `Owner`/`Self`, `Neutral`, or player-slot aliases. To heal, damage, or move particular units from Lua, first obtain live handles and pass them explicitly.

**Native rules fragment**, assuming the referenced native names exist:

```ini
[SuperWeaponTypes]
SF01=FieldSupply

[GAPILE]
SuperWeapon=FieldSupply

[FieldSupply]
Type=ActionSequence
UIName=Name:MTNK
SidebarImage=GTNKICON
RechargeTime=1
IsPowered=no
Starfall.Actions=SF_DropTank,SF_SupplyCredits

[SF_DropTank]
Action=SpawnUnit
Action.Target=Point
Action.UnitType=MTNK
Action.Owner=Owner

[SF_SupplyCredits]
Action=GiveCredits
Action.Target=Owner
Action.Amount=500
```

Sequence Lua receives `ctx.point`, `ctx.owner`, `ctx.superWeapon`, and `ctx.castId`; initial self/target are nil. Timers and state are associated with the cast, not an imaginary caster unit. Cast state is retired when no pending tasks remain; persistent animations follow their own ownership/lifetime rules.
