# Yaku & Scoring

A **yaku** is a scoring combination you capture. Only **captured** cards score: cards on the field or in a hand do not count. A round's **base** is the sum of the yaku that apply, and the base then goes through the [multipliers](multipliers.md) to become the award.

## Yaku conditions

| Yaku | Condition |
|---|---|
| **Goko** (Five Brights) | All 5 Brights. |
| **Shiko** (Four Brights) | 4 Brights, not including the Rain Man. |
| **Ame-shiko** (Rainy Four Brights) | The Rain Man plus 3 other Brights. |
| **Sanko** (Three Brights) | 3 Brights, not including the Rain Man. |
| **Hanami-zake** (Cherry Viewing) | Curtain + Sake Cup. *Disabled in Tournament and Yakuza.* |
| **Tsukimi-zake** (Moon Viewing) | Moon + Sake Cup. *Disabled in Tournament and Yakuza.* |
| **Ino-shika-cho** | Butterflies, Boar, and Deer. |
| **Akatan** (Red Poetry) | The three Poetry ribbons from January, February, and March. |
| **Aotan** (Blue Ribbons) | The three Blue ribbons from June, September, and October. |
| **Akatan + Aotan** | All six of the above. |
| **Tane** (Animals) | 5 or more Animals. The Sake Cup counts as an animal. |
| **Tanzaku** (Ribbons) | 5 or more ribbons, of any kind. (KK-ADD-10 counts from 4.) |
| **Kasu** (Plains) | 10 or more Plains. |

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

| Yaku | Capture pile | Nintendo | FudaWiki | KK-ADD-10 | Tournament | Yakuza |
|---|---|---:|---:|---:|---:|---:|
| **Goko** | all 5 Brights | 10 | 15 | 10 | 10 | 10 |
| **Shiko** | 4 Brights, no Rain Man | 8 | 8 | 8 | 8 | 8 |
| **Ame-shiko** | 4 Brights incl. Rain Man | 7 | 7 | 7 | 7 | 7 |
| **Sanko** | 3 Brights, no Rain Man | 5 | 6 | 5 | 5 | 5 |
| **Hanami-zake** | Curtain + Sake Cup | 5 | 5 | 5 | -- | -- |
| **Tsukimi-zake** | Moon + Sake Cup | 5 | 5 | 5 | -- | -- |
| **Ino-shika-cho** | Butterflies + Boar + Deer | 5 | 5 | 5 | 5 | 5 |
| **Akatan** | 3 Poetry ribbons (Jan/Feb/Mar) | 5 | 5 | 5 | 5 | 5 |
| **Aotan** | 3 Blue ribbons (Jun/Sep/Oct) | 5 | 5 | 5 | 5 | 5 |
| **Akatan + Aotan** | Akatan + Aotan (6 ribbons) | 10 | 10 | 10 | 10 | 10 |
| **Tane** | 5 Animals, Ino-shika-cho incomplete | 1 | 1 | 1 | 1 | 1 |
| **Tanzaku** | 5 Ribbons, no poetry/blue set complete | 1 | 1 | 2 | 1 | 1 |
| **Kasu** | 10 Plains | 1 | 1 | 1 | 1 | 1 |

*(a `--` means the yaku cannot score at all under that ruleset)*

<!-- END GENERATED: yaku-values -->

## Stacking in practice

These rows show how a bigger capture pile scores in each rule set.

<!-- BEGIN GENERATED: stacking -->

How counting yaku and special sets stack, per ruleset (same engine evaluation):

| Capture pile | Nintendo | FudaWiki | KK-ADD-10 | Tournament | Yakuza |
|---|---|---|---|---|---|
| Ino-shika-cho + 2 extra Animals (5 total) | Ino-shika-cho 7 = **7** | Ino-shika-cho 7 = **7** | Ino-shika-cho 5 + Tane 1 = **6** | Ino-shika-cho 7 = **7** | Ino-shika-cho 7 = **7** | 
| Akatan + 2 extra Ribbons (5 total) | Akatan 7 = **7** | Akatan 7 = **7** | Akatan 5 + Tanzaku 2 = **7** | Akatan 7 = **7** | Akatan 7 = **7** | 
| Akatan + Aotan + 2 extra Ribbons (8 total) | Akatan + Aotan 12 = **12** | Akatan + Aotan 12 = **12** | Akatan + Aotan 10 + Tanzaku 5 = **15** | Akatan + Aotan 12 = **12** | Akatan + Aotan 12 = **12** | 
| 6 Animals, no Ino-shika-cho | Tane 2 = **2** | Tane 2 = **2** | Tane 2 = **2** | Tane 2 = **2** | Tane 2 = **2** | 
| 4 Ribbons, no poetry/blue set | -- *(0)* | -- *(0)* | Tanzaku 1 = **1** | -- *(0)* | -- *(0)* | 
| 6 Ribbons, no poetry/blue set | Tanzaku 2 = **2** | Tanzaku 2 = **2** | Tanzaku 3 = **3** | Tanzaku 2 = **2** | Tanzaku 2 = **2** | 
| 9 Plains + Sake Cup | -- *(0)* | -- *(0)* | Kasu 1 = **1** | -- *(0)* | -- *(0)* | 
| 10 Plains (no Sake Cup) | Kasu 1 = **1** | Kasu 1 = **1** | Kasu 1 = **1** | Kasu 1 = **1** | Kasu 1 = **1** | 
| Curtain + Moon + Sake Cup (both Viewings, no Sanko) | Hanami-zake 5 + Tsukimi-zake 5 = **10** | Hanami-zake 5 + Tsukimi-zake 5 = **10** | Hanami-zake 5 + Tsukimi-zake 5 = **10** | -- *(0)* | -- *(0)* | 
| 3 Brights but one is the Rain Man (no Sanko) | -- *(0)* | -- *(0)* | -- *(0)* | -- *(0)* | -- *(0)* | 
| Mixed pile: 3 Brights + Cup + Ino-shika-cho + Akatan + 2 Plains | Sanko 5 + Hanami-zake 5 + Tsukimi-zake 5 + Ino-shika-cho 6 + Akatan 5 = **26** | Sanko 6 + Hanami-zake 5 + Tsukimi-zake 5 + Ino-shika-cho 6 + Akatan 5 = **27** | Sanko 5 + Hanami-zake 5 + Tsukimi-zake 5 + Ino-shika-cho 5 + Akatan 5 = **25** | Sanko 5 + Ino-shika-cho 6 + Akatan 5 = **16** | Sanko 5 + Ino-shika-cho 6 + Akatan 5 = **16** | 

*(a `--` = the pile scores nothing; `+` separates stacked yaku inside one cell)*

<!-- END GENERATED: stacking -->

## Card reference

The full card list, with IDs and special flags, is on the [Cards](../reference/cards.md) page.
