      * Compile this file with examples/LINKAGESRC.CBL for it to work.
       IDENTIFICATION DIVISION.
       PROGRAM-ID. LINKAGE-EXAMPLE.
       DATA DIVISION.
       LINKAGE SECTION.
      * Define a variable in the linkage section that is defined in
      * another file. If preceded with 'LS-', those are ignored,
      * meaning it is only read as 'FULLNAME'.
       01 LS-FULLNAME PIC X(32).
      * Specify which linkage variables are being used.
       PROCEDURE DIVISION USING LS-FULLNAME.
           DISPLAY "Hello " LS-FULLNAME ", this was linked".
           STOP RUN.