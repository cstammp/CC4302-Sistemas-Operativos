#define _XOPEN_SOURCE 500
#include "nthread-impl.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <nthread.h>

#include "disk.h"
#include "pss.h"

// Defina aca tipos y variables globales
int busy = 0;
int U = 0;
PriQueue *firstQueue;
PriQueue *secondQueue;

void f(nThread thisTh) {
    if (thisTh->ptr != NULL) {
        nth_cancelThread(thisTh);
        
        // Eliminar de ambas colas
        priDel(firstQueue, thisTh);
        priDel(secondQueue, thisTh);
        
        // Dejar el campo ptr en NULL para indicar que expiró el timeout
        thisTh->ptr = NULL;
    }
}

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
int nRequestDisk(int track, int timeout) {
    START_CRITICAL;
    nThread thisTh= nSelf();
    thisTh->ptr = &track;
    if (busy == 0) {
        busy = 1;
        U = track;
        thisTh->ptr = NULL;
        END_CRITICAL;
        return 0;
    } else {
        if (track>=U) {
            priPut(firstQueue, thisTh, track);
        } else {
            priPut(secondQueue, thisTh, track);
        }
        
        if (timeout > 0) {
            // Si hay un timeout positivo, programar un temporizador
            suspend(WAIT_SEND_TIMEOUT);
            nth_programTimer(timeout * 1000000LL, f);
        } else {
            // Si timeout es negativo, esperar indefinidamente a que llegue un mensaje
            suspend(WAIT_SEND);
        }
        schedule();  
    }
    
    int result = (thisTh->ptr == NULL) ? 1 : 0;   
    
    END_CRITICAL;
    return result;
}

// Notificación de término de uso del disco
void nReleaseDisk() {
    START_CRITICAL;
    if (!emptyPriQueue(firstQueue)) {
        U = priBest(firstQueue);
        nThread thisTh = priGet(firstQueue);
        if (thisTh->ptr != NULL) nth_cancelThread(thisTh);
        setReady(thisTh);
        schedule();
    } else if (!emptyPriQueue(secondQueue)) {
        PriQueue *temp = firstQueue;
        firstQueue = secondQueue;
        secondQueue = temp;
        
        U = priBest(firstQueue);
        nThread thisTh = priGet(firstQueue);
        if (thisTh->ptr != NULL) nth_cancelThread(thisTh);
        setReady(thisTh);
        schedule();
    } else {
        busy = 0;
    }
    END_CRITICAL;
}
