#ifndef BOARD_LAYOUT_H
#define BOARD_LAYOUT_H

/* Pure (raylib-free) geometry of the field table.
   The painter and every hit test go through these functions, so the slot a card is PAINTED in
   and the slot a click is TESTED against can never drift apart. */

/* Field slots in paint order: sorted by month, ties broken by position in the field array
   (this is the "sorted by month, then id" order of the handoff spec). */
void board_field_sort_slots(const int *field, int n, int *slots);

/* Top-left corner of slot `index` on a field that holds `total_cards` cards. */
void board_field_slot_xy(int index, int total_cards, float *x, float *y);

/* Slot a card is painted in.  If the card is not on the field yet (the revealed stock card)
   it is treated as appended by the engine, exactly as engine_reveal_stock() does, and
   *out_total reports the field size that goes with the returned slot.  Returns -1 on error. */
int board_field_slot_of(int card_id, const int *field, int n, int *out_total);

#endif /* BOARD_LAYOUT_H */
