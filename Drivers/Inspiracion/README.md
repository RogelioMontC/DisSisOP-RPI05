
# Driver de control de los leds, el speaker y los botones de la placa auxiliar Raspberry PI
##### Francisco Javier Cano Moreno, 3A Ingeniería de Computadores.

## LEDS
El archivo *leds* permite encender y apagar los leds de la placa auxiliar. Para ello, se debe escribir en el archivo un carácter, que representa un byte, donde los dos bits más significativos describen el modo de encendido de los leds, y el resto el estado de los leds. Después, con el comando *cat* se puede leer el estado de los leds.

### Modos de encendido
- *00*: Los leds se encienden y apagan con el valor de cada bit.
- *01*: Solamente se encienden los leds que tienen valor 1 y el resto se queda como está.
- *10*: Solamente se apagan los leds que tienen valor 1 y el resto se queda como está.
- *11*: Se encienden los leds de forma inversa, es decir, los leds que tienen valor 0 se encienden y los que tienen valor 1 se apagan.

### Implementación
- En primer lugar, se ha necesitado de una función que recibe un byte por parámetro y escribe en los leds el valor correspondiente según el modo. Dicha función es *byte2leds*.
- El driver debe ser de escritura y de lectura por lo que se crean dos funciones, *leds_read* y *leds_write*, que se encargan de leer y escribir en el archivo *leds* respectivamente.
- La función *leds_read* lee el estado de los leds y lo escribe en el buffer de lectura mediante la función *copy_to_user*. Por otro lado, la función *leds_write* recibe el byte que se quiere escribir en los leds mediante *copy_from_user* y lo escribe mediante la función *byte2leds*.
- Para la configuración de los GPIOs, en *r_init*, solicitamos los GPIOs de los leds mediante *gpio_request* y los configuramos como salida mediante *gpio_direction_output*. Después, en *r_cleanup*, liberamos los GPIOs mediante *gpio_free*.

## SPEAKER
*Speaker* es un device de sólo escritura con el cual podemos generar sonidos en el speaker pasandole una cadena de caracteres, donde los caracteres iguales a '0' apagan el speaker y los que son diferentes a '0' lo encienden.

### Implementación
- La implementación de la función de escritura *speaker_write* es muy sencilla, ya que simplemente se enciende o apaga el speaker según el carácter que se recibe con *copy_from_user* hasta que se termina la cadena de caracteres.
- La configuración del GPIO del speaker se realiza de la misma manera que en el caso de los leds.

## BUTTONS
Por último, el device *buttons* es solamente de lectura y nos muestra los botones se han pulsado en la placa auxiliar. Cuando se hace la lectura y no hay ninguna pulsación, se debe bloquear hasta que se recibe una pulsación. También, si se está esperando una pulsación y se hace una nueva lectura, debe mostrar un mensaje de "Dispositivo ocupado".

### Implementación
- Creamos una función *buttons_read* que lee el estado de los botones y lo escribe en el buffer de lectura mediante la función *copy_to_user*. El buffer de lectura es un string que almacena hasta 100 caracteres y que se va rellenando cada vez que se pulsa un botón. Además para no tener que recorrer el array entero, se guarda la posición en la que se debe escribir el siguiente carácter en la variable *pulsaciones_counter*.
- Para detectar la pulsación del botón se utilizan dos irqs, una para cada botón. Estas irqs comparten el mismo manejador, donde se comprueba qué interrupción ha llegado y se planifica el tasklet correspondiente. Estos tasklets se encargan de escribir en el buffer de lectura el carácter correspondiente al botón pulsado.
- Para que la lectura se bloqueante, en *buttons_read* se comprueba si hay alguna pulsación en el buffer de lectura. Si no hay ninguna, se bloquea el proceso mediante *wait_event_interruptible* y se espera a que se produzca una interrupción. Éste se despierta con *wake_up_interruptible* cuando se planifica un tasklet y se escribe el primer carácter en el buffer de lectura.
- El read solo se puede producir una vez, por lo que si se hace una segunda lectura antes de que se haya terminado de leer el buffer, se muestra un mensaje de "Dispositivo Ocupado". Esto se consigue con una variable *ocupado* y mediante el uso de semáforos para controlar el acceso a dicha variable. Ésta variable se pone a 0 cuando se realiza una lectura y se pone a 1 cuando se termina de leer el buffer, por lo que si se hace una segunda lectura, se comprueba si la variable está a 0 y se muestra el mensaje de "Dispositivo Ocupado".
- Por último, para solucionar los rebotes, se han implementado dos timers, uno para cada botón, los cuales se encargan de ignorar la interrupción correspondiente durante un tiempo que se pasa con el parámetro *time_waiting*.
- Para la configuración de los botones, debemos hacer una petición de los GPIOs correspondientes con *gpio_request*, asignarles una irq con *gpio_to_irq* y asignarsela con *request_irq*. Después, se liberan estas irq con *free_irq* y los GPIOs con *gpio_free* en *r_cleanup*. Además hay que eliminar los tasklets y los timers con *del_timer* y *tasklet_kill*.

