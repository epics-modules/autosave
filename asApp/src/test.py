#!/usr/bin/env python

"""

This software tests autosave by writing random values to a list of PVs, and
comparing values retrieved from those PVs with the values written.  To use,
load the SR_test database dbLoadRecords("$(AUTOSAVE)/asApp/Db/SR_test.db",
"P=xxx:,N=100"), and add the line "file SR_test_settings.req P=xxx:" to
auto_settings.req. The python distribution you use must have PyEpics and numpy.

% python
>>> import test
>>> test.prefix = "xxx:" # or whatever your ioc prefix is
>>> test.doPuts()
>>> test.compare()
 You should see "0  differences found"
 Wait for the autosave file to be written.
 Reboot the ioc.
>>> test.compare()
 You should see "0  differences found"

"""
from __future__ import print_function

import epics
import numpy

prefix = "xxx:"

import epics
import random

pv_types = {
"SR_aoDISP.DISP":"char",
"SR_ao.PREC":"short",
"SR_bo.IVOV":"ushort",
"SR_ao.SCAN":"enum",
"SR_ao.VAL":"double",
"SR_scaler.RATE":"float",
"SR_ao.DESC":"string",
"SR_ao.OUT":"link",
"SR_longout":"long",
"SR_bi.SVAL":"ulong",
"SR_char_array":"char_array",
"SR_double_array":"double_array",
"SR_float_array":"float_array",
"SR_long_array":"long_array",
"SR_short_array":"short_array",
"SR_string_array":"string_array",
"SR_uchar_array":"uchar_array",
"SR_ulong_array":"ulong_array",
"SR_ushort_array":"ushort_array"
}

pv_values = {
"SR_aoDISP.DISP":None,
"SR_ao.PREC":None,
"SR_bo.IVOV":None,
"SR_ao.SCAN":None,
"SR_ao.VAL":None,
"SR_scaler.RATE":None,
"SR_ao.DESC":None,
"SR_ao.OUT":None,
"SR_longout":None,
"SR_bi.SVAL":None,
"SR_char_array":None,
"SR_double_array":None,
"SR_float_array":None,
"SR_long_array":None,
"SR_short_array":None,
"SR_string_array":None,
"SR_uchar_array":None,
"SR_ulong_array":None,
"SR_ushort_array":None
}

alphabet = "abcdefghijklmnopqrstuvwxyz"

def put(pv, val):
	try:
		epics.caput(pv, val)
	except:
		print("exception while putting '%s' to '%s'" % (repr(val), pv))

