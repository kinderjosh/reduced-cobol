       IDENTIFICATION DIVISION.
       PROGRAM-ID. LENGTHOF-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * String to get the length of.
       01 WS-STRING PIC X(32) VALUE "Hello, World!".
       PROCEDURE DIVISION.
      * LENGTH OF measures the length of a string.
           DISPLAY "'" WS-STRING "' is " LENGTH OF WS-STRING
               " characters long".
           STOP RUN.