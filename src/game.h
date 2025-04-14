#ifndef GAMESTATE_H_
#define GAMESTATE_H_

#define MAX_CLIENTS 4
#define MAX_BULLETS (MAX_CLIENTS * 5)
#define BULLET_SPEED 6.0
#define BULLET_RADIUS 5
#define PLAYER_SPEED 200
#define TILE 24

#include <arpa/inet.h>
#include <raylib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  Vector2 dpos, sdir;
  bool isShooting;
} UpdPlayerInfo;

typedef struct {
  Vector2 pos;
  Vector2 sdir;
  int16_t health;
} PlayerInfo;

typedef struct {
  uint16_t id;
  struct sockaddr_in addr;
  PlayerInfo plinf;
  time_t last_seen;
} Client;

typedef struct {
 Vector2 pos, dir;
 char plID;
} Bullet;

typedef struct {
  Client clients[MAX_CLIENTS];
  int clientsCnt;
  Bullet bullets[MAX_BULLETS];
  int bulletsCnt;
} Gamestate;

#endif

#ifdef VARIABLES

const int mapW = 30, mapH = 30;
char map[] = 
  "##############################"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#     #####                  #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#          #                 #"
  "#          #                 #"
  "#          #   ######        #"
  "#          #                 #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "#                            #"
  "##############################";

Color plcolors[] = {
  RED, BLUE, GREEN, YELLOW, ORANGE,
};

float plgunsize = 35;
Vector2 plsize = {35, 35};

#endif
