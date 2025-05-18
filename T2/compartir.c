#include <pthread.h>
#include "compartir.h"

// Defina aca sus variables globales
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t w = PTHREAD_COND_INITIALIZER;

void *data = NULL;  // Datos compartidos 
int shared = 0;     // Indica si los datos estan siendo compartidos

void compartir(void *ptr) {
    pthread_mutex_lock(&m);
        while (shared) {
            pthread_cond_wait(&w,&m);
        }
        data = ptr;
        pthread_cond_broadcast(&w);
        while (data != NULL) {
            pthread_cond_wait(&w, &m);
        }
    pthread_mutex_unlock(&m);
}

void *acceder(void) {
    pthread_mutex_lock(&m);
        while (data == NULL) {
            pthread_cond_wait(&w,&m);
        }
        shared++;
    pthread_mutex_unlock(&m);   
    return data;
}

void devolver(void) {
    pthread_mutex_lock(&m);
        shared--;
        if (shared == 0) {
            data = NULL;
            pthread_cond_signal(&w);
        }   
    pthread_mutex_unlock(&m);   
}
