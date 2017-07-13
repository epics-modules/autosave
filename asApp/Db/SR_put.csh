#!/bin/csh
set P=$1
caput ${P}SR_aoDISP.DISP 1
caput ${P}SR_ao.PREC 3
caput ${P}SR_bo.IVOV 3
caput ${P}SR_ao.SCAN 3
caput ${P}SR_ao.VAL 3.1
caput ${P}SR_scaler.RATE 3.1
caput ${P}SR_ao.DESC desc
caput ${P}SR_ao.OUT "abc:rec PP"
caput ${P}SR_longout 3
caput ${P}SR_bi.SVAL 3
caput -S ${P}SR_char_array "abc"
caput -a ${P}SR_double_array 3 1 2 3
caput -a ${P}SR_float_array 3 1 2 3
caput -a ${P}SR_long_array 3 1 2 3
caput -a ${P}SR_short_array 3 1 2 3
caput -a ${P}SR_string_array 3 abc def ghi
caput -a ${P}SR_uchar_array 3 1 2 3
caput -a ${P}SR_ulong_array 3 1 2 3
caput -a ${P}SR_ushort_array 3 1 2 3
