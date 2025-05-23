#include <netinet/in.h>
#include <raylib.h>
#include <stdint.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <raymath.h>
#include <signal.h>
#define VARIABLES
#include "game.h"

#define BUF_SIZE 1024
#define CLIENT_TIMEOUT 4
#define UPD_DELAY 15000

uint16_t ids = 0;
pthread_mutex_t lock;
Gamestate gs = {0};
volatile sig_atomic_t stop = 0;

int sockfd;
struct sockaddr_in server_addr;

void handle_sigint(int signum) {
    stop = 1;
}

int addr_equals(struct sockaddr_in *a, struct sockaddr_in *b) {
  return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}

Vector2 rand_player_pos() {
  return (Vector2){
    (rand()%10000)/9999.0 * (mapW * TILE - plsize.x*2) + plsize.x, 
    (rand()%10000)/9999.0 * (mapH * TILE - plsize.y*2) + plsize.y,
  };
}

void bullets_create(PlayerInfo plinf, uint16_t plID) {
  Vector2 cplpos = Vector2Add(plinf.pos, Vector2Scale(plsize, 0.5));
  cplpos = Vector2Add(cplpos, Vector2Scale(plinf.sdir, plgunsize));
  gs.bullets[gs.bulletsCnt++] = (Bullet) {
    .pos = cplpos,
    .dir = plinf.sdir,
    .plID = plID,
  };
}

void bullets_delete(int *i) {
  gs.bullets[*i] = gs.bullets[gs.bulletsCnt-1];
  gs.bulletsCnt--;
  (*i)--;
}

void bullets_shot(int *bi) {
  Bullet b = gs.bullets[*bi];
  for (int i = 0; i < gs.clientsCnt; i++) {
    if (gs.clients[i].plinf.pos.x + plsize.x > b.pos.x && gs.clients[i].plinf.pos.y + plsize.y > b.pos.y && 
      gs.clients[i].plinf.pos.x < b.pos.x + (BULLET_RADIUS/2.0) && gs.clients[i].plinf.pos.y < b.pos.y + (BULLET_RADIUS/2.0)) {
      bullets_delete(bi);
      if (gs.clients[i].id == b.plID) return;
      gs.clients[i].plinf.health -= 34;
      if (gs.clients[i].plinf.health <= 0) {
        gs.clients[i].plinf.health = 100;
        gs.clients[i].plinf.pos = rand_player_pos();
      }
    }
  }
}

void *bullets_update(void *arg) {
  while (1) {
    pthread_mutex_lock(&lock);

    for (int i = 0; i < gs.bulletsCnt; i++) {
      gs.bullets[i].pos = Vector2Add(Vector2Scale(gs.bullets[i].dir, BULLET_SPEED), gs.bullets[i].pos);
      Vector2 cpos = Vector2Scale(gs.bullets[i].pos, 1.0/TILE);
      if (map[(int)cpos.y * mapW + (int)cpos.x] == '#') { 
        bullets_delete(&i);
      }
      bullets_shot(&i);
    }

    pthread_mutex_unlock(&lock);  

    usleep(UPD_DELAY / 2);
  }
  return NULL;
}

void map_collision(PlayerInfo *plinf) {
  int py = (int)(plinf->pos.y / TILE);
  int px = (int)(plinf->pos.x / TILE);
  int pyh = py + (int)(plsize.y / TILE);
  int pxw = px + (int)(plsize.x / TILE);

  for (int i = ((py-2) < 0 ? 0 : (py-2)); i < ((pyh+2) > mapH ? mapH : (pyh+2)); i++) {
    for (int j = ((px-2) < 0 ? 0 : (px-2)); j < ((pxw+2) > mapW ? mapW : (pxw+2)); j++) {
      if (map[i * mapW + j] == '#') {
        Vector2 tpos = {j * TILE, i * TILE};
        if (plinf->pos.x + plsize.x > tpos.x && plinf->pos.y + plsize.y > tpos.y && 
            plinf->pos.x < tpos.x + TILE && plinf->pos.y < tpos.y + TILE) {
          Vector2 dif = Vector2Subtract(
            Vector2Add(tpos, (Vector2){TILE/2.0, TILE/2.0}), 
            Vector2Add(plinf->pos, (Vector2){plsize.x/2.0, plsize.y/2.0})
          );

          dif.x *= plsize.y / plsize.x;
          if (fabs(dif.x) > fabs(dif.y)) {
            if (dif.x > 0) plinf->pos.x = tpos.x - plsize.x;
            else plinf->pos.x = tpos.x + TILE;
          } else {
            if (dif.y > 0) plinf->pos.y = tpos.y - plsize.y;
            else plinf->pos.y = tpos.y + TILE;
          }
        }
      }
    }
  } 
}

