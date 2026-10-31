# Rulesets

Xeno Koi ships five rule sets. Pick one in **Options → Ruleset**. Every ruleset uses the same deal, turn order, and matching. They differ in **how cards are scored** and in **when a Koi-Koi call is allowed**.

The Options screen changes the ruleset for future matches. [DEUS SUPREMUS](../reference/deus-supremus.md) always uses Yakuza rules.

| Rule set | Page |
|---|---|
| Nintendo (classic baseline) | [Nintendo](nintendo.md) |
| FudaWiki | [FudaWiki](fudawiki.md) |
| KK-ADD-10 | [KK-ADD-10](kk-add-10.md) |
| Tournament | [Tournament](tournament.md) |
| Yakuza | [Yakuza](yakuza.md) |

## At a glance

| Rule | Nintendo | FudaWiki | KK-ADD-10 | Tournament | Yakuza |
|---|:---:|:---:|:---:|:---:|:---:|
| Goko (5 Brights) | 10 | **15** | 10 | 10 | 10 |
| Sanko (3 Brights) | 5 | **6** | 5 | 5 | 5 |
| Viewings (Hanami/Tsukimi-zake) | Yes | Yes | Yes | **No** | **No** |
| Sake Cup counts as a Plain (Kasu) | No | No | **Yes** | No | No |
| Ino-shika-cho / Akatan / Aotan extra cards | +1 each | +1 each | Fixed 5 | +1 each | +1 each |
| Ino-shika-cho stacks with Tane | No | No | **Yes** | No | No |
| Tanzaku / Tane counts | Replaced by sets | Replaced by sets | **Stack** | Replaced by sets | Replaced by sets |
| Koi-Koi calls per player per round | 1 (then forced Shobu) | Unlimited | Unlimited | 1 (then forced Shobu) | 1 (then forced Shobu) |
| Speed minigame and match-end minipoints | No | No | **Yes** | No | No |
| Dealer's Privilege (draw pays dealer 6) | No | No | No | No | **Yes** |

The five Brights that the Goko / Sanko rows above are about:

![01-B Crane](../assets/cards/card-00.webp){ .card-thumb } ![03-B Curtain](../assets/cards/card-08.webp){ .card-thumb } ![08-B Moon](../assets/cards/card-28.webp){ .card-thumb } ![11-B Rainman](../assets/cards/card-40.webp){ .card-thumb } ![12-B Phoenix](../assets/cards/card-44.webp){ .card-thumb }

*"Replaced by sets" means the set score and the counted score are never added together. The higher one is used.*

## Sample scoring comparisons

These examples show how representative captured card groups score across the five rule sets:

<!-- BEGIN GENERATED: ruleset-facts -->

| Probe (capture pile) | Nintendo | FudaWiki | KK-ADD-10 | Tournament | Yakuza |
|---|---:|---:|---:|---:|---:|
| Goko value (5 Brights) | **10** | **15** | **10** | **10** | **10** |
| Shiko value (4 Brights, no Rain) | **8** | **8** | **8** | **8** | **8** |
| Ame-shiko value (4 Brights w/ Rain) | **7** | **7** | **7** | **7** | **7** |
| Sanko value (3 Brights, no Rain) | **5** | **6** | **5** | **5** | **5** |
| Hanami-zake enabled (Curtain + Cup) | yes | yes | yes | **no** | **no** |
| Tsukimi-zake enabled (Moon + Cup) | yes | yes | yes | **no** | **no** |
| Sake Cup counts as a Plain for Kasu (9 Plains + Cup) | **no** | **no** | yes | **no** | **no** |
| Ino-shika-cho and Tane score separately (Ino + 5 Animals) | **no, folded** (total 7) | **no, folded** (total 7) | yes (total 6) | **no, folded** (total 7) | **no, folded** (total 7) |
| Akatan + Aotan as one 10-pt set (6 Ribbons) | total 10 | total 10 | total 13 | total 10 | total 10 |
| Tanzaku window opens at 4 Ribbons (no sets) | **no, needs 5** (5 = 1 pt) | **no, needs 5** (5 = 1 pt) | yes (4 = 1 pt, 5 = 2 pt) | **no, needs 5** (5 = 1 pt) | **no, needs 5** (5 = 1 pt) |

*(all cells evaluated live from `score_captures()` in `src/rules.c`)*

<!-- END GENERATED: ruleset-facts -->
