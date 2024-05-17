Driver de Kernel para Raspberry Pi - Control de LEDs, Speaker y Buttons
Este módulo de kernel controla 3 dispositivos, LEDs, Speaker y Buttons

Funcionalidades Principales
LEDs
    Escritura:
        El módulo permite encender y apagar hasta 6 LEDs conectados a los pines GPIO de la Raspberry Pi. 
        El estado de los LEDs pueden ser leídos con el comando cat o sobrescritos con el comando echo y un byte de entrada
        donde los dos bit más significativos actúan de diferentes modos.
            -00: Los bits serán los valores que deben tomar los LEDS (1 para encender. 0 para apagar).
                    LEDs: xxxxxx Entrada: 010101 Salida: LEDs = 010101
            -01: Los bits que tienen valor 1 se encenderán y el resto quedará en el estado anterior
                    LEDS: 010101 Entrada: 110011 Salida: LEDs = 110111
            -10: Los bits que tienen valor 1 se apagarán y el resto se quedará en el estado anterior
                    LEDs: 010101 Entrada: 110011 Salida: LEDs = 000100
            -11: Los bits que tienen valor 1 cambiará el estado actual del led invirtiéndolo y el resto se quedará en el estado anterior.
                    LEDs: 010101 Entrada: 110011 Salida: LEDs = 100110
    lectura:
        Muestra el estado actual de los leds usando el comando cat
Altavoz
Permite activar y desactivar un altavoz conectado a la Raspberry Pi. 
Esto se realiza mediante operaciones de escritura al dispositivo de altavoz.
    Compile y use el archivo ./prueba_speaker2.c y ./sonidoTetris.c para comprobar el funcionamiento del dispositivo Speaker.
    
        gcc -C prueba_speaker2.c -o prueba_speaker2
        gcc -C sonidoTetris.c -o sonidoTetris
    
    Y ejecutar los archivos:
    
        ./prueba_speaker2
        ./sonidoTetris

Botones
El módulo maneja interrupciones de hasta dos botones conectados a la Raspberry Pi. 
Los botones son gestionados mediante el uso de interrupciones, y se implementa un mecanismo de debounce para evitar problemas de rebote.
    Compile y use el archivo ./lee_buttons.c para comprobar el funcionamiento del dispositivo Buttons
    
        gcc -C lee_buttons.c -o lee_buttons
    
    Y ejecutar los archivos

        ./lee_buttons

Instalación
Compila el módulo e instala el driver como módulo de kernel:
    make compile
    make install
Desinstalación y descarga del driver como módulo de kernel:
    make uninstall
    make clean

Autoría y Licencia
Este módulo de kernel fue desarrollado por 
    Grupo J. Augusto Jiménez Jiménez y Rogelio Montosa Callejón
como parte de la asignatura de Diseño de Sistemas Operativos. 

Está licenciado bajo GPL.
