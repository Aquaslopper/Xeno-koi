# Yaku & Scoring

A **yaku** is a scoring combination you capture. Only **captured** cards score: cards on the field or in a hand do not count. A round's **base** is the sum of the yaku that apply, and the base then goes through the [multipliers](multipliers.md) to become the award.

## Yaku conditions

| Yaku | Condition | Cards |
|---|---|---|
| **Goko** (Five Brights) | All 5 Brights. | ![01-B Crane](../assets/cards/card-00.webp){ .card-mini } ![03-B Curtain](../assets/cards/card-08.webp){ .card-mini } ![08-B Moon](../assets/cards/card-28.webp){ .card-mini } ![11-B Rainman](../assets/cards/card-40.webp){ .card-mini } ![12-B Phoenix](../assets/cards/card-44.webp){ .card-mini } |
| **Shiko** (Four Brights) | 4 Brights, not including the Rain Man. | ![01-B Crane](../assets/cards/card-00.webp){ .card-mini } ![03-B Curtain](../assets/cards/card-08.webp){ .card-mini } ![08-B Moon](../assets/cards/card-28.webp){ .card-mini } ![12-B Phoenix](../assets/cards/card-44.webp){ .card-mini } |
| **Ame-shiko** (Rainy Four Brights) | The Rain Man plus 3 other Brights. | ![01-B Crane](../assets/cards/card-00.webp){ .card-mini } ![03-B Curtain](../assets/cards/card-08.webp){ .card-mini } ![08-B Moon](../assets/cards/card-28.webp){ .card-mini } ![11-B Rainman](../assets/cards/card-40.webp){ .card-mini } |
| **Sanko** (Three Brights) | 3 Brights, not including the Rain Man. | ![01-B Crane](../assets/cards/card-00.webp){ .card-mini } ![03-B Curtain](../assets/cards/card-08.webp){ .card-mini } ![08-B Moon](../assets/cards/card-28.webp){ .card-mini } |
| **Hanami-zake** (Cherry Viewing) | Curtain + Sake Cup. *Disabled in Tournament and Yakuza.* | ![03-B Curtain](../assets/cards/card-08.webp){ .card-mini } ![09-A Sake Cup](../assets/cards/card-32.webp){ .card-mini } |
| **Tsukimi-zake** (Moon Viewing) | Moon + Sake Cup. *Disabled in Tournament and Yakuza.* | ![08-B Moon](../assets/cards/card-28.webp){ .card-mini } ![09-A Sake Cup](../assets/cards/card-32.webp){ .card-mini } |
| **Ino-shika-cho** | Butterflies, Boar, and Deer. | ![06-A Butterflies](../assets/cards/card-20.webp){ .card-mini } ![07-A Boar](../assets/cards/card-24.webp){ .card-mini } ![10-A Deer](../assets/cards/card-36.webp){ .card-mini } |
| **Akatan** (Red Poetry) | The three Poetry ribbons from January, February, and March. | ![01-RP Poetry](../assets/cards/card-01.webp){ .card-mini } ![02-RP Poetry](../assets/cards/card-05.webp){ .card-mini } ![03-RP Poetry](../assets/cards/card-09.webp){ .card-mini } |
| **Aotan** (Blue Ribbons) | The three Blue ribbons from June, September, and October. | ![06-RB Blue](../assets/cards/card-21.webp){ .card-mini } ![09-RB Blue](../assets/cards/card-33.webp){ .card-mini } ![10-RB Blue](../assets/cards/card-37.webp){ .card-mini } |
| **Akatan + Aotan** | All six of the above. | ![01-RP Poetry](../assets/cards/card-01.webp){ .card-mini } ![02-RP Poetry](../assets/cards/card-05.webp){ .card-mini } ![03-RP Poetry](../assets/cards/card-09.webp){ .card-mini } ![06-RB Blue](../assets/cards/card-21.webp){ .card-mini } ![09-RB Blue](../assets/cards/card-33.webp){ .card-mini } ![10-RB Blue](../assets/cards/card-37.webp){ .card-mini } |
| **Tane** (Animals) | 5 or more Animals. The Sake Cup counts as an animal. | ![02-A Warbler](../assets/cards/card-04.webp){ .card-mini } ![Card 12](../assets/cards/card-12.webp){ .card-mini } ![Card 16](../assets/cards/card-16.webp){ .card-mini } ![Card 29](../assets/cards/card-29.webp){ .card-mini } ![Card 41](../assets/cards/card-41.webp){ .card-mini } |
| **Tanzaku** (Ribbons) | 5 or more ribbons, of any kind. (KK-ADD-10 counts from 4.) | ![01-RP Poetry](../assets/cards/card-01.webp){ .card-mini } ![Card 13](../assets/cards/card-13.webp){ .card-mini } ![Card 17](../assets/cards/card-17.webp){ .card-mini } ![Card 25](../assets/cards/card-25.webp){ .card-mini } ![Card 42](../assets/cards/card-42.webp){ .card-mini } |
| **Kasu** (Plains) | 10 or more Plains. | ![01-P1 Plain](../assets/cards/card-02.webp){ .card-mini } ![01-P2 Plain](../assets/cards/card-03.webp){ .card-mini } ![Card 6](../assets/cards/card-06.webp){ .card-mini } ![Card 7](../assets/cards/card-07.webp){ .card-mini } ![Card 10](../assets/cards/card-10.webp){ .card-mini } ![Card 11](../assets/cards/card-11.webp){ .card-mini } ![Card 14](../assets/cards/card-14.webp){ .card-mini } ![Card 15](../assets/cards/card-15.webp){ .card-mini } ![Card 18](../assets/cards/card-18.webp){ .card-mini } ![Card 19](../assets/cards/card-19.webp){ .card-mini } |

