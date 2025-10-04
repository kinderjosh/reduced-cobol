       IDENTIFICATION DIVISION.
       PROGRAM-ID. ACCEPT-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * String to read into.
       01 WS-NAME PIC A(32).
       PROCEDURE DIVISION.
      * Ask the user for their name and read it from the command line.
           DISPLAY "What is your name? ".
           ACCEPT WS-NAME.

      * Say hello to the user.
           DISPLAY "Hello, " WS-NAME.

           STOP RUN.