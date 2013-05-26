import glob

for file in glob.glob('*.sli'):
	with open(file) as thbgm:
		title = file[0 : file.rindex('.')]
		headlen = 0
		looplen = 0
		for line in thbgm:
			parts = line.split('=', 1)
			if parts[0].lower() == 'loopstart':
				headlen = int(parts[1])
			elif parts[0].lower() == 'looplength':
				looplen = int(parts[1])
		print('<bgm pos="0,%s,%s">%s</bgm>' % (headlen, looplen, title))
