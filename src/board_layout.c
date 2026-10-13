#include "board_layout.h"
#include "cards.h"
#include "ui_theme.h"

void board_field_sort_slots(const int *field, int n, int *slots) {
    for (int i = 0; i < n; i++) slots[i] = i;
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            int a = slots[i], b = slots[j];
            if (CARDS[field[a]].month > CARDS[field[b]].month ||
                (CARDS[field[a]].month == CARDS[field[b]].month && a > b)) {
                slots[i] = b; slots[j] = a;
            }
        }
    }
}

void board_field_slot_xy(int index, int total_cards, float *x, float *y) {
    if (total_cards <= 0) { *x = FIELD_X; *y = 170.0f; return; }
    int top_count = (total_cards + 1) / 2, bot_count = total_cards - top_count;
    int row = (index < top_count) ? 0 : 1, col = (row == 0) ? index : (index - top_count);
    int row_count = (row == 0) ? top_count : bot_count;
    float step = 96.0f, total_w = (row_count > 0) ? ((row_count - 1) * step + FIELD_CARD_W) : FIELD_CARD_W;
    if (total_w > (FIELD_W - 20.0f) && row_count > 1) {
        step = (FIELD_W - 20.0f - FIELD_CARD_W) / (float)(row_count - 1);
        total_w = (row_count - 1) * step + FIELD_CARD_W;
    }
    *x = FIELD_X + (FIELD_W - total_w) / 2.0f + col * step;
    *y = (row == 0) ? 170.0f : 310.0f;
}

int board_field_slot_of(int card_id, const int *field, int n, int *out_total) {
    if (card_id < 0 || card_id >= 48 || n < 0 || n > 48) return -1;
    int tmp[49], slots[49], total = n, on_field = 0;
    for (int i = 0; i < n; i++) {
        tmp[i] = field[i];
        if (tmp[i] == card_id) on_field = 1;
    }
    if (!on_field) tmp[total++] = card_id;   /* the engine appends the drawn card at the end */
    board_field_sort_slots(tmp, total, slots);
    if (out_total) *out_total = total;
    for (int i = 0; i < total; i++) {
        if (tmp[slots[i]] == card_id) return i;
    }
    return -1;
}
