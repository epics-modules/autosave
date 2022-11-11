---
layout: default
title: Known Issues
nav_order: 5
---


autosave

- R5-4, R5-4-1, R5-4-2: Array restore can fail to parse correctly lines longer than 120 characters.
- R5-4, R5-4-1: set\_pass1\_restoreFile() is wrongly defined for iocsh. Here's how to fix it:
    
    ```
    
    STATIC void set_pass1_CallFunc(const iocshArgBuf *args)
     {
    -    set_pass1_restoreFile(args[0].sval, args[2].sval);
    +    set_pass1_restoreFile(args[0].sval, args[1].sval);
     }
    ```
    
    Thanks to Ralph Lange for finding and fixing this.
- R5-0, R5-1, R5-2: autosave fails to detect and reject a truncated .sav file, and segfaults if the file was truncated in the middle of an array.
    
    long-string PV's longer than 120 characters are not read to completion, so remaining string content is treated as a new PV name/value.
- R4-6 and earlier: array PV's whose representation in a .sav file is exactly 119 characters long result in the PV being handled twice, the second time as a scalar with no data. For an array of char, this has had the effect of setting the first element to zero.
- R4-1: .sav files must have a header line. Too sensitive to errors reported in errno. Status PV's restricted to PV\_NAME\_LEN chars. Blank but not empty lines cause problems.
- R4-0: Not too smart about array PV's changing number of elements.
    
    Kills itself if it can't find its status PV's.
- R3-5: Note that this 3.13-compatible software is not in the form of a module.
