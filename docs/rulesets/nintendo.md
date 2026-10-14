# Nintendo (Classic)

The **default baseline**. Nintendo is the reference point the other rule sets are compared against.

## Summary

- Standard Koi-Koi scoring as described on the [Yaku & Scoring](../scoring/yaku.md) page.
- Viewings (Hanami-zake and Tsukimi-zake) are **on**.
- Ino-shika-cho, Akatan, and Aotan score a base of 5 and gain **+1 for every other animal or ribbon** you hold (any animal or ribbon counts, not just the set's own).
- Tane and Tanzaku score **only when no set applies**. The set score replaces the counted score, so the two never stack.
- Kasu scores from the **10th Plain** onward, and the Sake Cup does not count as a Plain.
- **One Koi-Koi call per player per round.** After your first call, your next score increase forces an automatic Shobu.

## Scoring at a glance

| Yaku | Points |
|---|---|
| Goko (5 Brights) | 10 |
| Shiko (4 Brights, no Rain Man) | 8 |
| Ame-shiko (4 Brights with Rain Man) | 7 |
| Sanko (3 Brights, no Rain Man) | 5 |
| Hanami-zake (Curtain + Sake Cup) | 5 |
| Tsukimi-zake (Moon + Sake Cup) | 5 |
| Ino-shika-cho (Butterflies, Boar, Deer) | 5, +1 per other animal |
| Akatan (3 Poetry ribbons) | 5, +1 per other ribbon |
| Aotan (3 Blue ribbons) | 5, +1 per other ribbon |
| Akatan + Aotan (all 6) | 10, +1 per other ribbon |
| Tane (5+ animals) | n − 4, only if Ino-shika-cho does not apply |
| Tanzaku (5+ ribbons) | n − 4, only if a ribbon set does not apply |
| Kasu (10+ Plains) | k − 9 |

The exact values, with examples generated from the engine, are on the [Yaku & Scoring](../scoring/yaku.md) page.

## What is special about Nintendo

**Koi-Koi limit.** Each player may call Koi-Koi once per round. After that, the next time that player's score goes up, the round ends automatically with a Shobu. This is the main strategic difference from FudaWiki and KK-ADD-10.

**Set vs count.** A player who captures Ino-shika-cho plus extra animals scores the Ino-shika-cho value, which already includes the extra animals. The Tane score is not added on top.

## Example

You capture Curtain, Moon, and Sake Cup (plus two Plains):

![03-B Curtain](../assets/cards/card-08.webp){ .card-thumb } ![08-B Moon](../assets/cards/card-28.webp){ .card-thumb } ![09-A Sake Cup](../assets/cards/card-32.webp){ .card-thumb }
 That is Hanami-zake and Tsukimi-zake, which is 10 points total. Nintendo allows both Viewings, so this scores 10.

## Related

- [Rule set comparison](index.md)
- [Multipliers](../scoring/multipliers.md)
