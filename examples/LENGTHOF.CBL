       IDENTIFICATION DIVISION.
       PROGRAM-ID. LENGTHOF-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * String to get the length of.
       01 WS-STRING PIC X(32) VALUE "Hello, World!".
      * Number to get the size of.
       01 WS-NUM PIC S9(05) VALUE 12345.
       PROCEDURE DIVISION.
      * LENGTH OF gets the size of a piece of data.
      * In terms of a string, it measures the length.
           DISPLAY "'" WS-STRING "' is " LENGTH OF WS-STRING
               " characters long".

      * But in all other cases, it gets the size of the data.
      * WS-NUM has 5 digits, making it a 32 bit number, 4 bytes.
           DISPLAY WS-NUM " is a " LENGTH OF WS-NUM " byte number".
           STOP RUN.