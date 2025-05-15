#!/usr/bin/env python3
import subprocess
import re
from collections import defaultdict

codefiles = r"(\.[ch](pp)?|\.lua|\.md|\.cmake|\.java|\.gradle|Makefile|CMakeLists\.txt)$"

# two minor versions back, for "Active Contributors"
REVS_ACTIVE = "5.10.0..HEAD"
# all time, for "Previous Contributors"
REVS_PREVIOUS = "HEAD"

CUTOFF_ACTIVE = 3
CUTOFF_PREVIOUS = 21

# For a description of the points system see:
# <https://github.com/luanti-org/luanti/pull/9593#issue-398677198>

def load(revs):
	points = defaultdict(int)
	p = subprocess.Popen(["git", "log", "--mailmap", "--pretty=format:%h %aN <%aE>", revs],
		stdout=subprocess.PIPE, universal_newlines=True)
	for line in p.stdout:
		hash, author = line.strip().split(" ", 1)
		n = 0

		p2 = subprocess.Popen(["git", "show", "--numstat", "--pretty=format:", hash],
			stdout=subprocess.PIPE, universal_newlines=True)
		for line in p2.stdout:
			added, deleted, filename = re.split(r"\s+", line.strip(), maxsplit=2)
			if re.search(codefiles, filename) and added != "-":
				n += int(added)
		p2.wait()

		if n == 0:
			continue
		if n > 1200:
			n = 8
		elif n > 700:
			n = 4
		elif n > 100:
			n = 2
		else:
			n = 1
		points[author] += n
	p.wait()

	# Some authors duplicate? Don't add manual workarounds here, edit the .mailmap!
	for author in ("updatepo.sh <script@mt>", "Weblate <42@minetest.ru>",
		"import <script@mt>", "minetest <minetest@minetest.net>"):
		points.pop(author, None)
	return points

points_active = load(REVS_ACTIVE)
points_prev = load(REVS_PREVIOUS)

with open("results.txt", "w") as f:
	for author, points in sorted(points_active.items(), key=(lambda e: e[1]), reverse=True):
		if points < CUTOFF_ACTIVE: break
		points_prev.pop(author, None) # active authors don't appear in previous
		f.write("%d\t%s\n" % (points, author))
	f.write('\n---------\n\n')
	once = True
	for author, points in sorted(points_prev.items(), key=(lambda e: e[1]), reverse=True):
		if points < CUTOFF_PREVIOUS and once:
			f.write('\n---------\n\n')
			once = False
		f.write("%d\t%s\n" % (points, author))
