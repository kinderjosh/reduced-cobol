       IDENTIFICATION DIVISION.
       PROGRAM-ID. TESTPROG.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-CHAR PIC A VALUE 'A'.
       01  WS-COUNT PIC 9 VALUE 3.
       PROCEDURE DIVISION.
           DISPLAY "Count:".

      * Perform a procedure a known amount of times.
      * It must be a constant integer.
           PERFORM SAY-HI 5 TIMES.

           DISPLAY "Condition:".

      * Perform a procedure until a condition is true.
           PERFORM SAY-HI-DEC-COUNT UNTIL WS-COUNT = 0.

           STOP RUN.
      
       SAY-HI.
           DISPLAY "Hi!".

       SAY-HI-DEC-COUNT.
           PERFORM SAY-HI.
           DISPLAY "Count: " WS-COUNT.
           SUBTRACT 1 FROM WS-COUNT.
       
       END PROGRAM TESTPROG.