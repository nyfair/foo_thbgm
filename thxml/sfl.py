#dump sfl loop info
import glob, struct

for file in glob.glob('*.sfl'):
	with open(file, 'rb') as thbgm:
		title = file[0 : file.rindex('.')]
		riff = thbgm.read(4)
		thbgm.seek(8)
		sfpl = thbgm.read(4)
		if riff != b'RIFF' or sfpl != b'SFPL':
			continue
		thbgm.seek(28)
		headlen = struct.unpack('i', thbgm.read(4))[0]
		thbgm.seek(72)
		looplen = struct.unpack('i', thbgm.read(4))[0]
		print('<bgm pos="0,%s,%s">%s</bgm>' % (headlen, looplen, title))
