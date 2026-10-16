# Cards

The deck has **48 cards** in **12 months** of four cards each. The table below is generated from `src/cards.c`, so it always matches the game.

<div class="figure-zoom" id="card-in-hand"><div class="figure-zoom-scroll"><img src="../../assets/screenshots/card-tooltip.webp" width="1920" height="1080" loading="lazy" alt="A round in progress with one field card held under the cursor and its tooltip open beside it, naming the month, the suit, the type and the card."></div><a class="figure-zoom-btn" href="#card-in-hand">zoom the frame</a><a class="figure-zoom-btn" href="#">close</a></div>

*The names in this page's tables are the names the game prints. Hover a face-up card for half a
second on Easy and you get three lines it assembles at draw time: the month and the type from
tables in `src/ui_card_tooltip.c:71-77`, and the card's own name from `CARDS[]` in `src/cards.c`
with its id prefix cut off (`ui_card_tooltip.c:98`) - here `February / Plum`, `Plain / Kasu`,
`Plain`.*

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

**Rain Man and Sake Cup:**

![11-B Rainman](../assets/cards/card-40.webp){ .card-thumb } ![09-A Sake Cup](../assets/cards/card-32.webp){ .card-thumb }

*Rain Man (November Bright, left) and Sake Cup (September Animal, right).*

**Poetry ribbons — the Akatan set (January, February, March):**

![01-RP Poetry](../assets/cards/card-01.webp){ .card-thumb } ![02-RP Poetry](../assets/cards/card-05.webp){ .card-thumb } ![03-RP Poetry](../assets/cards/card-09.webp){ .card-thumb }

**Blue ribbons — the Aotan set (June, September, October):**

![06-RB Blue](../assets/cards/card-21.webp){ .card-thumb } ![09-RB Blue](../assets/cards/card-33.webp){ .card-thumb } ![10-RB Blue](../assets/cards/card-37.webp){ .card-thumb }

## Full list

<!-- BEGIN GENERATED: cards -->

