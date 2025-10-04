       IDENTIFICATION DIVISION.
       PROGRAM-ID. WRITE-EXAMPLE.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
      * Assign DATAFILE to the file "write.txt",
      * we don't need ORGANIZATION since we aren't reading from it.
           SELECT DATAFILE
               ASSIGN TO "examples/write.txt"
               FILE STATUS IS WS-FILESTATUS.
       DATA DIVISION.
       FILE SECTION.
      * File descriptor variable.
       FD DATAFILE.
       WORKING-STORAGE SECTION.
      * String to check if the file successfully opened.
       01 WS-FILESTATUS PIC X(02).
      * Text to write.
       01 WS-DATA PIC X(64) VALUE "Some text\nto write\nfrom COBOL.\n".
       01 WS-APPENDED-DATA PIC X(64) VALUE "This text was appended.".
       PROCEDURE DIVISION.
      * Open the file for writing.
           OPEN OUTPUT DATAFILE.

      * Check if opening the file failed.
           IF WS-FILESTATUS <> "00" THEN
                DISPLAY "Error opening file. File status is " 
                     WS-FILESTATUS
                STOP RUN
           END-IF.

      * Fill the file with a string.
           WRITE WS-DATA.
      * If the file is opened with OUTPUT or IO, WRITE will overwrite 
      * any contents in the file, then write the string into the file.

      * Let's reopen it for EXTEND to WRITE more data to the file,
      * without discarding its contents.
           CLOSE DATAFILE.
           OPEN EXTEND DATAFILE.

      * Append some text.
           WRITE WS-APPENDED-DATA.

      * Done with the file, close it.
           CLOSE DATAFILE.
           STOP RUN.
