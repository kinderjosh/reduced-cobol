       IDENTIFICATION DIVISION.
       PROGRAM-ID. READ-EXAMPLE.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
      * Assign DATAFILE to the file "HELLOWORLD.CBL",
      * reading the file line by line.
           SELECT DATAFILE
               ASSIGN TO "examples/HELLOWORLD.CBL"
               ORGANIZATION IS LINE SEQUENTIAL
               FILE STATUS IS WS-FILESTATUS.
       DATA DIVISION.
       FILE SECTION.
      * File descriptor variable.
       FD DATAFILE.
       WORKING-STORAGE SECTION.
      * String to check if the file successfully opened.
       01 WS-FILESTATUS PIC X(02).
      * Boolean to exit a loop.
       01 WS-EOF PIC 9 VALUE FALSE.
      * Current line read.
       01 WS-DATALINE PIC X(72).
       PROCEDURE DIVISION.
      * Open the file for read only.
           OPEN INPUT DATAFILE.

      * Check if opening the file failed.
           IF WS-FILESTATUS <> "00" THEN
                DISPLAY "Error opening file. File status is " 
                     WS-FILESTATUS
                STOP RUN
           END-IF.

      * Read the file line by line until we reach the end.
           PERFORM UNTIL WS-EOF
      * Read the current line into WS-DATALINE,
      * exit the loop when we reach the end of the file,
      * if we haven't yet, display the current line read.
               READ DATAFILE INTO WS-DATALINE
                   AT END MOVE TRUE TO WS-EOF
                   NOT AT END DISPLAY WS-DATALINE
           END-PERFORM.
    
      * Done with the file, close it.
           CLOSE DATAFILE.
           STOP RUN.
