### Lab7 Sistemas Operativos II
## Ingeniería en Compuatación - FCEFyN - UNC
# Sistemas Operativos en Tiempo Real

## Introducción
Toda aplicación de ingeniería que posea requerimientos rigurosos de tiempo, y que este controlado por un sistema de computación, utiliza un Sistema Operativo de Tiempo Real (RTOS, por sus siglas en inglés). Una de las características principales de este tipo de SO, es su capacidad de poseer un kernel preemtive y un scheduler altamente configurable. Numerosas aplicaciones utilizan este tipo de sistemas tales como aviónica, radares, satélites, etc. lo que genera un gran interés del mercado por ingenieros especializados en esta área.

## Objetivo
El objetivo del presente trabajo practico es el de diseñar, crear, comprobar y validar una aplicación de tiempo real sobre un
RTOS.

## Desarrollo
Se realizará un aplicativo que sera ejecutado en el emulador qemu para la placa de desarrollo LM3S811. El aplicativo debe:

- Tener una tarea que simule un sensor de temperatura. Generando valores aleatorios, con una frecuencia de 10 Hz.
- Tener una tarea que  reciba los valores del sensor y aplique un filtro pasa bajos. Donde cada valor resultante es el promedio de las ultimas N mediciones.
- Una tarea que grafica en el display los valores de temperatura en el tiempo.
- Se debe poder recibir comandos por la interfaz UART para cambiar el N del filtro.
- Implementar una tarea que muestre periódicamente estadísticas de las tareas (uso de cpu, uso de memoria, etc), ademas de calcular el stack necesario para cada tarea.


## Referencias

https://github.com/FreeRTOS/FreeRTOS
TAG:
https://github.com/FreeRTOS/FreeRTOS/releases/tag/202212.01
