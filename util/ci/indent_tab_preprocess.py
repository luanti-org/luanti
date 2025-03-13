#!/usr/bin/python3

import sys
import re

# preprocess files for indent check

# Code processing:
#
# Tabs are kept.
# Spaces after tabs are removed if indent is not smaller than in previous line.
#
# Example:
#
# <tab>a = 6;
# <spc>b = 5;
# <tab><spc>if (
# <tab><tab><tab><spc><spc>) {
# <tab><tab>c = 7;
# <tab><spc>}
#
# Result:
#
# <tab>a = 6;
# <spc>b = 5;
# <tab>if (
# <tab><tab><tab>) {
# <tab><tab>c = 7;
# <tab><spc>}

# Multiline comments processing:
#
# It is expected that tabs indent from comment begining is
# used as indent on begining of every commented line.
#
# Example:
#
# <tab>/*
# <spc><spc>A
# <tab><tab>B
# <tab><spc>C
# D
# <tab><spc>*/
# <tab><tab>/*
# <spc><tab><spc>R
#	<tab><spc><tab>*/
#
# Result:
#
# <tab>/*
# <spc>A
# <tab>B
# <tab>C
# D
# <tab>*/
# <tab><tab>/*
# <spc><tab>*R
#	<tab><spc>*/


def main():
	if len(sys.argv) != 4:
		#print("Bad arguments")
		#print(sys.argv)
		exit()
	# multiline comment begining and end from command line
	mlc_begin = sys.argv[1]
	mlc_end = sys.argv[2]
	file_name = sys.argv[3]

	mlc_end_len = len(mlc_end)
	re_begin = re.escape(mlc_begin)
	re_end = re.escape(mlc_end)

	re_mlc_begin = "^.*{}".format(re_begin)
	re_mlc_begin_end = "^.*{}.*{}".format(re_begin, re_end)
	re_mlc_end = "^.*{}".format(re_end)

	file = open(file_name, "r")

	lines = []

	#print(re_mlc_begin)
	#print(re_mlc_begin_end)
	#print(re_mlc_end)
	#print(file)

	# -1 means not in the multiline comment
	# other values mean multiline comment indent in the number of tabs
	limit = -1
	# this value stores the previous line indent in code mode (not in multiline comment)
	prev = 80
	for line in file:
		if limit == -1:
			find = re.search(re_mlc_begin, line)
			if find:
				if not re.search(re_mlc_begin_end, line):
					# enter in block comment mode
					find = re.search("^\\s*", line)
					limit = find.end()
			find = re.search("^\\t*", line)
			end = find.end()
			if end < prev:
				lines.append(line)
			else:
				# in this case, there can be allowed extra trailing whitespaces
				# up to 3 trailing whitespaces are allowed
				#find = re.search("^\\t+[ ]{0,3}", line)
				# unlimited number of trailng whitespaces are allowed
				find = re.search("^\\t+[ ]*", line)
				fend = find.end() if find else 0
				if fend >= end:
					find = re.search("^\\t*", line)
					lines.append(line[0:find.end()] + line[fend:])
				else:
					lines.append(line)
			prev = end
		else:
			# in block comment mode
			find = re.search(re_mlc_end, line)
			if find:
				# erase whitechars enabled for ignore, end in block comment mode
				begin = find.end() - mlc_end_len
				if begin > limit:
					lines.append(line[0:limit] + line[begin:])
				else:
					lines.append(line)
				limit = -1
			else:
				# erase whitechars enabled for ignore
				find = re.search("^$", line)
				if find:
					lines.append(line)
				else:
					find = re.search("^\\s*", line)
					if find:
						end = find.end()
						if end > limit:
							lines.append(line[0:limit] + line[end:])
						else:
							lines.append(line)
					else:
						lines.append(line)

	file.close()
	file = open(file_name, "w")
	for line in lines:
		file.write(line)
	file.close()

if __name__ == "__main__":
	main()
