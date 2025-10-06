       IDENTIFICATION DIVISION.
       PROGRAM-ID. TESTPROG.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * Basically copy and pastes the contents of COPYBOOK.CPY into
      * the WORKING-STORAGE SECTION.
           COPY COPYBOOK.
       PROCEDURE DIVISION.
      * Display the name variable defined in the copybook.
           DISPLAY "Hello, " WS-NAME.
           STOP RUN.