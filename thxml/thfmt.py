import glob, struct

format = '16siiii20s'
for file in glob.glob('*.fmt'):
	with open(file, 'rb') as thbgm:
		while True:
			bgm = thbgm.read(52)
			if len(bgm) < 52:
				thbgm.close()
				break
			title, offset, x1, headlen, totallen, x2 = struct.unpack(format, bgm)
			print('<bgm pos="%s,%s,%s">%s</bgm>' % (offset, headlen, totallen-headlen, title))
