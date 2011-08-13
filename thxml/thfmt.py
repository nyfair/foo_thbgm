import struct

thbgm = open('thbgm.fmt', 'rb')
format = '16siiii20s'

while True:
	bgm = thbgm.read(52)
	if len(bgm) < 52:
		break
	title, offset, nonuse1, headlen, totallen, nonuse2 = struct.unpack(format, bgm)
	print('<bgm pos="%s,%s,%s">%s</bgm>' % (offset, headlen, totallen-headlen, title))
	
thbgm.close()