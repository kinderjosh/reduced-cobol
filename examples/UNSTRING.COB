       IDENTIFICATION DIVISION.
       PROGRAM-ID. UNSTRING-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * The string to split.
       01 WS-FULL PIC X(64) VALUE "COBOL is hella annoying".
      * Variables to store the splits into.
       01 WS-FIRST PIC A(16).
       01 WS-SECOND PIC A(16).
       01 WS-THIRD PIC A(16).
       01 WS-FOURTH PIC A(16).
       PROCEDURE DIVISION.
      * Split WS-FULL at each space.
           UNSTRING WS-FULL DELIMITED BY SPACE
      * Store the splits into these variables.
               INTO WS-FIRST WS-SECOND WS-THIRD WS-FOURTH
           END-UNSTRING.

      * Print each split variable.
           DISPLAY "First: " WS-FIRST.
           DISPLAY "Second: " WS-SECOND.
           DISPLAY "Third: " WS-THIRD.
           DISPLAY "Fourth: " WS-FOURTH.
           STOP RUN.

