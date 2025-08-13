       IDENTIFICATION DIVISION.
       PROGRAM-ID. HELLOWORLD.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  MY-NUMBER PIC 9(2) VALUE 55.
       01  MY-CHARACTER PIC A VALUE 'X'.
       01  MY-STRING PIC A(32) VALUE "Hello!".
       01  MY-MIXED-STRING PIC X(32) VALUE "ABC123".
       PROCEDURE DIVISION.
      * Note that the DISPLAY intrinsic always ends in a new line.

      * Displaying string constants.
           DISPLAY "Hello, World!".

      * Displaying variables.
           DISPLAY MY-NUMBER.
           DISPLAY MY-CHARACTER.
           DISPLAY MY-STRING.
           DISPLAY MY-MIXED-STRING.

           STOP RUN.