| ID | Card image | Month | Card | Type | Special |
|---:|---|---|---|---|---|
| 0 | ![01-B Crane](../assets/cards/card-00.webp){ .card-thumb } | January (Pine) | 01-B Crane | Bright |  |
| 1 | ![01-RP Poetry](../assets/cards/card-01.webp){ .card-thumb } | January (Pine) | 01-RP Poetry | Ribbon | Poetry ribbon |
| 2 | ![01-P1 Plain](../assets/cards/card-02.webp){ .card-thumb } | January (Pine) | 01-P1 Plain | Plain |  |
| 3 | ![01-P2 Plain](../assets/cards/card-03.webp){ .card-thumb } | January (Pine) | 01-P2 Plain | Plain |  |
| 4 | ![02-A Warbler](../assets/cards/card-04.webp){ .card-thumb } | February (Plum) | 02-A Warbler | Animal |  |
| 5 | ![02-RP Poetry](../assets/cards/card-05.webp){ .card-thumb } | February (Plum) | 02-RP Poetry | Ribbon | Poetry ribbon |
| 6 | ![02-P1 Plain](../assets/cards/card-06.webp){ .card-thumb } | February (Plum) | 02-P1 Plain | Plain |  |
| 7 | ![02-P2 Plain](../assets/cards/card-07.webp){ .card-thumb } | February (Plum) | 02-P2 Plain | Plain |  |
| 8 | ![03-B Curtain](../assets/cards/card-08.webp){ .card-thumb } | March (Cherry) | 03-B Curtain | Bright |  |
| 9 | ![03-RP Poetry](../assets/cards/card-09.webp){ .card-thumb } | March (Cherry) | 03-RP Poetry | Ribbon | Poetry ribbon |
| 10 | ![03-P1 Plain](../assets/cards/card-10.webp){ .card-thumb } | March (Cherry) | 03-P1 Plain | Plain |  |
| 11 | ![03-P2 Plain](../assets/cards/card-11.webp){ .card-thumb } | March (Cherry) | 03-P2 Plain | Plain |  |
| 12 | ![04-A Cuckoo](../assets/cards/card-12.webp){ .card-thumb } | April (Wisteria) | 04-A Cuckoo | Animal |  |
| 13 | ![04-R Ribbon](../assets/cards/card-13.webp){ .card-thumb } | April (Wisteria) | 04-R Ribbon | Ribbon |  |
| 14 | ![04-P1 Plain](../assets/cards/card-14.webp){ .card-thumb } | April (Wisteria) | 04-P1 Plain | Plain |  |
| 15 | ![04-P2 Plain](../assets/cards/card-15.webp){ .card-thumb } | April (Wisteria) | 04-P2 Plain | Plain |  |
| 16 | ![05-A Bridge](../assets/cards/card-16.webp){ .card-thumb } | May (Iris) | 05-A Bridge | Animal |  |
| 17 | ![05-R Ribbon](../assets/cards/card-17.webp){ .card-thumb } | May (Iris) | 05-R Ribbon | Ribbon |  |
| 18 | ![05-P1 Plain](../assets/cards/card-18.webp){ .card-thumb } | May (Iris) | 05-P1 Plain | Plain |  |
| 19 | ![05-P2 Plain](../assets/cards/card-19.webp){ .card-thumb } | May (Iris) | 05-P2 Plain | Plain |  |
| 20 | ![06-A Butterflies](../assets/cards/card-20.webp){ .card-thumb } | June (Peony) | 06-A Butterflies | Animal |  |
| 21 | ![06-RB Blue](../assets/cards/card-21.webp){ .card-thumb } | June (Peony) | 06-RB Blue | Ribbon | Blue ribbon |
| 22 | ![06-P1 Plain](../assets/cards/card-22.webp){ .card-thumb } | June (Peony) | 06-P1 Plain | Plain |  |
| 23 | ![06-P2 Plain](../assets/cards/card-23.webp){ .card-thumb } | June (Peony) | 06-P2 Plain | Plain |  |
| 24 | ![07-A Boar](../assets/cards/card-24.webp){ .card-thumb } | July (Clover) | 07-A Boar | Animal |  |
| 25 | ![07-R Ribbon](../assets/cards/card-25.webp){ .card-thumb } | July (Clover) | 07-R Ribbon | Ribbon |  |
| 26 | ![07-P1 Plain](../assets/cards/card-26.webp){ .card-thumb } | July (Clover) | 07-P1 Plain | Plain |  |
| 27 | ![07-P2 Plain](../assets/cards/card-27.webp){ .card-thumb } | July (Clover) | 07-P2 Plain | Plain |  |
| 28 | ![08-B Moon](../assets/cards/card-28.webp){ .card-thumb } | August (Pampas) | 08-B Moon | Bright |  |
| 29 | ![08-A Geese](../assets/cards/card-29.webp){ .card-thumb } | August (Pampas) | 08-A Geese | Animal |  |
| 30 | ![08-P1 Plain](../assets/cards/card-30.webp){ .card-thumb } | August (Pampas) | 08-P1 Plain | Plain |  |
| 31 | ![08-P2 Plain](../assets/cards/card-31.webp){ .card-thumb } | August (Pampas) | 08-P2 Plain | Plain |  |
| 32 | ![09-A Sake Cup](../assets/cards/card-32.webp){ .card-thumb } | September (Chrysanthemum) | 09-A Sake Cup | Animal | Sake Cup |
| 33 | ![09-RB Blue](../assets/cards/card-33.webp){ .card-thumb } | September (Chrysanthemum) | 09-RB Blue | Ribbon | Blue ribbon |
| 34 | ![09-P1 Plain](../assets/cards/card-34.webp){ .card-thumb } | September (Chrysanthemum) | 09-P1 Plain | Plain |  |
| 35 | ![09-P2 Plain](../assets/cards/card-35.webp){ .card-thumb } | September (Chrysanthemum) | 09-P2 Plain | Plain |  |
| 36 | ![10-A Deer](../assets/cards/card-36.webp){ .card-thumb } | October (Maple) | 10-A Deer | Animal |  |
| 37 | ![10-RB Blue](../assets/cards/card-37.webp){ .card-thumb } | October (Maple) | 10-RB Blue | Ribbon | Blue ribbon |
| 38 | ![10-P1 Plain](../assets/cards/card-38.webp){ .card-thumb } | October (Maple) | 10-P1 Plain | Plain |  |
| 39 | ![10-P2 Plain](../assets/cards/card-39.webp){ .card-thumb } | October (Maple) | 10-P2 Plain | Plain |  |
| 40 | ![11-B Rainman](../assets/cards/card-40.webp){ .card-thumb } | November (Willow) | 11-B Rainman | Bright | Rain Man |
| 41 | ![11-A Swallow](../assets/cards/card-41.webp){ .card-thumb } | November (Willow) | 11-A Swallow | Animal |  |
| 42 | ![11-R Ribbon](../assets/cards/card-42.webp){ .card-thumb } | November (Willow) | 11-R Ribbon | Ribbon |  |
| 43 | ![11-P Lightning](../assets/cards/card-43.webp){ .card-thumb } | November (Willow) | 11-P Lightning | Plain |  |
| 44 | ![12-B Phoenix](../assets/cards/card-44.webp){ .card-thumb } | December (Paulownia) | 12-B Phoenix | Bright |  |
| 45 | ![12-P1 Plain](../assets/cards/card-45.webp){ .card-thumb } | December (Paulownia) | 12-P1 Plain | Plain |  |
| 46 | ![12-P2 Plain](../assets/cards/card-46.webp){ .card-thumb } | December (Paulownia) | 12-P2 Plain | Plain |  |
| 47 | ![12-P3 Plain](../assets/cards/card-47.webp){ .card-thumb } | December (Paulownia) | 12-P3 Plain | Plain |  |

