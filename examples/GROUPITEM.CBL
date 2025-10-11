       IDENTIFICATION DIVISION.
       PROGRAM-ID. TESTPROG.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * Define a group item for a person, with a name and age field,
      * which occurs 2 times making it a group item table.
      * In group items, every following item that has a lower level
      * is included in the group item.
       01 WS-PEOPLE OCCURS 2 TIMES.
           05 WS-NAME PIC X(32).
           05 WS-AGE PIC 9(02).
      * Define a single group item for some kind of co ordinates.
       01 WS-POINT.
           05 WS-X PIC 9(02).
           05 WS-Y PIC 9(02).
       PROCEDURE DIVISION.
      * Assign and print values to the first person 
      * in the group item table.
           MOVE "John Smith" TO WS-NAME(1).
           MOVE 21 TO WS-AGE(1).
           DISPLAY WS-NAME(1) " is " WS-AGE(1).

      * Assign and print values to the second person 
      * in the group item table.
           MOVE "Jane Smith" TO WS-NAME(2).
           MOVE 29 TO WS-AGE(2).
           DISPLAY WS-NAME(2) " is " WS-AGE(2).

      * Assign and print values to the single group item fields.
      * Since it's only a single group item, we don't have to index it.
           MOVE 2 TO WS-X.
           MOVE 3 TO WS-Y.
           DISPLAY "x: " WS-X ", y: " WS-Y.

           STOP RUN.