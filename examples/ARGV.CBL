      * Example program to use UNSTRING to split arguments from
      * the command line into a table.
       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARGV-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * Large string for the command line arguments.
       01 WS-CMDLINE PIC X(256).
      * Argument count.
       01 WS-ARGC PIC 9(02).
      * Argument table, unknown size at compile time, make 3.
       01 WS-ARGV PIC X(32) OCCURS 3 TIMES INDEXED BY WS-ARG.
       PROCEDURE DIVISION.
      * Call a procedure to get the argument count.
           PERFORM GET-ARGC.

      * Print the argument count.
           DISPLAY WS-CMDLINE.
           DISPLAY "Argument count: " WS-ARGC.

      * Call a procedure to fill the argument table.
           PERFORM GET-ARGV.

      * Print every argument from the table.
           SET WS-ARG TO 1.

           PERFORM UNTIL WS-ARG > WS-ARGC
                DISPLAY "Argument " WS-ARG ": " WS-ARGV(WS-ARG)
                SET WS-ARG UP BY 1
           END-PERFORM.

           STOP RUN.

       GET-ARGC.
      * Load a string containing the arguments into WS-CMDLINE.
           ACCEPT WS-CMDLINE FROM COMMAND-LINE.

      * Each argument is separated by a space, so count each space
      * and add 1 for the executable argument.
           INSPECT WS-CMDLINE TALLYING
               WS-ARGC FOR ALL ' '.

      * Add 1 for the executable.
           ADD 1 TO WS-ARGC.

       GET-ARGV.
      * Split at every space and fill the table in.
      * We only accept maximum 3 arguments here.
           UNSTRING WS-CMDLINE DELIMITED BY SPACE
                INTO WS-ARGV(1) WS-ARGV(2) WS-ARGV(3)
           END-UNSTRING.
      
       END PROGRAM ARGV-EXAMPLE.
