cmd_/home/rogelio/Documentos/DisOp/DisSisOP-RPI05/CalculadorMedia/media.mod := printf '%s\n'   media.o | awk '!x[$$0]++ { print("/home/rogelio/Documentos/DisOp/DisSisOP-RPI05/CalculadorMedia/"$$0) }' > /home/rogelio/Documentos/DisOp/DisSisOP-RPI05/CalculadorMedia/media.mod