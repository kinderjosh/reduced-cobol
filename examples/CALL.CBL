       IDENTIFICATION DIVISION.
       PROGRAM-ID. CALL-EXAMPLE.
       PROCEDURE DIVISION.
      * Call C functions directly, as this compiler generates C99 code.
           CALL "printf" USING "Hello, World!\n".
           STOP RUN.