       IDENTIFICATION DIVISION.
       PROGRAM-ID. TYPECONV-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * A numerical variable.
       01 WS-NUM PIC S9(04)V9(02) VALUE 55.
      * A string to store the conversion into.
       01 WS-STRING PIC X(07).
       PROCEDURE DIVISION.
      * A simple move will convert types.
           MOVE WS-NUM TO WS-STRING.
           DISPLAY WS-STRING.

      * Convert a valid string to a numeric with a simple move too.
           MOVE "3.14" TO WS-STRING.
           MOVE WS-STRING TO WS-NUM.
           DISPLAY WS-NUM.
           STOP RUN.