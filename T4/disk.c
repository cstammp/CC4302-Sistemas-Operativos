#define _XOPEN_SOURCE 500
#include "nthread-impl.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <nthread.h>

#include "disk.h"
#include "pss.h"

typedef struct {
    nThread nThr;
    int track;
} Request;

// Defina aca tipos y variables globales
int busy = 0;
int U = 0;
PriQueue *firstQueue;
PriQueue *secondQueue;

// Función de inicialización
void iniDisk(void) {
    firstQueue = makePriQueue();
    secondQueue = makePriQueue();
}

// Función de limpieza
void cleanDisk(void) {
    destroyPriQueue(firstQueue);
    destroyPriQueue(secondQueue);
}


// Solicita acceso al disco indicando la pista
void nRequestDisk(int track, int delay) {
    START_CRITICAL;
    if (busy == 0) {
        busy = 1;
        U = track;
    } else {
        nThread thisTh= nSelf();
        suspend(WAIT_SEM);
        Request req = {thisTh, track};
        if (track>=U) {
            priPut(firstQueue, &req, track);
        } else {
            priPut(secondQueue, &req, track);
        }
        schedule();
    }
    END_CRITICAL;
}

// Notificación de término de uso del disco
void nReleaseDisk() {
    START_CRITICAL;
    if (!emptyPriQueue(firstQueue)) {
        Request *req = priGet(firstQueue);
        U = req->track;
        setReady(req->nThr);
        schedule();
    } else if (!emptyPriQueue(secondQueue)) {
        PriQueue *temp = firstQueue;
        firstQueue = secondQueue;
        secondQueue = temp;
        
        Request *req = priGet(firstQueue);
        U = req->track;
        setReady(req->nThr);
        schedule();
    } else {
        busy = 0;
    }
    END_CRITICAL;
}