void update_client(struct sockaddr_in *client_addr, UpdPlayerInfo updInfo) {
  time_t now = time(NULL);

  for (int i = 0; i < gs.clientsCnt; i++) {
    if (addr_equals(&gs.clients[i].addr, client_addr)) {
      gs.clients[i].last_seen = now;
      gs.clients[i].plinf.pos = Vector2Add(gs.clients[i].plinf.pos, updInfo.dpos);
      gs.clients[i].plinf.sdir = Vector2Normalize(Vector2Subtract(updInfo.sdir,
        Vector2Add(gs.clients[i].plinf.pos, Vector2Scale(plsize, 0.5))));
      map_collision(&gs.clients[i].plinf);
      if (updInfo.isShooting) bullets_create(gs.clients[i].plinf, gs.clients[i].id); 
      return;
    }
  }

  if (gs.clientsCnt < MAX_CLIENTS) {
    gs.clients[gs.clientsCnt++] = (Client){
      .id = ids++,
      .addr = *client_addr,
      .last_seen = now,
      .plinf = (PlayerInfo){
        .health = 100,
        .pos = rand_player_pos(),
      },
    };
    printf("New client added: %s:%d\n", inet_ntoa(client_addr->sin_addr),
        ntohs(client_addr->sin_port));
    return;
  }

  printf("Client list full, cannot add new client.\n");
}

void remove_inactive_clients() {
  time_t now = time(NULL);
  for (int i = 0; i < gs.clientsCnt; i++) {
    if (now - gs.clients[i].last_seen > CLIENT_TIMEOUT) {
      printf("Removing inactive client: %s:%d\n",
             inet_ntoa(gs.clients[i].addr.sin_addr),
             ntohs(gs.clients[i].addr.sin_port));
      gs.clients[i] = gs.clients[gs.clientsCnt-1]; 
      gs.clientsCnt--;
      i--;
    }
  }
}

void *gamestate_receive(void *arg) {
  char buffer[BUF_SIZE];

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  while (1) {
    int n = recvfrom(sockfd, buffer, BUF_SIZE, 0,
                     (struct sockaddr *)&client_addr, &addr_len);
    if (n < 0)
      continue;

    UpdPlayerInfo updInf;
    memcpy(&updInf, buffer, sizeof(UpdPlayerInfo));

    pthread_mutex_lock(&lock);
    update_client(&client_addr, updInf);
    pthread_mutex_unlock(&lock);
  }

  return NULL;
}

void *gamestate_send(void *arg) {
  char buffer[BUF_SIZE];

  while (1) {
    for (int i = 0; i < gs.clientsCnt; i++) {
      pthread_mutex_lock(&lock);
      remove_inactive_clients();
      memcpy(buffer, &gs, sizeof(Gamestate));
      pthread_mutex_unlock(&lock);
 
      socklen_t addr_len = sizeof(gs.clients[i].addr);
      sendto(sockfd, buffer, sizeof(Gamestate), 0, (struct sockaddr *)&gs.clients[i].addr, addr_len);
    }

    usleep(UPD_DELAY);
  }

  return NULL;
}

void socket_initialization(char *argv[]) {
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("Socket failed");
    exit(EXIT_FAILURE);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(atoi(argv[1]));
}

int main(int argc, char *argv[]) {
  srand(time(0));
  signal(SIGINT, handle_sigint);

  if (argc < 1) {
    printf("Usage: [PORT]\n");
    exit(0);
  }

  socket_initialization(argv);

  printf("Gamestate size (bytes): %zu\n", sizeof(Gamestate));

  if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("Bind failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  gs.clientsCnt = 0;
  gs.bulletsCnt = 0;

  pthread_t bullets_update_thread;
  pthread_t gamestate_receive_thread;
  pthread_t gamestate_send_thread;
  pthread_mutex_init(&lock, NULL);
  pthread_create(&bullets_update_thread, NULL, bullets_update, NULL);
  pthread_create(&gamestate_receive_thread, NULL, gamestate_receive, NULL);
  pthread_create(&gamestate_send_thread, NULL, gamestate_send, NULL);

  printf("UDP server listening on port %s...\n", argv[1]);

  while (!stop) {}

  printf("Closing server...\n");

  pthread_cancel(bullets_update_thread);
  pthread_cancel(gamestate_receive_thread);
  pthread_cancel(gamestate_send_thread);
  pthread_mutex_destroy(&lock);
  close(sockfd);
  return 0;
}