### How the Bright family works

Only **one** Bright yaku ever counts, the highest one that applies:

- 5 Brights score **Goko only**. They do not also score Shiko or Sanko.
- 3 Brights that include the Rain Man score **0**. The Rain Man blocks Sanko.
- The Viewings are **independent** of the Brights. Curtain + Moon + Sake Cup scores both Viewings (10) and no Bright yaku.

### How the set and count yaku interact

Some yaku are a **fixed set** and some are a **count** of a card family. A card is never counted twice.

- In **Nintendo, FudaWiki, Tournament, and Yakuza**, the set score replaces the count: Ino-shika-cho replaces Tane, and the ribbon sets replace Tanzaku.
- In **KK-ADD-10**, sets and counts add together.

Full comparisons are in the generated tables below.

## Points per yaku, by rule set

The values below are generated from the game's scoring code. Each row uses the smallest capture pile that qualifies for that yaku.

<!-- BEGIN GENERATED: yaku-values -->

Points each yaku scores **with the exact capture pile listed** (evaluated by the engine's own `score_captures()`):

| Yaku | Capture pile | Cards | Nintendo | FudaWiki | KK-ADD-10 | Tournament | Yakuza |
|---|---|---|---:|---:|---:|---:|---:|
| **Goko** | all 5 Brights | ![01-B Crane](../assets/cards/card-00.webp){ .card-mini } ![03-B Curtain](../assets/cards/card-08.webp){ .card-mini } ![08-B Moon](../assets/cards/card-28.webp){ .card-mini } ![11-B Rainman](../assets/cards/card-40.webp){ .card-mini } ![12-B Phoenix](../assets/cards/card-44.webp){ .card-mini } | 10 | 15 | 10 | 10 | 10 |
| **Shiko** | 4 Brights, no Rain Man | ![01-B Crane](../assets/cards/card-00.webp){ .card-mini } ![03-B Curtain](../assets/cards/card-08.webp){ .card-mini } ![08-B Moon](../assets/cards/card-28.webp){ .card-mini } ![12-B Phoenix](../assets/cards/card-44.webp){ .card-mini } | 8 | 8 | 8 | 8 | 8 |
| **Ame-shiko** | 4 Brights incl. Rain Man | ![01-B Crane](../assets/cards/card-00.webp){ .card-mini } ![03-B Curtain](../assets/cards/card-08.webp){ .card-mini } ![08-B Moon](../assets/cards/card-28.webp){ .card-mini } ![11-B Rainman](../assets/cards/card-40.webp){ .card-mini } | 7 | 7 | 7 | 7 | 7 |
| **Sanko** | 3 Brights, no Rain Man | ![01-B Crane](../assets/cards/card-00.webp){ .card-mini } ![03-B Curtain](../assets/cards/card-08.webp){ .card-mini } ![08-B Moon](../assets/cards/card-28.webp){ .card-mini } | 5 | 6 | 5 | 5 | 5 |
| **Hanami-zake** | Curtain + Sake Cup | ![03-B Curtain](../assets/cards/card-08.webp){ .card-mini } ![09-A Sake Cup](../assets/cards/card-32.webp){ .card-mini } | 5 | 5 | 5 | -- | -- |
| **Tsukimi-zake** | Moon + Sake Cup | ![08-B Moon](../assets/cards/card-28.webp){ .card-mini } ![09-A Sake Cup](../assets/cards/card-32.webp){ .card-mini } | 5 | 5 | 5 | -- | -- |
| **Ino-shika-cho** | Butterflies + Boar + Deer | ![06-A Butterflies](../assets/cards/card-20.webp){ .card-mini } ![07-A Boar](../assets/cards/card-24.webp){ .card-mini } ![10-A Deer](../assets/cards/card-36.webp){ .card-mini } | 5 | 5 | 5 | 5 | 5 |
| **Akatan** | 3 Poetry ribbons (Jan/Feb/Mar) | ![01-RP Poetry](../assets/cards/card-01.webp){ .card-mini } ![02-RP Poetry](../assets/cards/card-05.webp){ .card-mini } ![03-RP Poetry](../assets/cards/card-09.webp){ .card-mini } | 5 | 5 | 5 | 5 | 5 |
| **Aotan** | 3 Blue ribbons (Jun/Sep/Oct) | ![06-RB Blue](../assets/cards/card-21.webp){ .card-mini } ![09-RB Blue](../assets/cards/card-33.webp){ .card-mini } ![10-RB Blue](../assets/cards/card-37.webp){ .card-mini } | 5 | 5 | 5 | 5 | 5 |
| **Akatan + Aotan** | Akatan + Aotan (6 ribbons) | ![01-RP Poetry](../assets/cards/card-01.webp){ .card-mini } ![02-RP Poetry](../assets/cards/card-05.webp){ .card-mini } ![03-RP Poetry](../assets/cards/card-09.webp){ .card-mini } ![06-RB Blue](../assets/cards/card-21.webp){ .card-mini } ![09-RB Blue](../assets/cards/card-33.webp){ .card-mini } ![10-RB Blue](../assets/cards/card-37.webp){ .card-mini } | 10 | 10 | 10 | 10 | 10 |
| **Tane** | 5 Animals, Ino-shika-cho incomplete | ![02-A Warbler](../assets/cards/card-04.webp){ .card-mini } ![04-A Cuckoo](../assets/cards/card-12.webp){ .card-mini } ![05-A Bridge](../assets/cards/card-16.webp){ .card-mini } ![08-A Geese](../assets/cards/card-29.webp){ .card-mini } ![11-A Swallow](../assets/cards/card-41.webp){ .card-mini } | 1 | 1 | 1 | 1 | 1 |
| **Tanzaku** | 5 Ribbons, no poetry/blue set complete | ![01-RP Poetry](../assets/cards/card-01.webp){ .card-mini } ![04-R Ribbon](../assets/cards/card-13.webp){ .card-mini } ![05-R Ribbon](../assets/cards/card-17.webp){ .card-mini } ![07-R Ribbon](../assets/cards/card-25.webp){ .card-mini } ![11-R Ribbon](../assets/cards/card-42.webp){ .card-mini } | 1 | 1 | 2 | 1 | 1 |
| **Kasu** | 10 Plains | ![01-P1 Plain](../assets/cards/card-02.webp){ .card-mini } ![01-P2 Plain](../assets/cards/card-03.webp){ .card-mini } ![02-P1 Plain](../assets/cards/card-06.webp){ .card-mini } ![02-P2 Plain](../assets/cards/card-07.webp){ .card-mini } ![03-P1 Plain](../assets/cards/card-10.webp){ .card-mini } ![03-P2 Plain](../assets/cards/card-11.webp){ .card-mini } ![04-P1 Plain](../assets/cards/card-14.webp){ .card-mini } ![04-P2 Plain](../assets/cards/card-15.webp){ .card-mini } ![05-P1 Plain](../assets/cards/card-18.webp){ .card-mini } ![05-P2 Plain](../assets/cards/card-19.webp){ .card-mini } | 1 | 1 | 1 | 1 | 1 |

*(a `--` means the yaku cannot score at all under that ruleset)*

<!-- END GENERATED: yaku-values -->

## Stacking in practice

These rows show how a bigger capture pile scores in each rule set.

<!-- BEGIN GENERATED: stacking -->

How counting yaku and special sets stack, per ruleset (same engine evaluation):

| Capture pile | Cards | Nintendo | FudaWiki | KK-ADD-10 | Tournament | Yakuza |
|---|---|---|---|---|---|---|
| Ino-shika-cho + 2 extra Animals (5 total) | ![06-A Butterflies](../assets/cards/card-20.webp){ .card-tiny } ![07-A Boar](../assets/cards/card-24.webp){ .card-tiny } ![10-A Deer](../assets/cards/card-36.webp){ .card-tiny } ![02-A Warbler](../assets/cards/card-04.webp){ .card-tiny } ![08-A Geese](../assets/cards/card-29.webp){ .card-tiny } | Ino-shika-cho 7 = **7** | Ino-shika-cho 7 = **7** | Ino-shika-cho 5 + Tane 1 = **6** | Ino-shika-cho 7 = **7** | Ino-shika-cho 7 = **7** | 
| Akatan + 2 extra Ribbons (5 total) | ![01-RP Poetry](../assets/cards/card-01.webp){ .card-tiny } ![02-RP Poetry](../assets/cards/card-05.webp){ .card-tiny } ![03-RP Poetry](../assets/cards/card-09.webp){ .card-tiny } ![04-R Ribbon](../assets/cards/card-13.webp){ .card-tiny } ![05-R Ribbon](../assets/cards/card-17.webp){ .card-tiny } | Akatan 7 = **7** | Akatan 7 = **7** | Akatan 5 + Tanzaku 2 = **7** | Akatan 7 = **7** | Akatan 7 = **7** | 
| Akatan + Aotan + 2 extra Ribbons (8 total) | ![01-RP Poetry](../assets/cards/card-01.webp){ .card-tiny } ![02-RP Poetry](../assets/cards/card-05.webp){ .card-tiny } ![03-RP Poetry](../assets/cards/card-09.webp){ .card-tiny } ![06-RB Blue](../assets/cards/card-21.webp){ .card-tiny } ![09-RB Blue](../assets/cards/card-33.webp){ .card-tiny } ![10-RB Blue](../assets/cards/card-37.webp){ .card-tiny } ![04-R Ribbon](../assets/cards/card-13.webp){ .card-tiny } ![05-R Ribbon](../assets/cards/card-17.webp){ .card-tiny } | Akatan + Aotan 12 = **12** | Akatan + Aotan 12 = **12** | Akatan + Aotan 10 + Tanzaku 5 = **15** | Akatan + Aotan 12 = **12** | Akatan + Aotan 12 = **12** | 
| 6 Animals, no Ino-shika-cho | ![02-A Warbler](../assets/cards/card-04.webp){ .card-tiny } ![04-A Cuckoo](../assets/cards/card-12.webp){ .card-tiny } ![05-A Bridge](../assets/cards/card-16.webp){ .card-tiny } ![06-A Butterflies](../assets/cards/card-20.webp){ .card-tiny } ![08-A Geese](../assets/cards/card-29.webp){ .card-tiny } ![11-A Swallow](../assets/cards/card-41.webp){ .card-tiny } | Tane 2 = **2** | Tane 2 = **2** | Tane 2 = **2** | Tane 2 = **2** | Tane 2 = **2** | 
| 4 Ribbons, no poetry/blue set | ![04-R Ribbon](../assets/cards/card-13.webp){ .card-tiny } ![05-R Ribbon](../assets/cards/card-17.webp){ .card-tiny } ![07-R Ribbon](../assets/cards/card-25.webp){ .card-tiny } ![11-R Ribbon](../assets/cards/card-42.webp){ .card-tiny } | -- *(0)* | -- *(0)* | Tanzaku 1 = **1** | -- *(0)* | -- *(0)* | 
| 6 Ribbons, no poetry/blue set | ![01-RP Poetry](../assets/cards/card-01.webp){ .card-tiny } ![02-RP Poetry](../assets/cards/card-05.webp){ .card-tiny } ![06-RB Blue](../assets/cards/card-21.webp){ .card-tiny } ![09-RB Blue](../assets/cards/card-33.webp){ .card-tiny } ![04-R Ribbon](../assets/cards/card-13.webp){ .card-tiny } ![05-R Ribbon](../assets/cards/card-17.webp){ .card-tiny } | Tanzaku 2 = **2** | Tanzaku 2 = **2** | Tanzaku 3 = **3** | Tanzaku 2 = **2** | Tanzaku 2 = **2** | 
| 9 Plains + Sake Cup | ![01-P1 Plain](../assets/cards/card-02.webp){ .card-tiny } ![01-P2 Plain](../assets/cards/card-03.webp){ .card-tiny } ![02-P1 Plain](../assets/cards/card-06.webp){ .card-tiny } ![02-P2 Plain](../assets/cards/card-07.webp){ .card-tiny } ![03-P1 Plain](../assets/cards/card-10.webp){ .card-tiny } ![03-P2 Plain](../assets/cards/card-11.webp){ .card-tiny } ![04-P1 Plain](../assets/cards/card-14.webp){ .card-tiny } ![04-P2 Plain](../assets/cards/card-15.webp){ .card-tiny } ![05-P1 Plain](../assets/cards/card-18.webp){ .card-tiny } ![09-A Sake Cup](../assets/cards/card-32.webp){ .card-tiny } | -- *(0)* | -- *(0)* | Kasu 1 = **1** | -- *(0)* | -- *(0)* | 
| 10 Plains (no Sake Cup) | ![01-P1 Plain](../assets/cards/card-02.webp){ .card-tiny } ![01-P2 Plain](../assets/cards/card-03.webp){ .card-tiny } ![02-P1 Plain](../assets/cards/card-06.webp){ .card-tiny } ![02-P2 Plain](../assets/cards/card-07.webp){ .card-tiny } ![03-P1 Plain](../assets/cards/card-10.webp){ .card-tiny } ![03-P2 Plain](../assets/cards/card-11.webp){ .card-tiny } ![04-P1 Plain](../assets/cards/card-14.webp){ .card-tiny } ![04-P2 Plain](../assets/cards/card-15.webp){ .card-tiny } ![05-P1 Plain](../assets/cards/card-18.webp){ .card-tiny } ![05-P2 Plain](../assets/cards/card-19.webp){ .card-tiny } | Kasu 1 = **1** | Kasu 1 = **1** | Kasu 1 = **1** | Kasu 1 = **1** | Kasu 1 = **1** | 
| Curtain + Moon + Sake Cup (both Viewings, no Sanko) | ![03-B Curtain](../assets/cards/card-08.webp){ .card-tiny } ![08-B Moon](../assets/cards/card-28.webp){ .card-tiny } ![09-A Sake Cup](../assets/cards/card-32.webp){ .card-tiny } | Hanami-zake 5 + Tsukimi-zake 5 = **10** | Hanami-zake 5 + Tsukimi-zake 5 = **10** | Hanami-zake 5 + Tsukimi-zake 5 = **10** | -- *(0)* | -- *(0)* | 
| 3 Brights but one is the Rain Man (no Sanko) | ![01-B Crane](../assets/cards/card-00.webp){ .card-tiny } ![03-B Curtain](../assets/cards/card-08.webp){ .card-tiny } ![11-B Rainman](../assets/cards/card-40.webp){ .card-tiny } | -- *(0)* | -- *(0)* | -- *(0)* | -- *(0)* | -- *(0)* | 
| Mixed pile: 3 Brights + Cup + Ino-shika-cho + Akatan + 2 Plains | ![01-B Crane](../assets/cards/card-00.webp){ .card-tiny } ![03-B Curtain](../assets/cards/card-08.webp){ .card-tiny } ![08-B Moon](../assets/cards/card-28.webp){ .card-tiny } ![09-A Sake Cup](../assets/cards/card-32.webp){ .card-tiny } ![06-A Butterflies](../assets/cards/card-20.webp){ .card-tiny } ![07-A Boar](../assets/cards/card-24.webp){ .card-tiny } ![10-A Deer](../assets/cards/card-36.webp){ .card-tiny } ![01-RP Poetry](../assets/cards/card-01.webp){ .card-tiny } ![02-RP Poetry](../assets/cards/card-05.webp){ .card-tiny } ![03-RP Poetry](../assets/cards/card-09.webp){ .card-tiny } ![01-P1 Plain](../assets/cards/card-02.webp){ .card-tiny } ![01-P2 Plain](../assets/cards/card-03.webp){ .card-tiny } | Sanko 5 + Hanami-zake 5 + Tsukimi-zake 5 + Ino-shika-cho 6 + Akatan 5 = **26** | Sanko 6 + Hanami-zake 5 + Tsukimi-zake 5 + Ino-shika-cho 6 + Akatan 5 = **27** | Sanko 5 + Hanami-zake 5 + Tsukimi-zake 5 + Ino-shika-cho 5 + Akatan 5 = **25** | Sanko 5 + Ino-shika-cho 6 + Akatan 5 = **16** | Sanko 5 + Ino-shika-cho 6 + Akatan 5 = **16** | 

*(a `--` = the pile scores nothing; `+` separates stacked yaku inside one cell)*

<!-- END GENERATED: stacking -->

## Card reference

The full card list, with IDs and special flags, is on the [Cards](../reference/cards.md) page.
