/*
Para correr la Tarea ejecutar:

make : Compila el código del módulo y genera el archivo binario cena-filosofos.ko.

sudo insmod cena-filosofos.ko : Carga el módulo compilado en el kernel del sistema operativo.

lsmod | grep cena_filosofos : Verifica que el módulo cena_filosofos esté cargado en el kernel.

sudo dmesg | tail : Muestra los últimos mensajes del sistema, incluyendo los relacionados con la inicialización del módulo.

sudo mknod /dev/cena-filosofos c 61 0 : Crea el archivo de dispositivo especial /dev/cena-filosofos con el número major 61 y minor 0.

sudo chmod 666 /dev/cena-filosofos : Cambia los permisos del archivo de dispositivo para que todos los usuarios puedan leer y escribir.

ls -l /dev/cena-filosofos : Lista el archivo de dispositivo para confirmar su creación, permisos y configuración.
*/

/* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h>   /* kmalloc() */
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>  /* O_ACCMODE */
#include <linux/uaccess.h> /* copy_from/to_user */

#include "kmutex.h"

MODULE_LICENSE("Dual BSD/GPL");

/* Declaración de funciones */
int filosofos_open(struct inode *inode, struct file *filp);
int filosofos_release(struct inode *inode, struct file *filp);
ssize_t filosofos_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t filosofos_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
void filosofos_exit(void);
int filosofos_init(void);

/* Estructura funciones de archivo */
struct file_operations filosofos_fops = {
    .read = filosofos_read,
    .write = filosofos_write,
    .open = filosofos_open,
    .release = filosofos_release
};

/* Declaración de las funciones init y exit */
module_init(filosofos_init);
module_exit(filosofos_exit);

/* -------- Driver cena-filosofos -------- */

/* Variables globales del driver */
int filosofos_major = 61;
#define MAX_SIZE 8192
#define NUM_FIL 5

static char *filosofos_buffer;
static ssize_t curr_size;

typedef enum estado { pensando, esperando, comiendo } estado;
typedef struct {
    estado estado_fil;
    int orden_llegada; // orden de prioridad
} filosofo_t;

static filosofo_t filosofos[NUM_FIL];
static int orden_global = 0;

static KMutex mutex;
static KCondition cond_fil[NUM_FIL];

/* funciones auxiliares */

static int puede_comer(int i) {
    int izq = (i + NUM_FIL - 1) % NUM_FIL;
    int der = (i + 1) % NUM_FIL;

    if (filosofos[izq].estado_fil == comiendo || 
        filosofos[der].estado_fil == comiendo) {
        return 0;
    }

    if (filosofos[izq].estado_fil == esperando && 
        filosofos[izq].orden_llegada < filosofos[i].orden_llegada) {
        return 0;
    }

    if (filosofos[der].estado_fil == esperando && 
        filosofos[der].orden_llegada < filosofos[i].orden_llegada) {
        return 0;
    }

    return 1;
}

int strcomp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && *s2) {
        if (*s1 != *s2) {
            return 0;
        }
        s1++;
        s2++;
        n--;
    }
    return n == 0 || (*s1 == '\0' && *s2 == '\0');
}

/* Driver functions */
int filosofos_init(void) {
    int rc;

    rc = register_chrdev(filosofos_major, "filosofos", &filosofos_fops);
    if (rc < 0) {
        printk("<1>cena-filosofos: cannot obtain major number %d\n", filosofos_major);
        return rc;
    }

    curr_size = 0;
    m_init(&mutex);

    for (int i = 0; i < NUM_FIL; ++i) {
        c_init(&cond_fil[i]);
        filosofos[i].estado_fil = pensando;
    }

    filosofos_buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
    if (filosofos_buffer == NULL) {
        filosofos_exit();
        return -ENOMEM;
    }

    printk("<1>Inserting cena-filosofos module\n");
    return 0;
}

