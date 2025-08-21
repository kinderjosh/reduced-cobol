       IDENTIFICATION DIVISION.
       PROGRAM-ID. TABLE-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * Table of 26 characters for the alphabet.
       01 WS-ALPHABET PIC A OCCURS 26 TIMES
              INDEXED BY WS-INDEX.
      * Character for setting each table item.
       01 WS-CHAR PIC A.
       PROCEDURE DIVISION.
      * Fill the table and display the whole thing.
           PERFORM FILL-ALPHABET.
           PERFORM DISPLAY-ALPHABET.
           STOP RUN.

       FILL-ALPHABET.
           SET WS-INDEX TO 1.
           MOVE 'A' TO WS-CHAR.

           PERFORM UNTIL WS-CHAR > 'Z'
               MOVE WS-CHAR TO WS-ALPHABET(WS-INDEX)
               SET WS-INDEX UP BY 1
               ADD 1 TO WS-CHAR
           END-PERFORM.

       DISPLAY-ALPHABET.
           SET WS-INDEX TO 1.

           PERFORM UNTIL WS-INDEX > 26
               DISPLAY WS-ALPHABET(WS-INDEX)
               SET WS-INDEX UP BY 1
           END-PERFORM.

       END PROGRAM TABLE-EXAMPLE.