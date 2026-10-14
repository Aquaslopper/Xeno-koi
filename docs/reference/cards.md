# Cards

The deck has **48 cards** in **12 months** of four cards each. The table below is generated from `src/cards.c`, so it always matches the game.

## Card types

| Type | Count | Scoring role |
|---|---:|---|
| Brights | 5 | Bright family (Goko, Shiko, Ame-shiko, Sanko) |
| Animals | 9 | Tane, and Ino-shika-cho (Butterflies, Boar, Deer). The Sake Cup is an animal. |
| Ribbons | 10 | Tanzaku, Akatan (Poetry), Aotan (Blue) |
| Plains | 24 | Kasu |

## Special cards

- **Rain Man (11-B):** a Bright. It changes Shiko into Ame-shiko and blocks Sanko when it is in a three-Bright pile.
- **Sake Cup (09-A):** an animal. It forms Hanami-zake with the Curtain and Tsukimi-zake with the Moon, except in Tournament and Yakuza. In KK-ADD-10 it also counts as a Plain.
- **Poetry ribbons (01, 02, 03):** the Akatan set.
- **Blue ribbons (06, 09, 10):** the Aotan set.

## Full list

<!-- BEGIN GENERATED: cards -->

| ID | Month | Card | Type | Special |
|---:|---|---|---|---|
| 0 | January (Pine) | 01-B Crane | Bright |  |
| 1 | January (Pine) | 01-RP Poetry | Ribbon | Poetry ribbon |
| 2 | January (Pine) | 01-P1 Plain | Plain |  |
| 3 | January (Pine) | 01-P2 Plain | Plain |  |
| 4 | February (Plum) | 02-A Warbler | Animal |  |
| 5 | February (Plum) | 02-RP Poetry | Ribbon | Poetry ribbon |
| 6 | February (Plum) | 02-P1 Plain | Plain |  |
| 7 | February (Plum) | 02-P2 Plain | Plain |  |
| 8 | March (Cherry) | 03-B Curtain | Bright |  |
| 9 | March (Cherry) | 03-RP Poetry | Ribbon | Poetry ribbon |
| 10 | March (Cherry) | 03-P1 Plain | Plain |  |
| 11 | March (Cherry) | 03-P2 Plain | Plain |  |
| 12 | April (Wisteria) | 04-A Cuckoo | Animal |  |
| 13 | April (Wisteria) | 04-R Ribbon | Ribbon |  |
| 14 | April (Wisteria) | 04-P1 Plain | Plain |  |
| 15 | April (Wisteria) | 04-P2 Plain | Plain |  |
| 16 | May (Iris) | 05-A Bridge | Animal |  |
| 17 | May (Iris) | 05-R Ribbon | Ribbon |  |
| 18 | May (Iris) | 05-P1 Plain | Plain |  |
| 19 | May (Iris) | 05-P2 Plain | Plain |  |
| 20 | June (Peony) | 06-A Butterflies | Animal |  |
| 21 | June (Peony) | 06-RB Blue | Ribbon | Blue ribbon |
| 22 | June (Peony) | 06-P1 Plain | Plain |  |
| 23 | June (Peony) | 06-P2 Plain | Plain |  |
| 24 | July (Clover) | 07-A Boar | Animal |  |
| 25 | July (Clover) | 07-R Ribbon | Ribbon |  |
| 26 | July (Clover) | 07-P1 Plain | Plain |  |
| 27 | July (Clover) | 07-P2 Plain | Plain |  |
| 28 | August (Pampas) | 08-B Moon | Bright |  |
| 29 | August (Pampas) | 08-A Geese | Animal |  |
| 30 | August (Pampas) | 08-P1 Plain | Plain |  |
| 31 | August (Pampas) | 08-P2 Plain | Plain |  |
| 32 | September (Chrysanthemum) | 09-A Sake Cup | Animal | Sake Cup |
| 33 | September (Chrysanthemum) | 09-RB Blue | Ribbon | Blue ribbon |
| 34 | September (Chrysanthemum) | 09-P1 Plain | Plain |  |
| 35 | September (Chrysanthemum) | 09-P2 Plain | Plain |  |
| 36 | October (Maple) | 10-A Deer | Animal |  |
| 37 | October (Maple) | 10-RB Blue | Ribbon | Blue ribbon |
| 38 | October (Maple) | 10-P1 Plain | Plain |  |
| 39 | October (Maple) | 10-P2 Plain | Plain |  |
| 40 | November (Willow) | 11-B Rainman | Bright | Rain Man |
| 41 | November (Willow) | 11-A Swallow | Animal |  |
| 42 | November (Willow) | 11-R Ribbon | Ribbon |  |
| 43 | November (Willow) | 11-P Lightning | Plain |  |
| 44 | December (Paulownia) | 12-B Phoenix | Bright |  |
| 45 | December (Paulownia) | 12-P1 Plain | Plain |  |
| 46 | December (Paulownia) | 12-P2 Plain | Plain |  |
| 47 | December (Paulownia) | 12-P2 Plain | Plain |  |

**Composition check (from `cards_validate()` invariants):** 5 Brights, 9 Animals, 10 Ribbons, 24 Plains = 48 cards.

<!-- END GENERATED: cards -->

## Notes

- Card **47** is named `12-P2 Plain` in the source, the same as card 46. It is only a label, so scoring is not affected. See [Errata & Known Issues](../errata.md).
