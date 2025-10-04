       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITHMETIC-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * Some signed integers.
       01 A PIC S9(05) VALUE 10.
       01 B PIC S9(05) VALUE 4.
       01 C PIC S9(05) VALUE ZERO.
      * A floating point.
       01 RESULT PIC S9(05)V9(02).
       PROCEDURE DIVISION.
      * Process simple arithmetic using intrinsics.
           ADD 5 TO A.
           SUBTRACT 2 FROM B.

      * Give the value to another variable if needed.
           ADD B TO A GIVING C.

      * MULTIPLY and DIVIDE must explicity state GIVING.
           MULTIPLY C BY 8 GIVING C.
           DIVIDE C BY 3 GIVING C.

      * Modulus with remainder.
           DIVIDE C BY 5 GIVING C REMAINDER C.

      * Process complex arithmetic expressions with COMPUTE.
           COMPUTE RESULT = (A + 2 - B * C) / 3.
           DISPLAY RESULT.
           STOP RUN.
