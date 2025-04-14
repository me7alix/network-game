#include <time.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <raylib.h>
#include <raymath.h>
#define VARIABLES
#include "game.h"

#define BUF_SIZE 1024

int sockfd;
struct sockaddr_in server_addr;
socklen_t addr_len = sizeof(server_addr);

Gamestate gs = {0};
pthread_mutex_t lock;

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

void gamestate_draw() {
  pthread_mutex_lock(&lock);
  char buf[16];

  for (int i = 0; i < gs.bulletsCnt; i++) {
    DrawCircleV(gs.bullets[i].pos, BULLET_RADIUS, WHITE);
  }

  for (int i = 0; i < gs.clientsCnt; i++) {
    Vector2 plhpos = Vector2Add(gs.clients[i].plinf.pos, Vector2Scale(plsize, 0.5));
    DrawLineEx(plhpos, Vector2Add(Vector2Scale(gs.clients[i].plinf.sdir, plgunsize), plhpos), 10.0, WHITE);
    Color clr = plcolors[gs.clients[i].id % (sizeof(plcolors) / sizeof(plcolors[0]))];
    DrawRectangleV(gs.clients[i].plinf.pos, plsize, clr);
    sprintf(buf, "%d", gs.clients[i].plinf.health);
    DrawText(buf, gs.clients[i].plinf.pos.x, gs.clients[i].plinf.pos.y, 24, BLACK);
  }

  pthread_mutex_unlock(&lock);
}

Vector2 get_deltapos() {
  Vector2 dpos = {0, 0};
  if (IsKeyDown(KEY_W)) dpos.y -= 1.0;
  if (IsKeyDown(KEY_S)) dpos.y += 1.0;
  if (IsKeyDown(KEY_D)) dpos.x += 1.0;
  if (IsKeyDown(KEY_A)) dpos.x -= 1.0;
  return Vector2Scale(Vector2Normalize(dpos), GetFrameTime() * PLAYER_SPEED);
}

void draw_map() {
  for (int i = 0; i < mapH; i++) {
    for (int j = 0; j < mapW; j++) {
      if (map[i * mapW + j] == '#') {
        DrawRectangle(j * TILE, i * TILE, TILE, TILE, GRAY);
      }
    }
  }
}

void socket_initialization(char *argv[]) {
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(argv[2]));
  server_addr.sin_addr.s_addr = inet_addr(argv[1]);
}

int main(int argc, char *argv[]) {
  srand(time(0));
  char buffer[BUF_SIZE];

  if (argc < 2) {
    printf("Usage: [IP] [PORT]\n");
    exit(0);
  }

  socket_initialization(argv);
  
  pthread_t gamestateReceiveThread; 
  pthread_mutex_init(&lock, NULL);
  pthread_create(&gamestateReceiveThread, NULL, gamestate_receive, NULL);

  InitWindow(mapW * TILE, mapH * TILE, "client");
  SetTargetFPS(60);

  float shootingTimer = 0.0;
  while (!WindowShouldClose()) {
    shootingTimer += GetFrameTime();

    UpdPlayerInfo upi = {get_deltapos(), (Vector2) {0}, false};

    upi.sdir = GetMousePosition();

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && shootingTimer > 0.5) {
      upi.isShooting = true;
      shootingTimer = 0.0; 
    }
    
    memcpy(buffer, &upi, sizeof(UpdPlayerInfo));
    sendto(sockfd, buffer, sizeof(UpdPlayerInfo), 0, (const struct sockaddr *)&server_addr,
           addr_len); 
 
    BeginDrawing();
    ClearBackground(BLACK);

    gamestate_draw();
    draw_map();

    EndDrawing();
  }

  pthread_cancel(gamestateReceiveThread);
  close(sockfd);
  CloseWindow();
  return 0;
}
