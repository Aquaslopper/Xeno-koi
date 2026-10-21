# Opponents

The campaign has **twelve CPU opponents**, one per month, plus the final boss **[Entity 13 // DEUS SUPREMUS](deus-supremus.md)**. Each opponent has a favourite card, a favourite yaku, and their own taunts.

Opponents unlock in order when your earned bronze, silver, and gold coins meet the cumulative requirement shown on each locked-sector dossier. See [Modes & Settings](modes.md#campaign-and-unlocks) for how to earn coins and the complete unlock thresholds.

<div class="figure-zoom" id="campaign-live"><div class="figure-zoom-scroll"><img src="../../assets/screenshots/campaign.webp" width="1920" height="1080" loading="lazy" alt="The campaign select screen: twelve portrait tiles in a row labelled M-01 to M-12, the first nine open and the last three reading LOCKED, the ninth selected with a large portrait beside a dossier that names the unit, its priority yaku, its signature target and three clearance buttons, with START MATCH and BACK below."></div><a class="figure-zoom-btn" href="#campaign-live">zoom the screen</a><a class="figure-zoom-btn" href="#">close</a></div>

*Example image: the campaign screen with nine of the twelve opponents unlocked.*

<div class="figure-zoom" id="grid-tiles"><div class="figure-zoom-scroll"><img src="../../assets/screenshots/opponent-grid.webp" width="1487" height="210" loading="lazy" alt="The twelve tiles in a row: nine Giger portraits of the monthly units with their slot captions M-01 to M-09, the ninth framed by its selection border, and M-10, M-11 and M-12 as empty cells reading LOCKED."></div><a class="figure-zoom-btn" href="#grid-tiles">zoom the tiles</a><a class="figure-zoom-btn" href="#">close</a></div>

*The three clearance buttons — Master (3 rounds), Hard (6 rounds), Easy (9 rounds) — set the difficulty and the match length. Win a sufficiently long match against any unlocked opponent to earn that opponent’s coin for the chosen tier: 3 or more rounds on Master, 6 or more on Hard, 9 or more on Easy. Opening the next sector depends on your cumulative bronze, silver and gold totals, not just the newest opponent.*

<div class="figure-zoom" id="dossier"><div class="figure-zoom-scroll"><img src="../../assets/screenshots/dossier-row.webp" width="1014" height="129" loading="lazy" alt="Three lines of the selected opponent's dossier: PRIORITY YAKU: Animals, SIGNATURE TARGET: 09-A Sake Cup (#33), and CLEARANCE PROTOCOL with three buttons reading MASTER (3 RDS), HARD (6 RDS) and EASY (9 RDS), the last one lit."></div><a class="figure-zoom-btn" href="#dossier">zoom the row</a><a class="figure-zoom-btn" href="#">close</a></div>

*Example image: the dossier row — the opponent's priority yaku, signature target, and clearance protocol.*

## Roster

<!-- BEGIN GENERATED: opponents -->

| Opponent | Unit classification | Favorite card | Focus |
|---|---|---|---|
| **01: JANUARY CRANE** | Obsidian Sentinel // Sector 01 | ![01-B Crane](../assets/cards/card-00.webp){ .card-mini } 01-B Crane (id 0) | Brights |
| **02: FEB. WARBLER** | Avian Construct // Sector 02 | ![02-A Warbler](../assets/cards/card-04.webp){ .card-mini } 02-A Warbler (id 4) | Ribbons |
| **03: MARCH CURTAIN** | Sensory Overload Unit // Sector 03 | ![03-B Curtain](../assets/cards/card-08.webp){ .card-mini } 03-B Curtain (id 8) | Brights |
| **04: APRIL CUCKOO** | Parasitic Vine-Mind // Sector 04 | ![04-A Cuckoo](../assets/cards/card-12.webp){ .card-mini } 04-A Cuckoo (id 12) | Plains/Kasu |
| **05: MAY BRIDGE** | Structural Intelligence // Sector 05 | ![05-A Bridge](../assets/cards/card-16.webp){ .card-mini } 05-A Bridge (id 16) | Animals |
| **06: JUNE SWARM** | Hive-Mind Vector // Sector 06 | ![06-A Butterflies](../assets/cards/card-20.webp){ .card-mini } 06-A Butterflies (id 20) | Animals |
| **07: JULY BOAR** | Heavy-Armor Unit // Sector 07 | ![07-A Boar](../assets/cards/card-24.webp){ .card-mini } 07-A Boar (id 24) | Ino-Shika-Cho |
| **08: AUGUST MOON** | Orbital Observer // Sector 08 | ![08-B Moon](../assets/cards/card-28.webp){ .card-mini } 08-B Moon (id 28) | Brights |
| **09: SEPT. SAKE** | Chemical Processing Unit // Sector 09 | ![09-A Sake Cup](../assets/cards/card-32.webp){ .card-mini } 09-A Sake Cup (id 32) | Animals |
| **10: OCT. DEER** | Sensory-Stealth Unit // Sector 10 | ![10-A Deer](../assets/cards/card-36.webp){ .card-mini } 10-A Deer (id 36) | Ino-Shika-Cho |
| **11: NOV. RAIN** | Atmospheric Anomaly // Sector 11 | ![11-B Rainman](../assets/cards/card-40.webp){ .card-mini } 11-B Rainman (id 40) | Ribbons |
| **12: DEC. PHOENIX** | The Apex Biomech // Sector 12 | ![12-B Phoenix](../assets/cards/card-44.webp){ .card-mini } 12-B Phoenix (id 44) | Brights |
| **ENTITY 13 // DEUS SUPREMUS** ([dossier](deus-supremus.md)) | Omnipresent Machine God // Sector Null | ![12-B Phoenix](../assets/cards/deus/card-44.webp){ .card-mini } 12-B Phoenix (id 44) | Brights |

<!-- END GENERATED: opponents -->

## How opponent strength works

Each opponent sets how accurately the CPU plays. This is the chance, on each decision, that the CPU picks its best-scoring card rather than a random one. Accuracy rises with each opponent:

| Difficulty | Opponent 1 (January Crane) | Opponent 12 (December Phoenix) |
|---|---:|---:|
| Easy | 20% | 60% |
| Hard | 50% | 80% |
| Master | 78% | 100% |
| [DEUS SUPREMUS](deus-supremus.md) | 100% | 100% |

The favourite card and favourite yaku also make the CPU prefer those cards and combinations. See [Difficulty](difficulty.md).
