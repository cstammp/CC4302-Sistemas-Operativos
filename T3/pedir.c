#include <stdio.h>
#include <pthread.h>

#include "pss.h"
#include "pedir.h"

typedef struct {
    int ready;
    pthread_cond_t w;
} Request;

// Defina aca sus variables globales
pthread_mutex_t m;
int busy = 0;
int c = 0;
Queue *q0;
Queue *q1;

void iniciar() {
    q0 = makeQueue();     
    q1 = makeQueue();     
    pthread_mutex_init(&m, NULL);       
}

void terminar() {
    destroyQueue(q0);              
    destroyQueue(q1);              
    pthread_mutex_destroy(&m);     
}

void pedir(int cat) {
  pthread_mutex_lock(&m);
  if (busy) {
      if (cat == 0) {
            Request req= {0, PTHREAD_COND_INITIALIZER};
            put(q0, &req);
            while (!req.ready) {
                pthread_cond_wait(&req.w, &m);
            } 
      } else {
          Request req= {0, PTHREAD_COND_INITIALIZER};
            put(q1, &req);
            while (!req.ready) {
                pthread_cond_wait(&req.w, &m);
            }
      }      
  }
  c = cat;
  busy = 1; 
  pthread_mutex_unlock(&m);
}

void devolver() {
    pthread_mutex_lock(&m);
    busy = 0;
    Request *pr = NULL;
    // Selecciona de qué cola sacar la solicitud según el valor de 'c'
    if (c == 0) {
        if (!emptyQueue(q1)) pr = get(q1);
        else if (!emptyQueue(q0)) pr = get(q0);
    } else {
        if (!emptyQueue(q0)) pr = get(q0);
        else if (!emptyQueue(q1)) pr = get(q1);
    }

    // Si se encuentra una solicitud en la cola, se señala
    if (pr != NULL) {
        pr->ready = 1;
        pthread_cond_signal(&pr->w);
        busy = 1;
    }

    pthread_mutex_unlock(&m);
}
