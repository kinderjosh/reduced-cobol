      * A program that counts the number of lines in a file.
       IDENTIFICATION DIVISION.
       PROGRAM-ID. COUNTLINES-EXAMPLE.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
      * Specify a file to read that is line sequential,
      * assign the name to a variable, and specify a file status.
           SELECT SRCFILE
               ASSIGN TO WS-FILEPATH
               ORGANIZATION IS LINE SEQUENTIAL
               FILE STATUS IS WS-FILESTATUS.
       DATA DIVISION.
       FILE SECTION.
       FD SRCFILE.
       WORKING-STORAGE SECTION.
      * Some variables for the reading and parsing the file.
       01 WS-FILEPATH PIC X(128).
       01 WS-FILESTATUS PIC X(2).
       01 WS-FILECONTENT PIC X(10240).
       01 WS-LINE PIC X(256).
       01 WS-EOF PIC 9.
       01 WS-LINECOUNT PIC Z9(08).
       01 WS-IDX PIC 9(08).
       01 WS-NUM PIC S9(08).
       PROCEDURE DIVISION.
      * Accept the file path from the user.
           DISPLAY "File path to open: ".
           ACCEPT WS-FILEPATH.
           PERFORM READ-SRC.

      * Count the lines in the path specified.
           PERFORM COUNT-LINES.
           DISPLAY WS-FILEPATH " contains " WS-LINECOUNT " lines".
           STOP RUN.

       READ-SRC.
           OPEN INPUT SRCFILE.

      * Check that the file exists.
           IF WS-FILESTATUS <> "00" THEN
               DISPLAY "Error: Failed to open '" WS-FILEPATH "'"
               EXIT PROGRAM
           END-IF.

      * Read the first line, if it's empty then WS-EOF will be TRUE.
           READ SRCFILE INTO WS-LINE
               AT END MOVE TRUE TO WS-EOF
               NOT AT END MOVE FALSE TO WS-EOF

      * Read until WS-EOF is TRUE, meaning the end of the file.
           PERFORM UNTIL WS-EOF
      * Append the last read line onto WS-FILECONTENT.
               STRING WS-FILECONTENT DELIMITED BY SIZE
                   WS-LINE DELIMITED BY SIZE
                   INTO WS-FILECONTENT
               END-STRING

      * Read the next line, set WS-EOF to TRUE if it is the last line.
               READ SRCFILE INTO WS-LINE
                   AT END MOVE TRUE TO WS-EOF

      * If we still have more lines to go, then append a newline.
               IF NOT WS-EOF THEN
                   STRING WS-FILECONTENT DELIMITED BY SIZE
                       "\n" DELIMITED BY SIZE
                       INTO WS-FILECONTENT
                   END-STRING
               END-IF
           END-PERFORM.

      * Done reading, close the file.
           CLOSE SRCFILE.

       COUNT-LINES.
      * Start on line 0 because the file could be empty.
           MOVE 0 TO WS-LINECOUNT.

           MOVE 1 TO WS-IDX.

           PERFORM UNTIL WS-FILECONTENT(WS-IDX) = '\0'
      * This file isn't empty, so if WS-LINECOUNT is still 0, make it 1.
               IF WS-IDX = 1 THEN
                   MOVE 1 TO WS-LINECOUNT
               END-IF

      * If we found a newline character, then increment WS-LINECOUNT.
               IF WS-FILECONTENT(WS-IDX) = '\n' THEN
                   ADD 1 TO WS-LINECOUNT
               END-IF

      * Increment the index.
               ADD 1 TO WS-IDX
           END-PERFORM.

       END PROGRAM COUNTLINES-EXAMPLE.
