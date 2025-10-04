       IDENTIFICATION DIVISION.
       PROGRAM-ID. IFELSE-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 A PIC 9(05) VALUE 5.
       01 B PIC 9(05) VALUE 10.
       PROCEDURE DIVISION.
      * Note that . are not used inside of nested blocks like if statements.

      * <, <=, >, >=, <> and = can be used for shorthand operators.
           IF A < B THEN
               ADD 1 TO A
               DISPLAY A
           END-IF.
    
      * You can also use their long form.
           IF A IS LESS THAN B THEN
               ADD 1 TO A
               DISPLAY A
           END-IF.

      * Lng forms are quite long...
           IF B IS GREATER THAN OR EQUAL TO B THEN
               SUBTRACT 1 FROM B
               DISPLAY B
           END-IF.

      * <> is the same as != in other languages, = in conditions
      * is the same as ==.
           MOVE B TO A.

           IF B IS EQUAL TO 1000 THEN
               DISPLAY "B is 1000!"
           ELSE
      * COBOL doesn't have elseif statements like in other languages,
      * so you have to just do it like this.
               IF B = A THEN
                   DISPLAY "B is equal to A!"
               END-IF 

               MOVE 0 TO B

               IF B <> A THEN
                   DISPLAY "B is not equal to A!"
               ELSE
                   IF B NOT EQUAL TO 5 THEN
                       DISPLAY "B is not equal to 5!"
                   END-IF
               END-IF
           END-IF.

      * AND and OR.
           MOVE 10 TO A.
           MOVE 5 TO B.

           IF A = 10 AND B = 5 THEN
               DISPLAY "AND!"
           END-IF.

           IF A = 500 OR B = 5 THEN
               DISPLAY "OR!"
           END-IF.

           STOP RUN.