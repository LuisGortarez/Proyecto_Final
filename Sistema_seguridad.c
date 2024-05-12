//////////////////////////////////////////////////////////////////////////
//////////  ITESO                                               //////////
//////////  Autores:    Diego Delgado,  ie730460@iteso.mx       //////////
//////////              Luis Gortàres,  luis.gortarez@iteso.mx  //////////
//////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include <signal.h>

#include <sys/mman.h>
#include <sys/wait.h>
#include <asm/termbits.h>
#include <string.h>

// Numero de hijos a crear
#define MAX_STRINGS			3U		// Nùmero de hijos a crear

// Para sensor de proximidad
#define PROXIMITY_SENSOR_PIN_TRIGGER	6U		// GPIO_28 en gpiochip2
#define PROXIMITY_SENSOR_PIN_ECHO		7U		// GPIO_30 en gpiochip2
#define ECHO_COUNT_LIMIT				15U		// Lìmite antes de sonar alarma

// Para sensor de movimiento
#define MOVEMENT_SENSOR_PIN		20U		// GPIO_44 en gpiochip4

// Para alarma
#define ALARM_PIN			0U		// GPIO_25 en gpiochip2
#define PIZANO_NUM			6666U

// Para PWM y servo
#define PWM_PIN_OUTPUT			8U		// GPIO_32 en gpiochip2
#define SERVO_START_ANGLE		750U
#define SERVO_MAX_ANGLE			1250U
#define SERVO_MIN_ANGLE			250U
#define SERVO_PERIOD			1500U
#define SERVO_POLLING_TIME		1000U

// Proceso UART
#define BUFFER_LEN 			9		// Tamaño de buffer de UART
#define ALARM_TOGGLE		97U		// Caracter a en ASCII
#define SET_ALARM			115U	// Caracter s en ASCII
#define END_PROGRAM			117U	// Caracter u en ASCII
#define SET_ALARM_OFF_NONE	50U		// Caracter 2 en ASCII
#define SET_ALARM_ON		49U		// Caracter 1 en ASCII
#define SET_ALARM_OFF		48U		// Caracter 0 en ASCII

void proximity_sensor();
void movement_sensor();
void leer_hora();
void UART();

// Variable para controlar si se ha presionado Ctrl+C
volatile sig_atomic_t ctrl_c_pressed = 0;

// Función para manejar la señal SIGINT (Ctrl+C)
void handle_ctrl_c(int sig)
{
    ctrl_c_pressed = 1;
}

int *finish;
int *alarm_flag;
short int *alarm_time_on;
short int *alarm_time_off;

