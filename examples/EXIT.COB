       IDENTIFICATION DIVISION.
       PROGRAM-ID. EXIT-EXAMPLE.
       PROCEDURE DIVISION.
           PERFORM SOME-PROC.

           DISPLAY "This will not be executed".
           STOP RUN.

       SOME-PROC.
      * EXIT PROGRAM will stop the program even when outside of the
      * main procedure.
           EXIT PROGRAM.

       END PROGRAM EXIT-EXAMPLE.