
    @echo off

    set /p varname=qual o Nome do Arquivo hex?(NomeDoArquivo.ino Sem a Extensao .hex) 
    
    echo Nome Arquivo: %varname%
	
    set /p varnameipPlaca=qual o IP para Reiniciar a Placa?(Exemplo:192.168.1.209)

    set /p varnameipBoot=qual o IP para Upload Bootloader?(Exemplo:192.168.1.209)
    
    copy ..\Compilados\%varname%.hex %varname%.hex

    avr-objcopy -I ihex %varname%.hex -O binary %varname%.bin 

    move %varname%.bin ..\Prontos_Para_Upload\%varname%.bin

    echo Arquivo %varname%.bin criado!!!

    pause
    
    :Upload

    http-ping -n 1 -w 1 -s  %varnameipPlaca%/reset
    
    echo Placa Reiniciada(%varnameipPlaca%)!!!

    echo Upload Iniciado(%varnameipBoot%)!!!

    tftp -i %varnameipBoot% put ..\Prontos_Para_Upload\%varname%.bin

    echo Upload Finalizado!!!
   
    set /p varnameRefaz=Enviar Novamente?(S/N)

    IF "%varnameRefaz%"=="S" (
       GOTO Upload 
    )  

    pause
