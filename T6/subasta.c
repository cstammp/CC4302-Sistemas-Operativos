#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "spinlocks.h"
#include "pss.h"
#include "subasta.h"

struct subasta {
    PriQueue *q;        // Cola de prioridad que guarda las ofertas
    int unidadesMax;    // Unidades a subastar
    int ofertas;        // Número de ofertas recibidas
    int status;         // Estado de la subasta (0: abierta, 1: cerrada)
};

int slm = OPEN;

// Inicializa el sistema para subastar n unidades idénticas.
Subasta nuevaSubasta(int unidades) {
    Subasta auction = malloc(sizeof(*auction));
    auction->q = makePriQueue();
    auction->unidadesMax = unidades;
    auction->ofertas = 0;
    auction->status = 0;
    return auction;
}

// Finaliza y libera los recursos de la subasta
void destruirSubasta(Subasta s) {
    destroyPriQueue(s->q);
    free(s);
}

// múltiples tareas invocan esta función para hacer una oferta por comprar una unidad del producto al valor precio.
// Esta función espera hasta que (i) la subasta se cierre, retornando TRUE, o (ii) n otras tareas ofrecieron un precio mayor por el producto, y en tal caso se retorna FALSE
int ofrecer(Subasta s, double precio) {
    spinLock(&slm);
    
    if (s->status == 1) {
        spinUnlock(&slm);
        return 0;
    }
    
    double minPrecio = priBest(s->q);
    if (s->ofertas >= s->unidadesMax){
        if (precio > minPrecio){
            s->ofertas--;
            int* w = priGet(s->q);
            spinUnlock(w);
        } else {
            spinUnlock(&slm);
            return 0;
        }
    }
    int mysl = CLOSED;
    priPut(s->q, &mysl, precio);
    s->ofertas++;
    spinUnlock(&slm);
    spinLock(&mysl);
    if (s->status == 1) return 1;
    return 0;
}

// Cierra la subasta retornando el monto total recaudado y entregando en *prestantes la cantidad de unidades sobrantes de la subasta, es decir las que no se
// ... vendieron porque llegaron menos que n oferentes.
double adjudicar(Subasta s, int *punidades) {
    spinLock(&slm);
    s->status = 1;
    *punidades = s->unidadesMax - s->ofertas;
    if (*punidades < 0) *punidades = 0;
    double totalRecaudado = 0;
    while(!emptyPriQueue(s->q)){
        totalRecaudado += priBest(s->q); 
        int* w = priGet(s->q);
        spinUnlock(w);
    }
    spinUnlock(&slm);
    return  totalRecaudado;
}