int main(void)
{
	finish = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);    // Variable compartida entre hilos
	alarm_flag = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);    // Variable compartida entre hilos
	alarm_time_on = mmap(NULL, sizeof(short int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);    // Variable compartida entre hilos
	alarm_time_off = mmap(NULL, sizeof(short int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);    // Variable compartida entre hilos

	*finish = 0;
	*alarm_time_on = 0;
	*alarm_time_off = 0;
	*alarm_flag = 0;

	pid_t hilos[MAX_STRINGS];
	int i = 0;
	int retval;
	int pid;

	signal(SIGINT, handle_ctrl_c);

    for(i = 0; i < MAX_STRINGS; i++)
	{	// Crear los hijos
        hilos[i] = fork();

        if(-1 == hilos[i])
		{	// Error al crear el proceso hijo
            printf("No se logrò crear el hijo: %d\n", i);
            exit(EXIT_FAILURE);
        }
		else if(0 == hilos[i])
		{	 // Procesos hijo
            switch(i)
			{
                case 0:
                    while(!*finish)
					{	// Hijo 1 encargado de sensor de proximidad
                    	proximity_sensor();
                    }
					exit(0);
                    break;
                case 1:
                    while(!*finish)
					{	// Hijo 2 encargado de sensor de movimiento y servo
                        movement_sensor();
                    }
					exit(0);
                    break;
                case 2:
                    while(!*finish)
					{	// Hijo 3 encargado de hora
                        leer_hora();
                    }
					exit(0);
                    break;
				default:
					exit(0);
					break;
            }
            exit(EXIT_SUCCESS);
        }
    }
	
	UART();			// Padre encargado de UART
	*finish = 1;

	for(i = 0; i < MAX_STRINGS; i++)
	{	// Esperar terminaciòn de hilos
    	pid = wait(&retval);
    }

	return(0);
}

void proximity_sensor()
{
	static int echo_count = 0;

	static int fd = 0;
	static struct gpiohandle_request pin_output;	// Configurar la solicitud para el pin de escritura
	static struct gpiohandle_request pin_input;		// Configurar la solicitud para el pin de lectura
	static struct gpiohandle_data data;

	static int fd_alarm = 0;
	static struct gpiohandle_request pin_alarm;		// Configurar el pin de la alarma
	static struct gpiohandle_data data_alarm;

	// Configuracion de pines para sensor de movimiento
	fd_alarm = open("/dev/gpiochip2", O_RDWR);
	
	pin_alarm.lineoffsets[0] = ALARM_PIN;
    pin_alarm.flags = GPIOHANDLE_REQUEST_OUTPUT;
    pin_alarm.lines = 1;
    pin_alarm.default_values[0] = 0;
	
	ioctl(fd_alarm, GPIO_GET_LINEHANDLE_IOCTL, &pin_alarm);	// Obtener un descriptor de archivo para el pin

	// Configuracion de pines para alarma
	fd_alarm = open("/dev/gpiochip2", O_RDWR);
	
	pin_alarm.lineoffsets[0] = ALARM_PIN;
    pin_alarm.flags = GPIOHANDLE_REQUEST_OUTPUT;
    pin_alarm.lines = 1;
    pin_alarm.default_values[0] = 0;
	
	ioctl(fd_alarm, GPIO_GET_LINEHANDLE_IOCTL, &pin_alarm);	// Obtener un descriptor de archivo para el pin

	data_alarm.values[0] = 0;		// Desactivar pin alarma
    ioctl(pin_alarm.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_alarm);

	usleep(2000000);	// Esperar 2 segundos

	/* Configuracion de pines para sensor */
	fd = open("/dev/gpiochip2", O_RDWR);
    
    pin_output.lineoffsets[0] = PROXIMITY_SENSOR_PIN_TRIGGER;
    pin_output.flags = GPIOHANDLE_REQUEST_OUTPUT;
    pin_output.lines = 1;
    pin_output.default_values[0] = 0;

	pin_input.lineoffsets[0] = PROXIMITY_SENSOR_PIN_ECHO;
    pin_input.flags = GPIOHANDLE_REQUEST_INPUT;
    pin_input.lines = 1;
    pin_input.default_values[0] = 0;

    ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &pin_output);	// Obtener un descriptor de archivo para el pin
	ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &pin_input);

    /* Establecer el valor del pin output */
    data.values[0] = 1;		// Activar pin
    ioctl(pin_output.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);

	usleep(10000);	// Esperar 10 microsegundos

 	data.values[0] = 0;		// Desactivar pin
    ioctl(pin_output.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);

	/* Leer el valor del pin */
	ioctl(pin_input.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
	while(!data.values[0])
	{
		ioctl(pin_input.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
	}
	
	while(data.values[0])
	{
		ioctl(pin_input.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
		usleep(10);	// Esperar 10 microsegundos
		echo_count++;
	}

	printf("Echo: %d\n", echo_count);
	if((ECHO_COUNT_LIMIT >= echo_count) && (true == *alarm_flag))
	{
		while(true == *alarm_flag)
		{
			data_alarm.values[0] = ~data_alarm.values[0];
    		ioctl(pin_alarm.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_alarm);
			usleep(800000);
		}
		data_alarm.values[0] = 0;		// Desactivar pin
    	ioctl(pin_alarm.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_alarm);
	}

	echo_count = 0;
}

void movement_sensor()
{
	static bool increasing = true;
	static int read = 0;

	static int sleep_1 = SERVO_START_ANGLE;
	static int sleep_2 = SERVO_START_ANGLE;

	static int fd_movement = 0;
	static struct gpiohandle_request pin_movement;		// Configurar el pin de sensor de movimiento
	static struct gpiohandle_data data_movement;

	static int fd_pwm = 0;
	static struct gpiohandle_request pin_pwm;			// Configurar el pin de PWM
	static struct gpiohandle_data data_pwm;

	// Configuracion de pines para sensor de movimiento
	fd_movement = open("/dev/gpiochip4", O_RDWR);
	
	pin_movement.lineoffsets[0] = MOVEMENT_SENSOR_PIN;
    pin_movement.flags = GPIOHANDLE_REQUEST_INPUT;
    pin_movement.lines = 1;
    pin_movement.default_values[0] = 0;

	ioctl(fd_movement, GPIO_GET_LINEHANDLE_IOCTL, &pin_movement);	// Obtener un descriptor de archivo para el pin

	// Configuracion de pines para PWM
	fd_pwm = open("/dev/gpiochip2", O_RDWR);
    
    pin_pwm.lineoffsets[0] = PWM_PIN_OUTPUT;
    pin_pwm.flags = GPIOHANDLE_REQUEST_OUTPUT;
    pin_pwm.lines = 1;
    pin_pwm.default_values[0] = 0;

	ioctl(fd_pwm, GPIO_GET_LINEHANDLE_IOCTL, &pin_pwm);		// Obtener un descriptor de archivo para el pin

	if((SERVO_POLLING_TIME == read) || (!data_movement.values[0]))
	{
		read = 0;
		ioctl(pin_movement.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data_movement);	// Detectar movimiento
	}
	else
	{
		read++;
	}

	if(data_movement.values[0])
	{
		usleep(2000);

		if(increasing)
		{
			sleep_1 += 1;
			sleep_2 -= 1;
			if(SERVO_MAX_ANGLE <= sleep_1)
			{
				increasing = false;
			}
		}
		else
		{
			sleep_2 += 1;
			sleep_1 -= 1;
			if(SERVO_MIN_ANGLE >= sleep_1)
			{
				increasing = true;
			}
		}
	}
	else
	{	// Regresar al origen lentamente
		if(sleep_1 > sleep_2)
		{
			sleep_1 -= 1;
			sleep_2 += 1;
		}
		else if(sleep_1 < sleep_2)
		{
			sleep_1 += 1;
			sleep_2 -= 1;
		}
	}

	// Establecer el valor del pin output
    data_pwm.values[0] = 1;		// Activar pin
    ioctl(pin_pwm.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_pwm);

	usleep(sleep_1);	// Esperar 3 msegundos

 	data_pwm.values[0] = 0;		// Desactivar pin
    ioctl(pin_pwm.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_pwm);

	usleep(sleep_2);
}

void leer_hora()
{
	usleep(5000000);	// Verificar hora cada 5 segundos

	static short int current_time = 0;

	FILE *fp;
    static char output[128];  					// Tamaño suficiente para contener la salida del comando
    static int hora, minuto;  					// Enteros para almacenar la hora y el minuto

    fp = popen("date +%H:%M", "r");				// Escribir hora de internet en archivo
    if (fp == NULL)
	{	// Abrir el comando en modo de lectura ("r") y leer su salida
        printf("Error al ejecutar el comando\n");
    }

    if(!fgets(output, sizeof(output), fp))
	{	// Leer la salida del comando y almacenarla en la variable output
		printf("No se logrò obtener la hora\n");
	}

    pclose(fp);		// Cerrar el puntero de archivo

    sscanf(output, "%d:%d", &hora, &minuto);	// Separar la hora y el minuto de la cadena de salida

    hora -= 6;	// Restar 6 horas a la hora actual (hora leìda es de UTC)
    if (hora < 0)
	{	// Si la hora es negativa, agregamos 24 horas para obtener la hora correcta
        hora += 24;
    }

    printf("La hora actual es: %02d:%02d\n", hora, minuto);

	current_time = (hora * 100) + minuto;

	if(*alarm_time_on == current_time)
	{
		*alarm_flag = 1;
		printf("Alarma encendida\n");
	}
	else if(*alarm_time_off == current_time)
	{
		*alarm_flag = 0;
		printf("Alarma apagada\n");
	}
}

void UART()
{
	uint8_t rx_buffer[BUFFER_LEN];
	struct termios2  tty;

	int serial_port = open("/dev/ttymxc2", O_RDWR);

	// Baudrate personalizado
	ioctl(serial_port, TCGETS2, &tty);

	tty.c_cflag = 0;
	tty.c_cflag |= CS8; // 8 bits per byte (most common)
	tty.c_cflag |= CREAD;
	tty.c_lflag = 0;
	tty.c_iflag = 0;
	tty.c_oflag = 0;

	tty.c_cflag |= CBAUDEX;
	tty.c_ispeed = 9600;
	tty.c_ospeed = 9600;

	ioctl(serial_port, TCSETS2, &tty);

	while(1)
	{
		int res = read(serial_port, rx_buffer, BUFFER_LEN);	// bloqueante
		printf("%s", rx_buffer);

		if(ALARM_TOGGLE == rx_buffer[0])
		{
			if(SET_ALARM_ON == rx_buffer[1])
			{	// Activar alarma
				*alarm_flag = 1;
			}
			else
			{	// Desactivar alarma
				*alarm_flag = 0;
			}
		}
		else if(SET_ALARM == rx_buffer[0])
		{
			if(SET_ALARM_ON == rx_buffer[1])
			{	// Definir hora de activaciòn de alarma
				*alarm_time_on = 0;
				*alarm_time_on += (rx_buffer[2]-48)*1000;
				*alarm_time_on += (rx_buffer[3]-48)*100;
				*alarm_time_on += (rx_buffer[4]-48)*10;
				*alarm_time_on += (rx_buffer[5]-48);
			}
			else if(SET_ALARM_OFF == rx_buffer[1])
			{	// Definir hora de desactivaciòn de alarma
				*alarm_time_off = 0;
				*alarm_time_off += (rx_buffer[2]-48)*1000;
				*alarm_time_off += (rx_buffer[3]-48)*100;
				*alarm_time_off += (rx_buffer[4]-48)*10;
				*alarm_time_off += (rx_buffer[5]-48);
			}
			else
			{	// Definir hora de alarma inalcanzable
				*alarm_time_on = PIZANO_NUM;
			}
		}
		else if(END_PROGRAM == rx_buffer[0])
		{	// Terminar programa
			break;
		}
	}

	close(serial_port);
}