      * Basic WIN32 message box in COBOL.
      * Compile this program with 
      *     cobc build -include windows examples\MESSAGEBOX.COB
       IDENTIFICATION DIVISION.
       PROGRAM-ID. MESSAGEBOX.
       PROCEDURE DIVISION.
           CALL "MessageBoxA" USING NULL, "Hello from COBOL!", "Message", 0.
           STOP RUN.