       IDENTIFICATION DIVISION.
       PROGRAM-ID. DISPLAY-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 MY-NUMBER PIC 9(02) VALUE 50.
       01 MY-CHARACTER PIC A VALUE 'X'.
       01 MY-STRING PIC A(32) VALUE "Hello!".
       01 MY-MIXED-STRING PIC X(32) VALUE "ABC123".
       01 HALF PIC 9.
       01 TRIMMED PIC Z9(05) VALUE 1.
       PROCEDURE DIVISION.
      * Note that the DISPLAY intrinsic always ends in a new line.

      * Displaying string constants.
           DISPLAY "Hello, World!".

      * Displaying variables.
           DISPLAY MY-NUMBER.
           DISPLAY MY-CHARACTER.
           DISPLAY MY-STRING.
           DISPLAY MY-MIXED-STRING.
           DISPLAY TRIMMED.

      * Displaying strings with variables.
      * Note that a newline is only printed when the statement is ended.
           DIVIDE MY-NUMBER BY 2 GIVING HALF.
           DISPLAY "Half of " MY-NUMBER " is " HALF.
           STOP RUN.
