<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
<meta http-equiv="content-type" content=
"text/html; charset=us-ascii">
<title>autosave</title>
<meta name="author" content="Tim Mooney">
</head>
<body>

<ul>
<li>R5-4, R5-4-1, R5-4-2:
<P>Array restore can fail to parse correctly lines longer than 120 characters.

<li>R5-4, R5-4-1:
<P>set_pass1_restoreFile() is wrongly defined for iocsh.  Here's how to fix it:
<pre>
STATIC void set_pass1_CallFunc(const iocshArgBuf *args)
 {
-    set_pass1_restoreFile(args[0].sval, args[2].sval);
+    set_pass1_restoreFile(args[0].sval, args[1].sval);
 }
</pre>
<P>Thanks to Ralph Lange for finding and fixing this.

<li>R5-0, R5-1, R5-2:
<P>autosave fails to detect and reject a truncated .sav file, and segfaults
if the file was truncated in the middle of an array.

<P>long-string PV's longer than 120 characters are not read to completion, so
remaining string content is treated as a new PV name/value.

<li>R4-6 and earlier:
<P>array PV's whose representation in a .sav file is exactly
119 characters long result in the PV being handled twice, the second time as
a scalar with no data.  For an array of char, this has had the effect of setting
the first element to zero.

<li>R4-1:
<P>.sav files must have a header line.  Too sensitive to errors reported in
errno.  Status PV's restricted to PV_NAME_LEN chars.  Blank but not empty 
lines cause problems.

<li>R4-0:
<P>Not too smart about array PV's changing number of elements.  <P>Kills itself
if it can't find its status PV's.

<li>R3-5:
<P>Note that this 3.13-compatible software is not in the form of a module.
</ul>
</body>
