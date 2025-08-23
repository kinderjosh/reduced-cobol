       IDENTIFICATION DIVISION.
       PROGRAM-ID. TESTPROG.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 WS-NAME PIC A(32) VALUE "John".
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
           END-STRING.

      * Display the full name.
           DISPLAY WS-NAME.

           STOP RUN.