**Composition check (from `cards_validate()` invariants):** 5 Brights, 9 Animals, 10 Ribbons, 24 Plains = 48 cards.

<!-- END GENERATED: cards -->
## The sheet this deck was drawn from

These 48 are the game's own rendering of a shape that is over two centuries old: twelve columns of
four, one column per month, the four ranks of the month down the column. This is a printed sheet of
the traditional pattern, in the same order the tables above use.

<div class="figure-zoom" id="deck-sheet"><div class="figure-zoom-scroll"><img src="../../assets/screenshots/deck-sheet.webp" width="2268" height="1190" loading="lazy" alt="A traditional hanafuda sheet: a header row naming the twelve months - Pine, Plum, Cherry, Wisteria, Iris, Peony, Clover, Pampas, Chrysanthemum, Maple, Willow, Paulownia - and under each name four cards, a bright with the month's animal and sun or moon, a ribbon with a short poem strip, and two plain cards of foliage."></div><a class="figure-zoom-btn" href="#deck-sheet">zoom the sheet</a><a class="figure-zoom-btn" href="#">close</a></div>

*The month names are the ones the game keeps, in the order `src/cards.c` lays the deck out. The
game's own sheet is `assets/atlas.png`: twelve columns of 176 x 286 cells, 2112 x 1144
(`src/ui_theme.h:41-42`), the same grid, column for column, and it is where every thumbnail in this
wiki is cut from (`tools/make_card_images.py`).*

Two things worth checking against it, because both are easy to get wrong from the rules alone.
**Columns are months, rows are not ranks.** Any two cards in a column match, so the January crane
and the January plain are the same suit to the engine and the art never enters into it - which is
also why the atlas is ordered by id, month first: a month with no bright puts its Animal in the top
row, and that is where April's Cuckoo (`id 12`) sits. Nobody would call that row "the brights".
**The ribbon colours are the real ones.** The three poetry strips - January, February, March - and
the three blue ones - June, September, October - are the same six cards on both sheets:
`01-RP`, `02-RP`, `03-RP` and `06-RB`, `09-RB`, `10-RB` in the tables above, Akatan and Aotan in
[Yaku](../scoring/yaku.md). The game renamed nothing and moved nothing; it just drew it like Giger.
