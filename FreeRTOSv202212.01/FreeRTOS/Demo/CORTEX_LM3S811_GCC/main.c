#include "main.h"

int main(void)
{
	xPrintQueue = xQueueCreate(mainQUEUE_SIZE, sizeof(int));
	xSensorQueue = xQueueCreate(mainQUEUE_SIZE, sizeof(int));

	vSemaphoreCreateBinary(xNFilterSemaphore);
	xSemaphoreTake(xNFilterSemaphore, 1);
	xSemaphoreGive(xNFilterSemaphore);

	prvSetupHardware();

	xTaskCreate(vSensorTask, "Sensor", configMINIMAL_STACK_SIZE, NULL, MAX_TASK_PRIORITY - 3, NULL);
	xTaskCreate(vFilterTask, "Filter", configMINIMAL_STACK_SIZE, NULL, MAX_TASK_PRIORITY - 2, NULL);
	xTaskCreate(vPrintDisplayTask, "Display", configMINIMAL_STACK_SIZE, NULL, MAX_TASK_PRIORITY, NULL);
	xTaskCreate(vStatusCPU, "Status", configMINIMAL_STACK_SIZE + 300, NULL, 1, NULL);
	xTaskCreate(vTaskHighCPU, "HighCPU", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(vTaskLowCPU, "LowCPU", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

	/* Start the scheduler. */
	vTaskStartScheduler();

	// Sólo llegará aquí si no hay suficiente heap para iniciar el programa.
	sendMessageUART("Error: No hay suficiente heap para iniciar el programa\n");

	return 0;
}

void vTaskHighCPU(void *pvParameters)
{
    while (1)
    {
        vTaskDelay(1);
    }
}

void vTaskLowCPU(void *pvParameters)
{
    while (1)
    {
        vTaskDelay(100);
    }
}


/**
 * Esta tarea se encarga de generar un número aleatorio (dato del sensor) y enviarlo a la cola a una frecuencia de 10Hz
 */
void vSensorTask(void *pvParameters)
{
	int iRandomNumber;

	for (;;)
	{
		// Genera un número aleatorio
		iRandomNumber = get_random_int();
		xQueueSend(xSensorQueue, &iRandomNumber, portMAX_DELAY);
		vTaskDelay(100);
	}
}

/**
 * Esta tarea se encarga de filtrar los datos del sensor y enviarlos a la cola de impresión
 * su filtrado es sacar el promedio de los últimos N datos, se envía a imprimir cada 10 datos
 */
void vFilterTask(void *pvParameters)
{
	int valuesSensor[NFilterMax];
	int newValue;
	int index = 0;
	uint8_t N = NFilterMax - 1;

	for (;;)
	{
		xQueueReceive(xSensorQueue, &newValue, portMAX_DELAY);

		xSemaphoreTake(xNFilterSemaphore, portMAX_DELAY);
		N = NFilter;
		xSemaphoreGive(xNFilterSemaphore);

		// desplaza el arreglo para agregar el nuevo dato
		for (int i = N - 1; i > 0; i--)
		{
			valuesSensor[i] = valuesSensor[i - 1];
		}

		// Guarda el nuevo dato en la posición 0
		valuesSensor[0] = newValue;

		index++;

		if (index == 10)
		{
			index = 0;

			int acc = 0;
			for (int i = 0; i < N; i++)
			{
				acc += valuesSensor[i];
			}
			int prom = acc / N;

			xQueueSend(xPrintQueue, &prom, portMAX_DELAY);
		}
	}
}

/**
 * Se grafica el valor del sensor en el Display, graficando una señal de temperatura según se van recibiendo los datos
 */
static void vPrintDisplayTask(void *pvParameters)
{	
	uint8_t currentValue = 0;
	uint8_t prevValue = 0;
	int uxLine = 0;
	int uxLinePrev = -1;
	int val;

	for (;;)
	{

		xQueueReceive(xPrintQueue, &val, portMAX_DELAY);

		// send val to uart
		// char valueResponce[20];
		// sprintf(valueResponce, "Valor: %d\n", val);
		// sendMessageUART(valueResponce);

		printToDisplay(val, prevValue, uxLine, uxLinePrev);

		prevValue = val;
		uxLine++;
		uxLinePrev++;
		if (uxLine > 95)
		{
			uxLine = 0;
		}
		if (uxLinePrev > 95)
		{
			uxLinePrev = 0;
		}
	}
}

/* This example demonstrates how a human readable table of run time stats
   information is generated from raw data provided by uxTaskGetSystemState().
   The human readable table is written to pcWriteBuffer. (see the vTaskList()
   API function which actually does just this). */
static void vStatusCPU(void *pvParameters)
{
	TaskStatus_t *pxTaskStatusArray;
	volatile UBaseType_t uxArraySize, x;

	unsigned long ulTotalRunTime, ulStatsAsPercentage;

	/* Take a snapshot of the number of tasks in case it changes while this
      function is executing. */
	uxArraySize = uxTaskGetNumberOfTasks();

	/* Allocate a TaskStatus_t structure for each task. An array could be
      allocated statically at compile time. */
	pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

	for (;;)
	{
		if (pxTaskStatusArray != NULL)
		{

			/* Generate raw status information about each task. */
			uxArraySize = uxTaskGetSystemState( pxTaskStatusArray,
										uxArraySize,
										&ulTotalRunTime );

			/* For percentage calculations. */
			ulTotalRunTime /= 100UL;

			// Evitar errores de división por cero.
			if (ulTotalRunTime > 0)
			{
				sendMessageUART("---------------------- Estados de las tareas ----------------------\n");
				// Para cada posición poblada en la matriz pxTaskStatusArray, formatee los datos sin procesar como datos ASCII legibles por humanos.
				for (x = 0; x < uxArraySize; x++)
				{
					// --------- Nombre de la tarea ---------
					sendMessageUART(pxTaskStatusArray[x].pcTaskName);

					// --------- Uso de la CPU ---------
					ulStatsAsPercentage =
						pxTaskStatusArray[x].ulRunTimeCounter / ulTotalRunTime;

					char message_cpu[1500];
					//

					if (ulStatsAsPercentage > 0UL)
					{
						ulStatsAsPercentage = 10;
						sprintf( message_cpu,  ": %lu",
										(unsigned long) ulStatsAsPercentage );
					}
					else
					{
						// Si el porcentaje es cero aquí, entonces la tarea ha consumido menos del 1% del tiempo total de ejecución.
						sprintf(message_cpu, "\tCPU usage: <1%%");
					}
					sendMessageUART(message_cpu);

					// --------- Stack libre ---------
					sendMessageUART("\tFree Stack: ");
					char message_stack[15];
					sprintf(message_stack, "%u", pxTaskStatusArray[x].usStackHighWaterMark);
					sendMessageUART(message_stack);

					// --------- Estado de la tarea ---------
					switch (pxTaskStatusArray[x].eCurrentState)
					{
					case eRunning:
						sendMessageUART("\tStatus: Running");
						break;
					case eReady:
						sendMessageUART("\tStatus: Ready");
						break;
					case eBlocked:
						sendMessageUART("\tStatus: Blocked");
						break;
					case eSuspended:
						sendMessageUART("\tStatus: Suspended");
						break;
					case eDeleted:
						sendMessageUART("\tStatus: Deleted");
						break;
					}

					// --------- Prioridad de la tarea ---------
					sendMessageUART("\tPriority: ");
					char message_priority[15];
					sprintf(message_priority, "%u", pxTaskStatusArray[x].uxCurrentPriority);
					sendMessageUART(message_priority);
					sendMessageUART("\n");
				}
			}
			// Libera memoria
			vPortFree(pxTaskStatusArray);
		}
		sendMessageUART("\n");
		vTaskDelay(1000);
	}
}

/**
 * Configura el hardware del microcontrolador y el periférico UART
 * Inicializa el Display y envía un mensaje por UART
 */
static void prvSetupHardware(void)
{
	/* Setup the PLL. */
	SysCtlClockSet(SYSCTL_SYSDIV_10 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_6MHZ);

	// Habilita el UART
	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

	// Configura el UART para operación 8-N-1, establece la velocidad de transmisión y deshabilita el FIFO.
	UARTConfigSet(UART0_BASE, mainBAUD_RATE, UART_CONFIG_WLEN_8 | UART_CONFIG_PAR_NONE | UART_CONFIG_STOP_ONE);

	// Habilita las interrupciones de transmisión en el UART.
	HWREG(UART0_BASE + UART_O_IM) |= UART_INT_RX;
	IntPrioritySet(INT_UART0, configKERNEL_INTERRUPT_PRIORITY);
	IntEnable(INT_UART0);

	// Inicializa el Display
	OSRAMInit(false);
	sendMessageUART("Ingrese un valor entre 0 y 9 para modificar el parámetro N del filtro pasa bajo\n");
}

/**
 * Recibe un valor por RX UART el cual se utiliza 
 * para modificar el valor de NFilter, 
 * el cual es el tamaño del filtro pasa bajo
 * El valor de NFilter debe estar entre 0 y 9
 */
void vUART_ISR(void)
{
	char valueRx;
	unsigned long ulStatus;
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE; 
	ulStatus = UARTIntStatus(UART0_BASE, true);

	UARTIntClear(UART0_BASE, UART_INT_TX);

	if (ulStatus & UART_INT_RX)
	{
		valueRx = UARTCharGet(UART0_BASE);
		// transformo el char en uint8_t
		uint8_t value = valueRx - '0'; // '0' = 48, '9' = 57
		if (value <= 0 || value > NFilterMax)
		{
			// Envio por UART un mensaje de error
			sendMessageUART("Error: Valor fuera de rango\n");
		}
		else
		{
			char valueChar;
			// Envio por UART el valor ingresado
			sendMessageUART("Valor ingresado: ");
			char valueResponce[10];
			sprintf(valueResponce, "%d", value);
			sendMessageUART(valueResponce);
			sendMessageUART("\n");

			xSemaphoreTakeFromISR(xNFilterSemaphore, &xHigherPriorityTaskWoken); // acquire the semaphore, pdFALSE will block the task if the semaphore is not available
			NFilter = value;
			xSemaphoreGiveFromISR(xNFilterSemaphore, &xHigherPriorityTaskWoken); // release the semaphore
		}
	}
}

void vGPIO_ISR(void)
{
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}

/**
 * Timer necesario para uxTaskGetSystemState
 */
void vTimer_ISR(void)
{
	TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
	timer++;
}

/**
 * Grafica en el display el valor actual y el anterior
 * El valor actual se grafica de forma negativa
 */
void printToDisplay(uint8_t currentValue, uint8_t prevValue, int uxLine, int uxLinePrev)
{
	const uint8_t values[] = {0b10000000, 0b01000000, 0b00100000, 0b00010000, 0b00001000, 0b00000100, 0b00000010, 0b00000001};
	const uint8_t negedValues[] = {0b01111111, 0b10111111, 0b11011111, 0b11101111, 0b11110111, 0b11111011, 0b11111101, 0b11111110};
	const uint8_t empty = 0;
	const uint8_t full = 0xFF;

	// Grafica el valor actual
	if (currentValue < 8)
	{
		OSRAMImageDraw(&negedValues[currentValue], uxLine, 1, 1, 1);
		OSRAMImageDraw(&full, uxLine, 0, 1, 1);
	}
	else
	{
		OSRAMImageDraw(&negedValues[currentValue - 8], uxLine, 0, 1, 1);
		OSRAMImageDraw(&full, uxLine, 1, 1, 1);
	}

	// Grafica el valor anterior
	if (prevValue < 8 && uxLinePrev >= 0)
	{
		OSRAMImageDraw(&values[prevValue], uxLinePrev, 1, 1, 1);
		OSRAMImageDraw(&empty, uxLinePrev, 0, 1, 1);
	}
	else if (uxLinePrev >= 0)
	{
		OSRAMImageDraw(&values[prevValue - 8], uxLinePrev, 0, 1, 1);
		OSRAMImageDraw(&empty, uxLinePrev, 1, 1, 1);
	}
}

/**
 * Funcion que genera un numero pseudoaleatorio entre 0 y 95
 * Devuelve un número entre 0 y 95 (16(columnas del display)*6) = 96 posibles valores
 */
int get_random_int()
{
	long int hi = seed / q;
	long int lo = seed % q;
	long int test = a * lo - r * hi;

	if (test > 0)
	{
		seed = test;
	}
	else
	{
		seed = test + m;
	}

	return seed % 16;
}

/**
 * Envia un mensaje por UART
 */
void sendMessageUART(const char *message)
{
	while (*message)
	{
		UARTCharPut(UART0_BASE, *message);
		message++;
	}
}

/**
 * Esta función se llama cuando se produce un desbordamiento de pila en una tarea
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
	sendMessageUART("Error: Stack Overflow in \n");
	sendMessageUART(pcTaskName);
	sendMessageUART("\n");
	for (;;)
	{
	}
}

/**
 * Timer necesario para uxTaskGetSystemState
 */
void confTimer0()
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	IntMasterEnable();
	TimerConfigure(TIMER0_BASE, TIMER_CFG_32_BIT_TIMER);
	TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet() / 1000 - 1);
	TimerIntRegister(TIMER0_BASE, TIMER_A, vTimer_ISR);
	TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
	TimerEnable(TIMER0_BASE, TIMER_A);
}

/**
 * Timer necesario para uxTaskGetSystemState
 */
unsigned long get_time()
{
	return timer;
}
