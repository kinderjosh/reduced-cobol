       IDENTIFICATION DIVISION.
       PROGRAM-ID. PERFORM-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 WS-CHAR PIC A VALUE 'A'.
       01 WS-COUNT PIC 9 VALUE 3.
       01 WS-I PIC 9(05).
       PROCEDURE DIVISION.
           DISPLAY "Count:".

      * Perform a procedure a known amount of times.
      * It must be a constant integer.
           PERFORM SAY-HI 5 TIMES.

           DISPLAY "Condition:".

      * Perform a procedure until a condition is true.
           PERFORM SAY-HI-DEC-COUNT UNTIL WS-COUNT = 0.

      * Perform a series of clauses with an iterator variable.
           PERFORM VARYING WS-I FROM 1 BY 1 UNTIL WS-I > 5
               DISPLAY WS-I
           END-PERFORM.

      * Perform a series of clauses until a condition is true.
           MOVE 0 TO WS-I.

           PERFORM UNTIL WS-I = 5
               ADD 1 TO WS-I
               DISPLAY WS-I
           END-PERFORM.

           STOP RUN.
      
       SAY-HI.
           DISPLAY "Hi!".

       SAY-HI-DEC-COUNT.
           PERFORM SAY-HI.
           DISPLAY "Count: " WS-COUNT.
           SUBTRACT 1 FROM WS-COUNT.
       
       END PROGRAM PERFORM-EXAMPLE.