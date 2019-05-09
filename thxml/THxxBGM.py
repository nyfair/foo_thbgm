# Convert THxxBGM file to thxml file
import sys, codecs
from xml.etree.cElementTree import *

if len(sys.argv) < 2:
	sys.exit('usage: python THxxBGM.py <THxxBGM file> {<thxml file>}')

file = open(sys.argv[1], mode='r', encoding='shift_jis')
bgmlist = []
for line in file:
	if line[0] == '#':
		continue
	elif line[0] == '@':
		x = line.split(',')
		album = x[1]
		path = x[0][1:]
	else:
		bgmlist.append(line.split(','))

root = Element('thxml')
_album = Element('album')
_album.text = album
_albumartist = Element('albumartist')
_albumartist.text = '上海アリス幻樂団'
_path = Element('path')
_path.text = path
_bgmlist = Element('bgmlist')

for bgm in bgmlist:
	_bgm = Element('bgm')
	if bgm[0][0] == '%':
		_bgm.set('file', bgm[0][1:])
		del(bgm[0])
	_bgm.set('pos', str(int('0x'+bgm[0], 16)) + ',' + 
		str(int('0x'+bgm[1], 16)) + ',' + str(int('0x'+bgm[2], 16)))
	_bgm.text = bgm[3]
	_bgmlist.append(_bgm)

root.append(_album)
root.append(_path)
root.append(_bgmlist)
root.append(_albumartist)

# Get pretty look
def indent(elem, level=0):
	i = "\n" + level*"\t"
	if len(elem):
		if not elem.text or not elem.text.strip():
			elem.text = i + "\t"
		for e in elem:
			indent(e, level+1)
		if not e.tail or not e.tail.strip():
			e.tail = i
	if level and (not elem.tail or not elem.tail.strip()):
		elem.tail = i
	return elem

if len(sys.argv) > 2:
	thxml = open(sys.argv[2] + '.thxml', mode='w', encoding='utf_8_sig')
else:
	thxml = open(sys.argv[1].split('.')[0] + '.thxml', \
	mode='w', encoding='utf_8_sig')
thxml.write('<?xml version="1.0" encoding="utf-8"?>')
thxml.write('<!DOCTYPE thxml "nyfair_thxml.dtd">')
thxml.write("\n\n" + tostring(indent(root), encoding = 'utf_8', \
	method = 'html').decode('utf_8') + '\n')
