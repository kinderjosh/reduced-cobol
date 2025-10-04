       IDENTIFICATION DIVISION.
       PROGRAM-ID. COMP-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * COMP, COMP-4 and BINARY have no effect.
       01 WS-INT PIC S9(05) USAGE IS COMP-4 VALUE 5.
      * COMP-1 is a single precision float. It cannot be used with PIC.
       01 WS-SINGLE USAGE IS COMP-1 VALUE 1.23.
      * COMP-2 is a double precision float. It cannot be used with PIC.
       01 WS-DOUBLE USAGE IS COMP-2 VALUE 4.56.
      * COMP-5 is an integer that uses hardware widths.
      * A number over 10 digits is treated as the widest integer.
      * In this case, this variable equals a "long long" in C.
       01 WS-HARD PIC S9(12) USAGE IS COMP-5 VALUE 1234567890.
       PROCEDURE DIVISION.
           DISPLAY WS-INT.
           DISPLAY WS-SINGLE.
           DISPLAY WS-DOUBLE.
           DISPLAY WS-HARD.
           STOP RUN.