def doPuts(verbose=0):
	for (pv) in pv_types.keys():
		pv_type = pv_types[pv]
		if pv_type == "char" or pv_type == "uchar":
			pv_values[pv] = random.randint(0,0xff)
			put(prefix+pv, pv_values[pv])

		elif pv_type == "short":
			pv_values[pv] = random.randint(0,0xffff)-0x7fff
			put(prefix+pv, pv_values[pv])

		elif pv_type == "ushort":
			pv_values[pv] = random.randint(0,0xffff)
			put(prefix+pv, pv_values[pv])

		elif pv_type == "enum":
			p = epics.PV(prefix+pv)
			i = random.randint(0,len(p.enum_strs)-1)
			# SCAN field, can use I/O Intr (2)
			if i==2: i=3
			pv_values[pv] = i
			put(prefix+pv, pv_values[pv])

		elif pv_type == "double":
			pv_values[pv] = random.random()
			s = pv_values[pv].__str__()
			if verbose: print("double: writing string value '%s' " % s)
			put(prefix+pv, s)

		elif pv_type == "float":
			pv_values[pv] = random.random()
			s = pv_values[pv].__str__()
			put(prefix+pv, s)

		elif pv_type == "string":
			s = ""
			m = random.randint(0, 39)
			for i in range(m):
				s = s+alphabet[random.randint(0,25)]
			pv_values[pv] = s
			put(prefix+pv, s)

		elif pv_type == "link":
			s = ""
			m = random.randint(0, 31)
			for i in range(m):
				s = s+alphabet[random.randint(0,25)]
			pv_values[pv] = s + " NPP NMS"
			put(prefix+pv, pv_values[pv])

		elif pv_type == "long":
			pv_values[pv] = random.randint(0,0xffffffff)-0x7fffffff
			put(prefix+pv, pv_values[pv])

		elif pv_type == "ulong":
			pv_values[pv] = random.randint(0,0xffffffff)
			put(prefix+pv, pv_values[pv])

		#arrays

		elif pv_type == "char_array":
			p = epics.PV(prefix+pv)
			values = []
			n = random.randint(1, p.nelm)
			for i in range(n):
				values.append(random.randint(0,0xff))
			pv_values[pv] = values
			if verbose>1: print("char_array: writing values ", values)
			put(prefix+pv, numpy.array(pv_values[pv]))

		elif pv_type == "double_array":
			p = epics.PV(prefix+pv)
			values = []
			n = random.randint(1, p.nelm)
			for i in range(n):
				values.append(random.random())
			pv_values[pv] = values
			if verbose>1: print("double_array: writing values ", values)
			put(prefix+pv, numpy.array(pv_values[pv]))

		elif pv_type == "float_array":
			p = epics.PV(prefix+pv)
			values = []
			n = random.randint(1, p.nelm)
			for i in range(n):
				values.append(random.random())
			pv_values[pv] = values
			if verbose>1: print("float_array: writing values ", values)
			put(prefix+pv, numpy.array(pv_values[pv]))

		elif pv_type == "long_array":
			p = epics.PV(prefix+pv)
			values = []
			n = random.randint(1, p.nelm)
			for i in range(n):
				values.append(random.randint(0,0xffffffff)-0x7fffffff)
			pv_values[pv] = values
			if verbose>1: print("long_array: writing values ", values)
			put(prefix+pv, numpy.array(pv_values[pv]))

		elif pv_type == "short_array":
			p = epics.PV(prefix+pv)
			values = []
			n = random.randint(1, p.nelm)
			for i in range(n):
				values.append(random.randint(0,0xffff)-0x7fff)
			pv_values[pv] = values
			if verbose>1: print("short_array: writing values ", values)
			put(prefix+pv, numpy.array(pv_values[pv]))

		elif pv_type == "string_array":
			p = epics.PV(prefix+pv)
			values = []
			n = random.randint(1, p.nelm)
			for i in range(n):
				s = ""
				m = random.randint(0, 39)
				for j in range(m):
					s = s+alphabet[random.randint(0,25)]
				values.append(s)
			pv_values[pv] = values
			if verbose>1: print("string_array: writing values ", values)
			put(prefix+pv, pv_values[pv])

		elif pv_type == "uchar_array":
			p = epics.PV(prefix+pv)
			values = []
			n = random.randint(1, p.nelm)
			for i in range(n):
				values.append(random.randint(0,0xff))
			pv_values[pv] = values
			if verbose>1: print("uchar_array: writing values ", values)
			put(prefix+pv, numpy.array(pv_values[pv]))

		elif pv_type == "ulong_array":
			p = epics.PV(prefix+pv)
			values = []
			n = random.randint(1, p.nelm)
			for i in range(n):
				values.append(random.randint(0,0xffffffff))
			pv_values[pv] = values
			if verbose>1: print("ulong_array: writing values ", values)
			put(prefix+pv, numpy.array(pv_values[pv]))

		elif pv_type == "ushort_array":
			p = epics.PV(prefix+pv)
			values = []
			n = random.randint(1, p.nelm)
			for i in range(n):
				values.append(random.randint(0,0xffff))
			pv_values[pv] = values
			if verbose>1: print("ushort_array: writing values ", values)
			put(prefix+pv, numpy.array(pv_values[pv]))
		else:
			if verbose: print("pv_type", pv_type, "not supported"
		
	for (pv) in pv_types.keys():
		p = epics.PV(prefix+pv)
		if p.nelm <= 1:
			if verbose: print(prefix+pv, " = ", pv_values[pv])
		else:
			if verbose>1: print(prefix+pv, " = ", pv_values[pv])

def different(a,b):
	if type(a) == type(1.2):
		r = abs(a-b) > .000001
	else:
		r = (a != b)
	return(r)

def compare() :
	numDifferences=0
	for (pv) in pv_types.keys():
		pv_type = pv_types[pv]
		currVal = epics.caget(prefix+pv)
		if type(pv_values[pv]) != type([]):
			if different(pv_values[pv], currVal):
				print("PV:", prefix+pv, "different:")
				print("\tsaved  =", pv_values[pv])
				print("\tcurrent=", currVal)
				numDifferences = numDifferences+1
		else:
			for i in range(len(pv_values[pv])):
				if different(pv_values[pv][i], currVal[i]):
					print("PV:", i, prefix+pv, "different:")
					print("\tsaved  =", pv_values[pv][i])
					print("\tcurrent=", currVal[i])
					numDifferences = numDifferences+1
	print( numDifferences, " differences found"