void filosofos_exit(void) {
    unregister_chrdev(filosofos_major, "filosofos");
    if (filosofos_buffer) {
        kfree(filosofos_buffer);
    }
    printk("<1>Removing cena-filosofos module\n");
}

int filosofos_open(struct inode *inode, struct file *filp) {
    return 0;
}

int filosofos_release(struct inode *inode, struct file *filp) {
    return 0;
}

ssize_t filosofos_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
    ssize_t rc;
    m_lock(&mutex);

    static char buffer[128];
    static size_t data_size = 0;

    if (*f_pos == 0) {
        data_size = 0;

        for (int i = 0; i < NUM_FIL; i++) {
            char fil = i + '0';

            if (data_size < sizeof(buffer)) {
                buffer[data_size++] = fil;
            }

            if (data_size < sizeof(buffer)) {
                buffer[data_size++] = ' ';
            }

            char *estado = (filosofos[i].estado_fil == pensando) ? "pensando" :
                           (filosofos[i].estado_fil == esperando) ? "esperando" :
                           "comiendo";

            for (int j = 0; estado[j] != '\0' && data_size < sizeof(buffer); j++) {
                buffer[data_size++] = estado[j];
            }

            if (data_size < sizeof(buffer)) {
                buffer[data_size++] = '\n';
            }
        }
    }

    if (*f_pos >= data_size) {
        rc = 0;
        goto epilog;
    }

    if (count > data_size - *f_pos) {
        count = data_size - *f_pos;
    }

    if (copy_to_user(buf, buffer + *f_pos, count)) {
        rc = -EFAULT;
        goto epilog;
    }

    *f_pos += count;
    rc = count;

epilog:
    if (rc == 0) *f_pos = 0;
    m_unlock(&mutex);
    return rc;
}

ssize_t filosofos_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) {
    ssize_t rc;
    char command[16];
    int filosofo;

    m_lock(&mutex);

    if (count >= sizeof(command)) {
        rc = -EINVAL;
        goto epilog;
    }

    if (copy_from_user(command, buf, count)) {
        rc = -EFAULT;
        goto epilog;
    }
    command[count] = '\0';

    if (strcomp(command, "comer ", 6) == 1) {
        filosofo = command[6] - '0';

        if (filosofo < 0 || filosofo >= NUM_FIL) {
            rc = -EINVAL;
            goto epilog;
        }

        filosofos[filosofo].estado_fil = esperando;
        filosofos[filosofo].orden_llegada = orden_global++;

        while (!puede_comer(filosofo)) {
            c_wait(&cond_fil[filosofo], &mutex);
        }

        filosofos[filosofo].estado_fil = comiendo;
        printk(KERN_INFO "Filósofo %d está comiendo\n", filosofo);

    } else if (strcomp(command, "pensar ", 7) == 1) {
        filosofo = command[7] - '0';

        if (filosofo < 0 || filosofo >= NUM_FIL) {
            rc = -EINVAL;
            goto epilog;
        }

        filosofos[filosofo].estado_fil = pensando;
        printk(KERN_INFO "Filósofo %d está pensando\n", filosofo);

        int vecino_izq = (filosofo + NUM_FIL - 1) % NUM_FIL;
        int vecino_der = (filosofo + 1) % NUM_FIL;

        if (puede_comer(vecino_izq)) {
            filosofos[vecino_izq].estado_fil = comiendo;
            c_signal(&cond_fil[vecino_izq]);
            printk(KERN_INFO "Filósofo %d ahora está comiendo\n", vecino_izq);
        }

        if (puede_comer(vecino_der)) {
            filosofos[vecino_der].estado_fil = comiendo;
            c_signal(&cond_fil[vecino_der]);
            printk(KERN_INFO "Filósofo %d ahora está comiendo\n", vecino_der);
        }
    } else {
        printk(KERN_ERR "Comando desconocido: '%s'\n", command);
        rc = -EINVAL;
        goto epilog;
    }

    rc = count;

epilog:
    m_unlock(&mutex);
    return rc;
}

