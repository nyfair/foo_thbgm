import glob
import struct

format = '16siiii20s'
for file in glob.glob('*.fmt'):
	with open(file, 'rb') as thbgm:
		while True:
			bgm = thbgm.read(52)
			if len(bgm) < 52:
				thbgm.close()
				break
			title, offset, nonuse1, headlen, totallen, nonuse2 = struct.unpack(format, bgm)
			print('<bgm pos="%s,%s,%s">%s</bgm>' % (offset, headlen, totallen-headlen, title))
