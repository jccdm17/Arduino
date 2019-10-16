    @echo off

    set varname=%1
    set varnameip=%2

    echo Upload Iniciado!!!

    echo %varname% %varnameip% !!

    tftp -i %varnameip% put ..\Prontos_Para_Upload\%varname%.bin

    echo Upload Finalizado!!!

    pause