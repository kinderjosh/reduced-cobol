       IDENTIFICATION DIVISION.
       PROGRAM-ID. TESTPROG.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
      * Some data to point to.
       01 WS-NUM PIC S9(03) VALUE 256.
       01 WS-STRING PIC X(32) VALUE "Hello, World!".
      * Define a pointer with USAGE.
      * Pointers can't be assigned values on initialization.
       01 WS-PTR USAGE IS POINTER.
       PROCEDURE DIVISION.
      * Display the current value of the data.
           DISPLAY "Old: " WS-NUM.
           DISPLAY "Old: " WS-STRING.

      * Point to the address of WS-NUM in memory using SET.
           SET WS-PTR TO ADDRESS OF WS-NUM.

      * Assign a new value in the memory location of the pointer
      * using MOVE. The equivalent of a dereference.
           MOVE 128 TO WS-PTR.

      * This can also be used to change characters in strings.
      * Here we change the second character.
           SET WS-PTR TO ADDRESS OF WS-STRING.
           MOVE 'J' TO WS-PTR(2).

      * Display the changed value of the data.
           DISPLAY "New: " WS-NUM.
           DISPLAY "New: " WS-STRING.
           STOP RUN.