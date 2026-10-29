#ifndef UI_DEALERSHIP_H
#define UI_DEALERSHIP_H

/* The dealership - one travelling token that owns "whose event is unlocked".
   Dealership.md is the specification; this header is the whole public surface.

   Stations, in track order:
       DL_PLAYER  <->  DL_DECK  <->  DL_CPU
   The deck sits between the two players, so a transfer between players is always
   two legs and the deck is always passed through.

   The token is not an ornament. Nothing at a station may act until the token has
   arrived there, and the token does not leave a station until that station's
   event has completely finished. */

enum {
    DL_NONE   = -1,
    DL_PLAYER = 0,
    DL_DECK   = 1,
    DL_CPU    = 2
};

/* Park the token at a station. Call on match start / round start. */
void dealership_reset(int station);

/* Hide the token and open every gate (menus, Speedcoin, Action Card, replay). */
void dealership_set_active(int active);

/* Advance one frame.
     station - the station whose event is next (DL_NONE: nothing to gate)
     settled - 1 when the previous station's event has completely finished;
               the token will not leave its station until this is 1           */
void dealership_update(float dt, int station, int settled);

/* 1 when the token has arrived at the station the current phase demands. */
int  dealership_unlocked(void);

/* 1 while the token is in flight between two stations. */
int  dealership_travelling(void);

/* Draw the token, its glow, its D badge and its trail. */
void dealership_draw(void);

#endif /* UI_DEALERSHIP_H */
