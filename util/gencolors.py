import csv,os,sys,sqlite3
from contextlib import closing

from PIL import Image

def calculateColor(path):
    image = Image.open(path)
    image = image.resize((1,1),resample=1).convert("RGB")
    ret = image.getcolors()
    if ret is None: return (0xff,0xaa,0xaa)
    try: 
        ret.sort(key=lambda e: e[0])
        median = ret[len(ret)/2][1]
        return median[0:3]
    except:
        print(path,ret)
        raise

db = sqlite3.Connection("colors.sqlite")

db.execute("""
CREATE TABLE IF NOT EXISTS colors (
  id INTEGER PRIMARY KEY,
  name TEXT UNIQUE,
  r INTEGER,
  g INTEGER,
  b INTEGER)
""")

derp = {}

for top,dirs,files in os.walk("."):
    for f in files:
        if f.endswith('.png'):
            derp[f] = os.path.join(top,f)

print """
0 128 128 128   # CONTENT_STONE
2 39 66 106     # CONTENT_WATER
3 255 255 0     # CONTENT_TORCH
9 39 66 106     # CONTENT_WATERSOURCE
e 117 86 41     # CONTENT_SIGN_WALL
f 128 79 0      # CONTENT_CHEST
10 118 118 118  # CONTENT_FURNACE
15 103 78 42    # CONTENT_FENCE
1e 162 119 53   # CONTENT_RAIL
1f 154 110 40   # CONTENT_LADDER
20 255 100 0    # CONTENT_LAVA
21 255 100 0    # CONTENT_LAVASOURCE
800 107 134 51  # CONTENT_GRASS
801 86 58 31    # CONTENT_TREE
802 48 95 8     # CONTENT_LEAVES
803 102 129 38  # CONTENT_GRASS_FOOTSTEPS
804 178 178 0   # CONTENT_MESE
805 101 84 36   # CONTENT_MUD
808 104 78 42   # CONTENT_WOOD
809 210 194 156 # CONTENT_SAND
80a 123 123 123 # CONTENT_COBBLE
80b 199 199 199 # CONTENT_STEEL
80c 183 183 222 # CONTENT_GLASS
80d 219 202 178 # CONTENT_MOSSYCOBBLE
80e 70 70 70    # CONTENT_GRAVEL
80f 204 0 0     # CONTENT_SANDSTONE
810 0 215 0     # CONTENT_CACTUS
811 170 50 25   # CONTENT_BRICK
812 104 78 42   # CONTENT_CLAY
813 58 105 18   # CONTENT_PAPYRUS
814 196 160 0   # CONTENT_BOOKSHELF
815 205 190 121 # CONTENT_JUNGLETREE
816 62 101 25   # CONTENT_JUNGLEGRASS
817 255 153 255 # CONTENT_NC
818 102 50 255  # CONTENT_NC_RB
819 200 0 0     # CONTENT_APPLE
"""

with closing(db.cursor()) as c:
    for name,filename in csv.reader(sys.stdin):
        c.execute("SELECT r,g,b FROM colors WHERE name = ?",(name,))
        row = c.fetchone()
        if row:
            r,g,b = row
        else:
            try: r,g,b = calculateColor(derp[filename])
            except KeyError: r,g,b = (0xff,0x80,0x80)
            c.execute("INSERT INTO colors (name,r,g,b) VALUES (?,?,?,?)",
                      (name,r,g,b));
        print name,r,g,b

db.commit()

