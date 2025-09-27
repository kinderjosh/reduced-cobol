       IDENTIFICATION DIVISION.
       PROGRAM-ID. TESTPROG.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 WS-NAME PIC A(32) VALUE "John".
       01 WS-LENGTH PIC 9(05).
       PROCEDURE DIVISION.
      * Concatenate " Smith" onto WS-NAME to make "John Smith".

      * Load WS-NAME until the first space, since it doesnt have a space,
      * the whole thing is loaded.
           STRING WS-NAME DELIMITED BY SPACE
      * Append Smith onto it, delimited by its size, basically
      * appending the whole string onto it.
               " Smith" DELIMITED BY SIZE
      * Store the result string into WS-NAME.
               INTO WS-NAME
      * Store the length of the result string in WS-LENGTH.
               WITH POINTER WS-LENGTH
           END-STRING.

      * Display the full name and its length.
           DISPLAY "'" WS-NAME "' is " WS-LENGTH " characters long".

           STOP RUN.
