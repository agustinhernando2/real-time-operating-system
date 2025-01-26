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
	xTaskCreate(vStatusCPU, "Status", configMINIMAL_STACK_SIZE + 30, NULL, MAX_TASK_PRIORITY - 3, NULL);
	// xTaskCreate(vGenerateOverflow, "Overfl", 10, NULL, MAX_TASK_PRIORITY , NULL);

	/* Start the scheduler. */
	vTaskStartScheduler();

	// Sólo llegará aquí si no hay suficiente heap para iniciar el programa.
	sendMessageUART("Error: No hay suficiente heap para iniciar el programa\n");

	return 0;
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

			int prom = 0;
			for (int i = 0; i < N; i++)
			{
				prom += valuesSensor[i];
			}
			prom = prom / N;

			xQueueSend(xPrintQueue, &prom, portMAX_DELAY);
		}
	}
}

/**
 * Se grafica el valor del sensor en el Display, graficando una señal de temperatura según se van recibiendo los datos
 */
static void vPrintDisplayTask(void *pvParameters)
{
	int prevValue = 0;
	int uxLine = 0;
	int uxLinePrev = -1;
	int val;

	for (;;)
	{

		xQueueReceive(xPrintQueue, &val, portMAX_DELAY);

		uint8_t currentValue = val % 16; // 16 columnas del display

		printToDisplay(currentValue, prevValue, uxLine, uxLinePrev);

		prevValue = currentValue;
		uxLine++;
		uxLinePrev++;
		if (uxLine == 96)
		{
			uxLine = 0;
		}
		if (uxLinePrev == 96)
		{
			uxLinePrev = 0;
		}
	}
}

/**
 * Esta tarea se encarga de mostrar el estado de la CPU, debe mostrar el stack libre de cada tarea, el uso de la CPU de cada tarea
 * el estado de cada tarea (Ready, Running, Blocked, etc) y su prioridad
 */
static void vStatusCPU(void *pvParameters)
{
	TaskStatus_t *pxTaskStatusArray;
	volatile UBaseType_t uxArraySize, x;

	unsigned long ulTotalRunTime, ulStatsAsPercentage;

	// Total de tareas en el sistema.
	uxArraySize = uxTaskGetNumberOfTasks();

	// Asigne una estructura TaskStatus_t para cada tarea.
	pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

	for (;;)
	{
		if (pxTaskStatusArray != NULL)
		{

			// Genere información de estado sin procesar sobre cada tarea.
			uxArraySize = uxTaskGetSystemState(pxTaskStatusArray,
											   uxArraySize,
											   &ulTotalRunTime);

			// Para cálculos de porcentaje.
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

					char message_cpu[15];
					if (ulStatsAsPercentage > 0UL)
					{
						if (ulStatsAsPercentage >= 100UL)
						{
							sprintf(message_cpu, "\tCPU usage: >99%%");
						}
						else
						{
							sprintf(message_cpu, "\tCPU usage: >99%%");
						}
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
 * Se genera un overflow de pila
 */
static void vGenerateOverflow(void *pvParameters)
{
	int A[10000];
	long count = 1;
	for (;;)
	{
		for(int i = 0; i < count; i++)
		{
			A[i] = i;
		}
		count ++;
		vTaskDelay(1000);
	}
}

/**
 * Recibe un valor por RX UART
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
		uint8_t value = valueRx - '0';
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

			xSemaphoreTakeFromISR(xNFilterSemaphore, &xHigherPriorityTaskWoken);
			NFilter = value;
			xSemaphoreGiveFromISR(xNFilterSemaphore, &xHigherPriorityTaskWoken);
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
 * Genera un número aleatorio entre 0 y 96
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

	// Devuelve un número entre 0 y 96 (16(columnas del display)*6)
	return seed % 96;
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