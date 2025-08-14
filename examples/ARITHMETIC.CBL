       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITHMETIC-EXAMPLE.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  A PIC 9 VALUE 10.
       01  B PIC 9 VALUE 4.
       01  C PIC 9.
       01  RESULT PIC 9.
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

      * Note: I still don't know why MOD isn't recognized by the vscode
      *       theme, but it works for the transpiler.
           COMPUTE C = 3 MOD 2.
           DISPLAY C.
           STOP RUN.