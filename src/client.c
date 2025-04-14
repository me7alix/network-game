#include <stdbool.h>
#define DEFS
#include "game.h"
#include <time.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <raylib.h>
#include <raymath.h>

#define PORT 38568
#define BUF_SIZE 2048

Gamestate gs = {0};
pthread_mutex_t lock;

int sockfd;
struct sockaddr_in server_addr;
socklen_t addr_len = sizeof(server_addr);

PlayerInfo plinf = {
  .pos = (Vector2){100, 100},
  .sdir = (Vector2) {0, 1.0},
};

int addr_equals(struct sockaddr_in *a, struct sockaddr_in *b) {
  return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}

Vector2 player_get_dpos() {
  Vector2 dpos = {0, 0};
  if (IsKeyDown(KEY_W)) dpos.y -= 1.0;
  if (IsKeyDown(KEY_S)) dpos.y += 1.0;
  if (IsKeyDown(KEY_D)) dpos.x += 1.0;
  if (IsKeyDown(KEY_A)) dpos.x -= 1.0;
  return Vector2Scale(Vector2Normalize(dpos), GetFrameTime() * PLAYER_SPEED);
}

void map_draw() {
  for (int i = 0; i < mapH; i++) {
    for (int j = 0; j < mapW; j++) {
      if (map[i * mapW + j] == '#') {
        DrawRectangle(j * TILE, i * TILE, TILE, TILE, GRAY);
      }
    }
  }
}

void *gamestate_receive(void *arg) {
  char buffer[BUF_SIZE];

  while (1) {
    int n = recvfrom(sockfd, buffer, sizeof(Gamestate), 0,
                     (struct sockaddr *)&server_addr, &addr_len);

    pthread_mutex_lock(&lock);
    memcpy(&gs, buffer, sizeof(Gamestate));
    pthread_mutex_unlock(&lock);
  }

  return NULL;
}

int main(int argc, char * argv[]) {
  srand(time(0));
  char buffer[BUF_SIZE];

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  if (argc > 1) 
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
  
  pthread_t gamestate_receive_thread;
  
  if (pthread_mutex_init(&lock, NULL) != 0) {
    printf("Mutex initialization error\n");
    exit(1);
  }

  pthread_create(&gamestate_receive_thread, NULL, gamestate_receive, NULL);

  InitWindow(mapW * TILE, mapH * TILE, "client");
  SetTargetFPS(30);

  float stm = 0.0;

  while (!WindowShouldClose()) {
    stm += GetFrameTime();

    UpdPlayerInfo upi = {player_get_dpos(), (Vector2) {0}, false};

    upi.sdir = GetMousePosition();

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && stm > 0.5) {
      upi.isShooting = true;
      stm = 0.0; 
    }
    
    memcpy(buffer, &upi, sizeof(UpdPlayerInfo));
    sendto(sockfd, buffer, sizeof(UpdPlayerInfo), 0, (const struct sockaddr *)&server_addr,
           addr_len); 
 
    BeginDrawing();
    ClearBackground(BLACK);

    pthread_mutex_lock(&lock);

    for (int i = 0; i < gs.bulletsCnt; i++) {
      DrawCircleV(gs.bullets[i].pos, BULLET_RADIUS, WHITE);
    }

    for (int i = 0; i < gs.clientsCnt; i++) {
      Vector2 plhpos = Vector2Add(gs.clients[i].plinf.pos, Vector2Scale(plsize, 0.5));
      DrawLineEx(plhpos, Vector2Add(Vector2Scale(gs.clients[i].plinf.sdir, plgunsize), plhpos), 10.0, WHITE);
      Color clr = plcolors[gs.clients[i].id % (sizeof(plcolors) / sizeof(plcolors[0]))];
      DrawRectangleV(gs.clients[i].plinf.pos, plsize, clr);
      sprintf(buffer, "%d", gs.clients[i].plinf.health);
      DrawText(buffer, gs.clients[i].plinf.pos.x, gs.clients[i].plinf.pos.y, 24, BLACK);
    }

    pthread_mutex_unlock(&lock);

    map_draw();

    EndDrawing();
  }

  close(sockfd);
  CloseWindow();
  return 0;
}
