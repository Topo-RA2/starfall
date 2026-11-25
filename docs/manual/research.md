# Research Choices

English reference for mod authors, AI assistants, and maintainers. Configure projects in `Starfall.ini` or the engine-loaded rules. See [configuration sources](setup.md#installation-and-configuration-sources) for precedence.

[Manual home](../manual.md) · [Configuration reference](reference.md)

## Contents

- [Quick start](#quick-start)
- [Research projects](#research-projects)
- [Project schema](#project-schema)
- [Effect catalog](#effect-catalog)
- [Selection, refresh, and global settings](#selection-refresh-and-global-settings)
- [Periodic research points from buildings](#periodic-research-points-from-buildings)
- [Free research from buildings](#free-research-from-buildings)
- [Free research choices](#free-research-choices)
- [Random research rewards](#random-research-rewards)
- [Native research query](#native-research-query)

## Quick start

Place `Starfall.dll` in the game directory and load it with a compatible Syringe launcher. Merge the following fields into `Starfall.ini` or the loaded rules. Use unused registry numbers and distinct project IDs when combining configurations.

```ini
[Starfall.Research]
InitialPoints=100

[Starfall.ResearchTypes]
0=Demo_ReserveCash
1=Demo_GrizzlyDiscount
2=Demo_InfantryDiscount

[Demo_ReserveCash]
Name=Emergency Funds
Summary=Immediately gain 500 credits
Rarity=Common
Effect=GiveCredits
Credits=500

[Demo_GrizzlyDiscount]
Name=Grizzly Production
Summary=Reduce Grizzly Tank cost by 20%
Rarity=Common
Effect=ModifyTypeCost
TargetTypes=MTNK
CostModifier=-20%

[Demo_InfantryDiscount]
Name=Infantry Supply
Summary=Reduce infantry cost by 10%
Rarity=Common
Effect=ModifyCategoryCost
Category=Infantry
CostModifier=-10%
```

Start a new match and open the research button at the top right. Each participating house starts with 100 research points and can choose one of the three projects. Check the received credits or the unit production prices. The complete [research starter](../../examples/Research/Starfall.ini) is available in the repository.

## Research projects

On the Tab diplomacy screen, select Research beside a player to see that house's completed projects in acquisition order, including free research rewards. Hover over an icon for its name, rarity and effect details. Use the arrows to page through longer rows.

For a complete three-choice setup with starting points, see the [research starter](../../examples/Research/Starfall.ini).


### Project schema

Register every project in `[Starfall.ResearchTypes]`. Ordinary purchase costs exactly **100 research points** per project, once per house/project ID. There is no configurable `Price` field.

```ini
[ResearchProjectID]
Name=(text or ID)
; Display name.
; Default: Project ID, except built-in legacy defaults

TargetLabel=(text or ID)
; Text after the dot in this project's card subtitle. Set it on the project section, including projects with Effects. Omit it or leave it blank to use the automatic label.
; Default: derived from the effects and their target types

Summary=(text or ID)
; Short card description.
; Default: empty, except legacy defaults

Detail=(text or ID)
; Hover description; supported placeholders below.
; Default: generated where supported

Icon=(text or ID)
; PNG or engine-loadable PCX path.
; Default: built-in fallback

Countries=(list)
; Country IDs or *; comma-separated.
; Default: unrestricted when both filters are omitted

UnlockWithTech=(comma-separated type IDs)
; Alternative to a country match. A player who has acquired an UnlockTech production license for any listed type qualifies. With only this field, a license is required. Countries=* admits every country. Other project conditions still apply. Example: Countries=British and UnlockWithTech=SNIPE. Building rewards keep their existing country-filter exemption.
; Default: empty

Prerequisites=(comma-separated research IDs)
; Research project IDs, all owned by the purchasing house.
; Default: none

Replaces=(list)
; Comma-separated project IDs. The recipient must currently own all of them. Acquiring the upgrade retires those projects for this house, including their ongoing passive, price and production grants. Retired IDs cannot be drawn again or satisfy prerequisites. Prior credits, conversions and effects already applied to units remain. The upgrade defines its complete effects.
; Default: empty

GrantOnly=(boolean)
; Set this field in the project section. With yes, only Action=GrantResearch with that project ID can grant it. Ordinary offers, refreshes, AI purchases, building and free choices (even explicit lists), and random rewards of any or specified rarity skip it.
; Default: no

Excludes=(list)
; Comma-separated research project IDs, case-insensitive. If this house owns any listed project, omit this project from ordinary offers, refreshes, random grants, free choices and building rewards. For example, Excludes=ProjectB,ProjectC on ProjectA blocks A after B or C. To block B after A as well, add Excludes=ProjectA on B.
; Default: empty

Rarity=(Common, Rare, Epic, or Insane)
; Common,Rare,Epic,Insane. Invalid value excludes project.
; Default: Common

Effect=(research effect)
; One effect from the next table.
; Default: GrantPassive

Effects=(list)
; Ordered list of effect sections; overrides the single-effect fields when present. Empty list is invalid.
; Default: absent

```

Unknown prerequisites, cycles, and chains depending on invalid references block affected projects. Country restrictions and prerequisites are evaluated separately from immediate target availability. A valid conversion project may be visible while temporarily having no eligible conversion targets.


### Effect catalog

Each effect section below lists its fields, accepted values, and behavior.

#### GrantPassive

```ini
[ResearchProjectID or EffectSectionID]
Effect=GrantPassive
; Grants the registered passive to eligible current and future units.

Passive=(registered passive ID)
; Selects the passive to grant.
; Required

TargetTypes=(comma-separated type IDs or category selectors)
; Selects eligible units owned by the researcher.
; Required
```

#### GiveCredits

```ini
[ResearchProjectID or EffectSectionID]
Effect=GiveCredits
; Grants credits once.

Credits=(positive integer)
; Amount of credits to grant.
; Default: 0
```

#### UnlockTech

```ini
[ResearchProjectID or EffectSectionID]
Effect=UnlockTech
; Grants permission to produce an existing unit or building type.

Technology=(registered type ID)
; Selects the unit or building to unlock.
; Required
```

#### GrantFreeResearch

```ini
[ResearchProjectID or EffectSectionID]
Effect=GrantFreeResearch
; Queues one free research choice.

Reward.ResearchOptions=(comma-separated research project IDs)
; Restricts the candidates to the listed projects.
; Default: omitted, use all registered projects

Reward.Rarity=(Any, Common, Rare, Epic, or Insane)
; Restricts the candidate rarity.
; Default: Any
```

#### GrantRandomResearch

```ini
[ResearchProjectID or EffectSectionID]
Effect=GrantRandomResearch
; Grants randomly selected projects.

Reward.Rarity=(Any, Common, Rare, Epic, or Insane)
; Restricts the reward rarity.
; Default: Any

Reward.Count=(integer)
; Sets the number of direct draws.
; Default: 1; range: 1..256
```

#### ReplaceProduction

```ini
[ResearchProjectID or EffectSectionID]
Effect=ReplaceProduction
; Replaces a type in production for the acquiring house.

FromType=(registered type ID)
; Existing production type.
; Required

ToType=(registered type ID)
; Replacement of the same object class. Building types also require the same BuildCat.
; Required
```

#### ConvertInfantry

```ini
[ResearchProjectID or EffectSectionID]
Effect=ConvertInfantry
; Converts eligible existing infantry owned by the acquiring house.

FromType=(infantry type ID)
; Existing infantry type.
; Required

ToType=(infantry type ID)
; Destination infantry type.
; Required
```

#### ConvertVehicle

```ini
[ResearchProjectID or EffectSectionID]
Effect=ConvertVehicle
; Converts eligible existing vehicles owned by the acquiring house.

FromType=(vehicle type ID)
; Existing vehicle type.
; Required

ToType=(vehicle type ID)
; Destination vehicle type.
; Required
```

#### ModifyTypeCost

```ini
[ResearchProjectID or EffectSectionID]
Effect=ModifyTypeCost
; Changes the production cost of specified types for the acquiring house.

TargetTypes=(comma-separated type IDs)
; Selects the concrete types whose cost changes.
; Required

CostModifier=(nonzero signed integer or percentage)
; Adds a fixed credit amount or percentage to the production cost.
; Required
```

#### ModifyCategoryCost

```ini
[ResearchProjectID or EffectSectionID]
Effect=ModifyCategoryCost
; Changes the production cost of one sidebar category for the acquiring house.

Category=(Building, Defense, Infantry, or Vehicle)
; Selects the production category.
; Required

CostModifier=(nonzero signed percentage)
; Adds a percentage to the production cost.
; Required
```

#### Effect rules

GrantPassive `TargetTypes` accepts concrete IDs, `*`, `@Infantry`, `@Vehicle`, `@GroundVehicle`, `@Aircraft`, and `@Building`; lists may mix them. Unknown `@` categories are invalid. These selectors are not universally supported by other fields with the same name. Cost targeting requires real type IDs. `@GroundVehicle` selects Unit-class types excluding Naval, JumpJet, BalloonHover, ConsideredAircraft, Winged speed and Fly movement zone. It includes harvesters, transports, MCVs and custom ground types, and remains applicable when a ground vehicle is temporarily knocked airborne. Research, aura, nearby conditions, Lua queries and warhead grant filters share this selector.

Cost categories: `Building` means ordinary buildings; `Defense` means `BuildCat=Combat` buildings; `Infantry` means infantry; `Vehicle` includes ground vehicles, ships, and aircraft. After native country/factory modifiers, add all research percentage deltas, then fixed credit deltas, round to nearest integer, and clamp at zero. A native quote of 700 with `-20%` and `-100` becomes 460. Production orders lock their price when started; cancellation refunds the paid amount.

Production replacements are derived from saved research ownership, with no extra save payload. Keep source and destination IDs unique across active replacements; replacement chains are not supported. Replacement effects affect production only and do not use building `Transform`, which still requires identical occupied foundations. Other research that lists concrete source types must also list the replacement types if both should benefit.

Art sections can set `Starfall.PlacementPreviewScale` (default 1.0, range 0.01–2.00) to scale the Phobos placement preview. The building foundation is defined separately.

Effects lists deduplicate repeated effect section IDs while preserving first occurrence. Each effect section must explicitly define its `Effect`; it does not inherit the parent project's fields. Any invalid effect makes the project unavailable. At purchase time, temporarily inapplicable effects are skipped; at least one successful effect is needed to charge points and record ownership. Executed effects are not rolled back if a later effect fails, and skipped instant effects are not granted later.

Use canonical `UnlockTech`. The parser retains `UnlockTeck` as a compatibility spelling, but new configuration should not generate it. The legacy project IDs `SF_Repair`, `SF_Drain`, and `SF_Riposte` have built-in text/passive defaults; they still need registration and usable referenced passives. Define all fields explicitly for new projects.


### Selection, refresh, and global settings

Candidates exclude owned, invalid, wrong-country, and unmet-prerequisite projects. First select one nonempty rarity according to its weight, then offer up to three projects from that rarity. Opening the local panel does not consume the simulation RNG. Chosen project IDs, seen cards, and used refresh slots are saved.

Refresh requires a balance of at least 100, but is free. Each card slot may refresh once per offer round, only to an eligible unseen project of the same rarity. No alternative means no refresh allowance is consumed. Closing/reopening retains the board. Purchasing starts the next round.

With `SharedRaritySequence=yes`, every house uses the same weighted rarity sequence for ordinary research: the first purchase uses sequence entry 1, the second uses entry 2, etc. The sequence depends on the match seed and the house's completed ordinary purchase count, never on its country, house index, selected project IDs, panel opening order, or other players' progress. All cards in a round share that rarity; individual projects remain country/prerequisite/ownership-filtered and are drawn separately for each house. AI research follows the same rule. The sequence is random according to `RarityWeight.*`; it does not force an Epic-first or Rare-second order.

If a house has no eligible project at the designated rarity, it draws from its own remaining nonempty, positive-weight tiers instead. This exception can differ from other houses, but does not skip sequence entries or change anyone else's sequence. Zero-weight tiers remain excluded. A refresh, failed/duplicate purchase, or a free research choice does not advance the counter. Successful ordinary purchases advance exactly once, even while the switch is off. The sequence seed, counter and current board survive saves, including saves before the first purchase. Older saves without this data use the current match seed and resume after the total number of owned projects (including free rewards, which those saves cannot distinguish). For predictable initial boards, configure the switch before starting the match; already saved eligible boards are retained when reloading configuration.

Fields in `[Starfall.Research]`:

```ini
[Starfall.Research]
Enabled=(boolean)
; yes,true,1 enable; no,false,0 disable research UI, point awards, purchases, and free research rewards. Stored ownership/points are retained.
; Default: yes

ShowAvailableAnimation=(boolean)
; Five blue trails converge on the research button when an ordinary purchase becomes available or the player receives a free choice. Each free entitlement triggers one local notification.
; Default: yes

AllowAIResearch=(boolean)
; Enable computer-controlled houses to claim action-granted free choices and buy ordinary research. Every 15 simulation frames each AI first tries to claim one applicable free choice. Otherwise, an AI with at least 100 points randomly chooses an applicable card from its ordinary offer. A successful purchase costs 100 points and follows AnnounceSelection. If no card can take effect, the AI keeps its points.
; Default: no

SharedRaritySequence=(boolean)
; All houses share the weighted rarity sequence by ordinary purchase number. Specific projects remain independent; an unavailable tier falls back to other eligible positive-weight tiers. Free building research does not advance it.
; Default: no

AnnounceSelection=(boolean)
; Show the player and project name to everyone after a successful ordinary or free research choice. Set to no to turn off these messages.
; Default: yes

AllowCardRefresh=(boolean)
; Same boolean forms; controls ordinary card refresh.
; Default: yes

Title=(text or ID)
; Set explicitly for an English interface.
; Default: Built-in Chinese text meaning “Choose an upgrade”

InitialPoints=(number)
; Nonnegative integer initial points per participating house.
; Default: 0

PeriodicPoints=(number)
; Nonnegative integer award each period.
; Default: 0

PeriodicIntervalSeconds=(number)
; Nonnegative integer game seconds, at most INT_MAX/15; both amount and interval must be positive to enable.
; Default: 0

RarityWeight.Common=(number)
; Nonnegative integer.
; Default: 5

RarityName.Common=(text or ID)
; Global display name for each tier. Omitted or blank names use that tier's default. Project Rarity and reward filters still take the tier IDs.
; Default: 普通, 稀有, 史诗, 疯狂

RarityName.Rare=(text or ID)
; Global display name for each tier. Omitted or blank names use that tier's default. Project Rarity and reward filters still take the tier IDs.
; Default: 普通, 稀有, 史诗, 疯狂

RarityName.Epic=(text or ID)
; Global display name for each tier. Omitted or blank names use that tier's default. Project Rarity and reward filters still take the tier IDs.
; Default: 普通, 稀有, 史诗, 疯狂

RarityName.Insane=(text or ID)
; Global display name for each tier. Omitted or blank names use that tier's default. Project Rarity and reward filters still take the tier IDs.
; Default: 普通, 稀有, 史诗, 疯狂

RarityWeight.Rare=(number)
; Nonnegative integer.
; Default: 4

RarityWeight.Epic=(number)
; Nonnegative integer.
; Default: 1

RarityWeight.Insane=(number)
; Nonnegative integer; zero disables this rarity.
; Default: 0

```

Initial/periodic awards include human and AI houses in player slots; neutral, defeated, and observer houses are excluded. Campaign handling also considers the current player. Pause does not advance the timer. Saves preserve progress and prevent duplicate initial awards; older saves lacking that record do not receive a retroactive initial award.

**Complete research configuration**, assuming `MTNK` exists:

```ini
[Starfall.PassiveTypes]
0=SF_ResearchRepair

[SF_ResearchRepair]
Trigger=OnKill
Filter.TargetHouse=Enemies
Action=Heal
Action.Target=Self
Action.Amount=50

[Starfall.ResearchTypes]
0=SF_FieldRepair
1=SF_SupplyPackage

[Starfall.Research]
Title=Choose an upgrade
RarityName.Common=Standard
InitialPoints=100
PeriodicPoints=10
PeriodicIntervalSeconds=30

[SF_FieldRepair]
Name=Field Repair
TargetLabel=Grizzly Tanks
Summary=Owned tanks heal after a kill.
Rarity=Common
Effect=GrantPassive
Passive=SF_ResearchRepair
TargetTypes=MTNK

[SF_SupplyPackage]
Name=Supply Package
Rarity=Common
Effects=SF_SupplyMoney,SF_SupplyDiscount

[SF_SupplyMoney]
Effect=GiveCredits
Credits=500

[SF_SupplyDiscount]
Effect=ModifyTypeCost
TargetTypes=MTNK
CostModifier=-10%
```

Do not also bind `SF_ResearchRepair` to `MTNK` if the intended design requires research first.


<a id="buildings-and-free-research"></a>

### Periodic research points from buildings

Configure these fields in a building type's section to award research points at a fixed interval.

```ini
[BuildingTypeID]
Starfall.ResearchPoints=(integer)
; Positive points per completed building interval.
; Default: 0

Starfall.ResearchInterval=(integer)
; Positive frames; both fields must be positive.
; Default: 150
```

Each working building has its own income clock. EMP, offline power, or chrono removal pauses it; construction, sale, or leaving the map clears progress. Ownership change restarts the clock for the new owner.

### Free research from buildings

Configure these fields in a building type's section to offer free research when the corresponding capture or infiltration condition is met.

```ini
[BuildingTypeID]

Starfall.ResearchOptions=(comma-separated research IDs)
; Comma-separated project IDs. Up to three distinct IDs use the configured order. Longer lists use the global rarity weights to draw up to three different projects.
; Default: empty

Starfall.ResearchOptions.AllowEngineerCapture=(boolean)
; Permit reward authorization after successful engineer capture.
; Default: no

Starfall.ResearchOptions.AllowSpyInfiltration=(boolean)
; Permit authorization for the spy's owner after successful infiltration.
; Default: no

```

Open the top-right research button to claim a free choice at any point balance. Its badge counts pending building rewards and choices granted by `GrantFreeResearch`, including building rewards outside the viewport. Free choices open first. Use the button below the cards to cycle through pending rewards and, with sufficient points, return to ordinary research.

In a list of more than three distinct IDs, each slot draws a nonempty rarity tier using `RarityWeight.*`, then picks a project uniformly from that tier without replacement. Invalid, owned and zero-weight candidates are excluded. The board may mix rarities and has fewer than three cards when the pool runs out. It is stable across reopening and save/load with the same configuration and ownership. A building’s fixed list of up to three IDs can explicitly offer zero-weight tiers.

Each physical reward building can be claimed once. Ownership change invalidates old authorization; generic ownership transfer is not a substitute for an authorized capture/infiltration. Claiming is free and ignores project country/prerequisite restrictions, but still requires a valid unowned project and applicable effect. Existing claimed state persists. Older unclaimed authorization without a recorded acquisition source is cleared on load. The building must separately support native capture/infiltration mechanics.


### Free research choices

Use `Action=GrantFreeResearch` to let the target unit's owner choose one technology from the top-right research button. Each successful grant adds one to the badge and plays the configured free-research notification. Map scripts and ActionSequence superweapons can give the choice to their owner. Lua accepts, for example, `ctx:action("GrantFreeResearch", {ResearchOptions={"Demo_ReserveCash","Demo_GrizzlyDiscount"}})`.

| Scope | Cards offered |
| --- | --- |
| Omit `Action.ResearchOptions` | Draw up to three distinct registered projects using the global rarity weights. |
| Set 1–3 distinct project IDs | Show eligible listed projects in configuration order, including zero-weight rarities. |
| Set 4 or more distinct IDs | Draw up to three distinct listed projects. Each slot draws a nonempty rarity by global weight, then a project from that rarity. |
| Set `Action.Rarity` to `Common`, `Rare`, `Epic` or `Insane` | Restrict either pool to that rarity, including a rarity with zero global weight. |

All choices respect the recipient's country, prerequisites and existing ownership. An empty eligible pool gives no choice. Multiple grants can wait independently. If the player acquires another project, pending offers are recalculated within their original scope. Reopening the panel and loading a save preserve the offer for the same ownership state. Claiming costs zero points and preserves the ordinary purchase round and used refresh slots.

```ini
[SF_SovietResearchReward]
Trigger=OnKill
MaxTriggers=1
Action=GrantFreeResearch
Action.ResearchOptions=Demo_ReserveCash,Demo_GrizzlyDiscount
```

A research project or a section referenced by its `Effects` list can also grant one choice. Set `Effect=GrantFreeResearch` and optionally `Reward.ResearchOptions` and `Reward.Rarity` (default `Any`). Its reward becomes available after the project itself is acquired.


### Random research rewards

External game extensions can read the resulting ownership through the [native research query](#native-research-query).

`Action=GrantRandomResearch` awards exactly one registered project to the target unit's owner (`Action.Target=Self` by default). It also supports the owner context of map scripts and ActionSequence superweapons, and Lua `ctx:action("GrantRandomResearch", {Rarity="Epic"})`. For a research project's own reward, use the separate `Effect=GrantRandomResearch` described below.

Research effects accept `Reward.Rarity=Any` (default; also `Common`, `Rare`, `Epic`, `Insane`) and `Reward.Count=1` (default, range 1..256). These fields work both directly on a project and in an `Effects` entry. The source is recorded as owned before drawing rewards, excluding itself and every other owned project. Other reward projects may be drawn and enqueue their own rewards; each project's direct draws finish before queued chains are processed. The whole chain is synchronous, deterministic and saved through the resulting ownership and RNG state. Loading a save or creating a unit never awards these rewards again. Only the original ordinary purchase costs points and advances its round. Empty specified tiers never fall back; a partially exhausted pool awards only the remaining eligible projects. A reward-only cycle with no usable terminal project cannot be selected.

The infantry/reward additions are Super Body Armor (Epic, all owned infantry armor x3 and speed x0.85), Hua Tuo Scroll (Rare, all owned on-map infantry heal 25 HP every 30 frames, capped at maximum), Chaos Destiny (Epic, 2 random projects of any rarity), Epic Transmutation (Rare, 1 Epic project), and Rare Transmutation (Common, 1 Rare project). Both infantry bonuses cover existing and future infantry and follow current ownership. Install the updated DLL with either icon configuration; these five projects do not require Lua.

`Action.Rarity=Common`, `Rare`, `Epic`, or `Insane` restricts the draw to that rarity. Omit the field or set `All` to draw uniformly from all eligible projects across all rarities. `RarityWeight.*` only controls ordinary offers and is not used by this reward; a zero-weight tier can still be awarded. A specified empty tier fails without falling back to another rarity. The pool excludes owned projects, wrong-country projects, unmet/invalid prerequisite chains, invalid effects, and projects whose effects cannot currently apply. No eligible project means no award and no RNG draw; Lua reports `false`. Otherwise the draw uses the saved, synchronized extension RNG shared with Lua random calls; it does not advance the native game RNG. The project's normal effects execute before ownership and acquisition order are recorded.

This result respects `Enabled` and `AnnounceSelection`, costs no points, and does not advance `SharedRaritySequence` or ordinary purchase counts. After an award, ordinary offers are rebuilt on every peer to exclude acquired projects; already-used refresh slots remain used. Ownership and effects persist through saves like other acquired research. `AllowAIResearch` controls autonomous purchases only and does not prevent this explicit reward for an AI house.

```ini
[SF_RandomEpicReward]
Trigger=OnKill
MaxTriggers=1
Action=GrantRandomResearch
Action.Target=Self
Action.Rarity=Epic
```

Register this passive in `[Starfall.PassiveTypes]` and bind/grant it to a unit as usual. Use `Action.Rarity=All` for unrestricted rarity. `MaxTriggers=1` in this example is per unit, not a once-per-house limit; binding the passive to many units can award many projects.


### Native research query

A 32-bit C or C++ extension can resolve `Starfall_GetOwnedResearch` from the loaded `Starfall.dll`. Call it on the game thread during a match, after initialization or save restoration. Pass the player's `HouseClass::ArrayIndex`. The result includes purchased projects, building rewards and random grants, sorted by normalized INI project ID. For example, `Demo_InfantryDiscount` is returned as `demo_infantrydiscount`. [Public C header](../../src/StarfallAPI.h)

The return value is the required buffer size in bytes. A missing house returns `0`. A valid house with no research returns `2` and writes two NUL bytes when space permits. Each ID ends in NUL, followed by one extra NUL at the end of the list. A null or undersized buffer receives no data. The caller owns the copied bytes and can retain them across game frames.

```cpp
#include "StarfallAPI.h"
#include <windows.h>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

std::optional<std::vector<std::string>> ownedResearch(int32_t houseIndex) {
    HMODULE module = GetModuleHandleW(L"Starfall.dll");
    if (!module) return std::nullopt;
    auto query = reinterpret_cast<Starfall_GetOwnedResearchFn>(
        GetProcAddress(module, "Starfall_GetOwnedResearch"));
    if (!query) return std::nullopt;

    std::vector<char> buffer;
    uint32_t bytes = query(houseIndex, nullptr, 0);
    while (bytes > buffer.size()) {
        buffer.resize(bytes);
        bytes = query(houseIndex, buffer.data(), bytes);
    }
    if (!bytes) return std::nullopt;

    std::vector<std::string> ids;
    for (const char* id = buffer.data(); *id; id += std::strlen(id) + 1)
        ids.emplace_back(id);
    return ids;
}
```
