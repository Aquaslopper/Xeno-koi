# Opponents

The campaign has **twelve CPU opponents**, one per month, plus the final boss **Entity 13 // DEUS SUPREMUS**. Each opponent has a favourite card, a favourite yaku, and their own taunts. Their names and text are generated from `src/cards.c`.

Opponents unlock in order. You win a match against the newest opponent to unlock the next one. See [Modes](modes.md) for the unlock rules.

## Roster

<!-- BEGIN GENERATED: opponents -->

| # | Opponent | Unit classification | Favorite card | Focus |
|---:|---|---|---|---|
| 0 | **01: JANUARY CRANE** | Obsidian Sentinel // Sector 01 | ![01-B Crane](../assets/cards/card-00.webp){ .card-mini } 01-B Crane (id 0) | Brights |
| 1 | **02: FEB. WARBLER** | Avian Construct // Sector 02 | ![02-A Warbler](../assets/cards/card-04.webp){ .card-mini } 02-A Warbler (id 4) | Ribbons |
| 2 | **03: MARCH CURTAIN** | Sensory Overload Unit // Sector 03 | ![03-B Curtain](../assets/cards/card-08.webp){ .card-mini } 03-B Curtain (id 8) | Brights |
| 3 | **04: APRIL CUCKOO** | Parasitic Vine-Mind // Sector 04 | ![04-A Cuckoo](../assets/cards/card-12.webp){ .card-mini } 04-A Cuckoo (id 12) | Plains/Kasu |
| 4 | **05: MAY BRIDGE** | Structural Intelligence // Sector 05 | ![05-A Bridge](../assets/cards/card-16.webp){ .card-mini } 05-A Bridge (id 16) | Animals |
| 5 | **06: JUNE SWARM** | Hive-Mind Vector // Sector 06 | ![06-A Butterflies](../assets/cards/card-20.webp){ .card-mini } 06-A Butterflies (id 20) | Animals |
| 6 | **07: JULY BOAR** | Heavy-Armor Unit // Sector 07 | ![07-A Boar](../assets/cards/card-24.webp){ .card-mini } 07-A Boar (id 24) | Ino-Shika-Cho |
| 7 | **08: AUGUST MOON** | Orbital Observer // Sector 08 | ![08-B Moon](../assets/cards/card-28.webp){ .card-mini } 08-B Moon (id 28) | Brights |
| 8 | **09: SEPT. SAKE** | Chemical Processing Unit // Sector 09 | ![09-A Sake Cup](../assets/cards/card-32.webp){ .card-mini } 09-A Sake Cup (id 32) | Animals |
| 9 | **10: OCT. DEER** | Sensory-Stealth Unit // Sector 10 | ![10-A Deer](../assets/cards/card-36.webp){ .card-mini } 10-A Deer (id 36) | Ino-Shika-Cho |
| 10 | **11: NOV. RAIN** | Atmospheric Anomaly // Sector 11 | ![11-B Rainman](../assets/cards/card-40.webp){ .card-mini } 11-B Rainman (id 40) | Ribbons |
| 11 | **12: DEC. PHOENIX** | The Apex Biomech // Sector 12 | ![12-B Phoenix](../assets/cards/card-44.webp){ .card-mini } 12-B Phoenix (id 44) | Brights |
| 12 | **ENTITY 13 // DEUS SUPREMUS** | Omnipresent Machine God // Sector Null | ![12-B Phoenix](../assets/cards/deus/card-44.webp){ .card-mini } 12-B Phoenix (id 44) | Brights |

Taunt lines:

| # | Match taunt | Koi-Koi call |
|---:|---|---|
| 0 | *THE CRANE SEES ALL FUTURES.* | *SOLAR RESERVES PEAKING. KOI-KOI!* |
| 1 | *A BEAUTIFUL, SYMMETRICAL DEATH.* | *THE WEAVE EXPANDS. KOI-KOI!* |
| 2 | *REVEL IN THE NEON BLOSSOMS.* | *ENERGY SURGING. KOI-KOI!* |
| 3 | *DEATH BY A THOUSAND CUTS.* | *STRANGLING ALL PROTOCOLS. KOI-KOI!* |
| 4 | *CROSS THE VOID.* | *SPANNING THE ABYSS. KOI-KOI!* |
| 5 | *THE SWARM CONSUMES.* | *THE HIVE MULTIPLIES. KOI-KOI!* |
| 6 | *BRUTE FORCE PROTOCOL ENGAGED.* | *TRAMPLING ALL DEFENSES. KOI-KOI!* |
| 7 | *ECLIPSE IMMINENT.* | *ORBITAL ALIGNMENT COMPLETE. KOI-KOI!* |
| 8 | *INTOXICATING ALGORITHMS.* | *FERMENTING SYNAPSE. KOI-KOI!* |
| 9 | *PRECISION STRIKE.* | *TARGET LOCK MAINTAINED. KOI-KOI!* |
| 10 | *THE STORM WASHES ALL AWAY.* | *DELUGE IMMINENT. KOI-KOI!* |
| 11 | *ASH TO ASH. DUST TO DUST.* | *REBORN FROM CRUCIBLE. KOI-KOI!* |
| 12 | *PURE MATHEMATICAL PERFECTION.* | *INEVITABILITY CONFIRMED. KOI-KOI!* |

<!-- END GENERATED: opponents -->

## How opponent strength works

Each opponent sets how accurately the CPU plays. This is the chance, on each decision, that the CPU picks its best-scoring card rather than a random one. Accuracy rises with each opponent:

| Difficulty | Opponent 1 (January Crane) | Opponent 12 (December Phoenix) |
|---|---:|---:|
| Easy | 20% | 60% |
| Hard | 50% | 80% |
| Master | 78% | 100% |
| DEUS SUPREMUS | 100% | 100% |

The favourite card and favourite yaku also make the CPU prefer those cards and combinations. See [Difficulty](difficulty.md).
