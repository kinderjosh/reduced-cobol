       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITHMETIC-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * Some signed integers.
       01 A PIC S9(02) VALUE 10.
       01 B PIC S9(02) VALUE 4.
       01 C PIC S9(02).
      * A floating point.
       01 RESULT PIC S9(04)V99.
       PROCEDURE DIVISION.
      * Process simple arithmetic using intrinsics.
           ADD 5 TO A.
           SUBTRACT 2 FROM B.

      * Give the value to another variable if needed.
           ADD B TO A GIVING C.

      * MULTIPLY and DIVIDE must explicity state GIVING.
           MULTIPLY C BY 8 GIVING C.
           DIVIDE 3 INTO C GIVING C.

      * Modulus with remainder.
           DIVIDE 5 INTO C GIVING C REMAINDER C.

      * Process complex arithmetic expressions with COMPUTE.
           COMPUTE RESULT = (A + 2 - B * C) / 3.
           DISPLAY RESULT.
           STOP RUN.
