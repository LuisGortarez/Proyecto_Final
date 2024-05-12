Nuestro proyecto se centra en el desarrollo de un sistema de seguridad diseñado para monitorear diversos entornos, como hogares u oficinas, utilizando una combinación de sensores y una cámara accesible a través de una aplicación móvil.

El código genera tres procesos hijo para el manejo del hardware externo empleado en la elaboración de este proyecto.

- Proceso hijo 1 - sensor de proximidad y alarma: Constantemente mide la distancia en cm a la que se encuentra un objeto. Si la distancia es menor a 15cm y si la alarma está activada, el buzzer sonará hasta que se apague desde la aplicación.

- Proceso hijo 2 - sensor de movimiento y servomotor: La cámara se encuentra sobre el servomotor, es alimentada por la señal enviada del sensor de movimiento, es decir, solo cuando se detecte movimiento se encenderá la cámara. Esto también encenderá un PWM para controlar al motor y mover la cámara aumentando así su rango de visión.

- Proceso hijo 3 - lectura de hora por conexión a internet: Cada 5 segundos se leerá la hora a través del internet para activar o desactivar la alarma. Desde la aplicación se establece el rango de horario en el que la alarma estará activada. 

- Proceso padre - recibimiento de comandos por UART: Constantemente espera información transmitida. Al recibir un comando de la aplicación, el padre realizará una de cuatro acciones; establecimiento de hora de encendido de alarma, establecimiento de hora de apagado de alarma, activación/desactivación de alarma o terminación de programa. Al terminar el programa, el padre elevará una bandera para indicar a los hijos el fin de su ejecución y el padre esperará hasta que todos sus hijos hayan terminado.