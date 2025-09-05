       IDENTIFICATION DIVISION.
       PROGRAM-ID. INSPECT-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * String to check.
       01 WS-STRING PIC A(32) VALUE "John Smith".
      * Occurrence count.
       01 WS-COUNT PIC 9(02).
      * Total string length.
       01 WS-TOTAL PIC 9(02).
       PROCEDURE DIVISION.
      * Check "John Smith" and count all 'J' that come before 'S',
      * and count the total number of characters.
           INSPECT WS-STRING TALLYING 
               WS-COUNT FOR ALL 'J' BEFORE INITIAL 'S'
               WS-TOTAL FOR CHARACTERS.

      * Print occurrence count.
           DISPLAY "'J' occurs " WS-COUNT " times before 'S' in '"
               WS-STRING "'".

      * Replace all 'n' with 'e' after the first 'o', 
      * and the first 'n' with 'b'.
      * WARNING: This feature is very buggy and chaining multiple
      * statements together can cancel eachother out.
           INSPECT WS-STRING REPLACING 
               ALL 'n' BY 'e' AFTER INITIAL 'o'
               FIRST 'h' BY 'b'.

           DISPLAY WS-STRING.

           STOP RUN.
