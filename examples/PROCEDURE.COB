       IDENTIFICATION DIVISION.
       PROGRAM-ID. PROCEDURE-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * All variables are global, they can be used in all procedures.
       01  X PIC 9 VALUE 0.
       PROCEDURE DIVISION.
      * The program starts here.

      * Perform these procedures and come back here.
           PERFORM CHECKX.

           PERFORM SETX.
      * X should now be 1, meaning it should display a message the next time
      * CHECKX is performed.

           PERFORM CHECKX.
    
      * STOP RUN terminates the program.
           STOP RUN.

      * Procedures are created just like this. They end when either
      * the program is ended as seen below, or a new procedure is created.
       SETX.
           MOVE 1 TO X.

      * New procedure, SETX stops here.
       CHECKX.
           IF X = 1 THEN
               DISPLAY "X is 1!"
           END-IF.

      * Add this at the end to tell CHECKX that it should end.
       END PROGRAM PROCEDURE-EXAMPLE.