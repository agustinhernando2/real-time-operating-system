#include "DriverLib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Baud rate para UART
#define mainBAUD_RATE (19200)
#define configKERNEL_INTERRUPT_PRIORITY 255
// unsigned int _Min_Heap_Size = 0x200; /* Tamaño del heap en bytes */

// Variables para el generador de números aleatorios Montecarlo
#define a 16807
#define m 2147483647
#define q (m / a)
#define r (m % a)

// Valor maximo de NFilter
#define NFilterMax 10

// Prioridad de las tareas
#define MAX_TASK_PRIORITY (tskIDLE_PRIORITY + 5)

// Tamaño de la cola de mensajes
#define mainQUEUE_SIZE (3)

static void vPrintDisplayTask(void *pvParameter);
static void vSensorTask(void *pvParameters);
static void vFilterTask(void *pvParameters);
static void vStatusCPU(void *pvParameters);
static void vGenerateOverflow(void *pvParameters);

static void prvSetupHardware(void);
void printToDisplay(uint8_t currentValue, uint8_t prevValue, int uxLine, int uxLinePrev);
int get_random_int();
void sendMessageUART(const char *message);

void vTimer_ISR(void);
void confTimer0();
unsigned long get_time();

/* String that is transmitted on the UART. */
QueueHandle_t xPrintQueue;
QueueHandle_t xSensorQueue;

//semaforo
SemaphoreHandle_t xNFilterSemaphore;

uint8_t NFilter = 9;
static long int seed = 1;
unsigned long timer = 0;