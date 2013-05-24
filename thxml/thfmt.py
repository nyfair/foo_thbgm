import struct

format = '16siiii20s'
with open('thbgm.fmt', 'rb') as thbgm:
	bgm = thbgm.read(52)
	title, offset, nonuse1, headlen, totallen, nonuse2 = struct.unpack(format, bgm)
	print('<bgm pos="%s,%s,%s">%s</bgm>' % (offset, headlen, totallen-headlen, title))
