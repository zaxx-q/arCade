/*
 * player.h  -  the golf ball and the 3-state "aim -> power -> shoot" machine
 *
 *   MOVING --action--> AIMING --action--> POWER --action--> MOVING (launched)
 *
 *   [T13 Struct]  Player contains another struct (Ball) and arrays (trail)
 *   [T13 Enum]    BallState selects the behaviour in a switch statement
 */
#ifndef GG_PLAYER_H
#define GG_PLAYER_H

#include "common.h"
#include "physics.h"

typedef struct {
    Ball      ball;                 /* position + velocity                    */
    BallState state;
    float     aim_angle;            /* radians, 0 = pointing right            */
    float     power;                /* 0..100                                  */
    bool      power_growing;        /* direction of the ping-pong meter       */
    int       strikes;              /* shots left (0..3)                      */
    int       strokes;              /* shots fired since the last reset       */
    Vec2      spawn;                /* where the ball returns to on reset     */

    /* [T09 Array] motion trail stored in a RING BUFFER */
    Vec2      trail[TRAIL_LEN];
    int       trail_head;           /* index of the next slot to write        */
    int       trail_count;          /* how many slots hold valid data         */
    int       trail_tick;           /* sample every 2nd physics tick          */
} Player;

void     Player_Init(Player *p, Vec2 spawn);          /* [T10] p is a POINTER, */
void     Player_Reset(Player *p);                     /*       so the function */
void     Player_RefillStrikes(Player *p);             /*       can modify it   */
unsigned Player_Update(Player *p, const Level *lv, bool action, float dt);

int      Player_TrailCount(const Player *p);
Vec2     Player_TrailPoint(const Player *p, int i);   /* i = 0 is the oldest   */

#endif /* GG_PLAYER_H */
