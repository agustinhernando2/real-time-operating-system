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

// Definicion de constantes para el generador de numeros aleatorios Montecarlo
#define a 16807             // Multiplicador del generador de congruencia multiplicativa
#define m 2147483647        // Modulo (numero primo 2^31 - 1)
#define q (m / a)           // Cociente de m / a, optimiza el calculo reduciendo divisiones
#define r (m % a)           // Residuo de m / a, usado en la optimizacion

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
static void vTaskHighCPU(void *pvParameters);
static void vTaskStackUsage(void *pvParameters);
int fibonacci(int n);

static void prvSetupHardware(void);
void printToDisplay(uint8_t currentValue, uint8_t prevValue, int uxLine, int uxLinePrev);
int get_random_int();
void sendMessageUART(const char *message);

void vTimer_ISR(void);
void confTimer0();
unsigned long get_time();

// String that is transmitted on the UART.
QueueHandle_t xPrintQueue;
QueueHandle_t xSensorQueue;

// semaphore
SemaphoreHandle_t xNFilterSemaphore;

uint8_t NFilter = 9;
static long int seed = 1; // Semilla global para el generador de numeros aleatorios
unsigned long timer = 0;