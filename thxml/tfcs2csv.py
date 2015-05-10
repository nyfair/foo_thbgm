# Convert TFCS file to csv file
import csv, os, struct, sys, zlib

err = lambda msg: sys.exit('invalid tfcs file: %s' % msg)

if len(sys.argv) != 3:
	sys.exit('usage: python THxxBGM.py <TFCS csv file> <output csv file>')

fmt = '=5sII'
size = struct.calcsize(fmt)
fsize = os.path.getsize(sys.argv[1])
if fsize < size:
	err('files is too small')

data = []
with open(sys.argv[1],'rb') as tfcsfile:
	magic, tfcslen, csvlen = struct.unpack(fmt, tfcsfile.read(size))
	if magic != b'TFCS\x00' or tfcslen != fsize - size:
		err('wrong header')
	raw = zlib.decompress(tfcsfile.read(tfcslen))
	if len(raw) != csvlen:
		err('decompress error')

	nrow, = struct.unpack('I', raw[:4])
	pos = 4
	for i in range(nrow):
		row = []
		ncol, = struct.unpack('I', raw[pos:pos+4])
		pos += 4
		for j in range(ncol):
			strlen, = struct.unpack('I', raw[pos:pos+4])
			pos += 4
			row.append(raw[pos:pos+strlen].decode('shift_jis'))
			pos += strlen
		data.append(row)

with open(sys.argv[2],'w', encoding='utf_8_sig', newline='') as csvfile:
	writer = csv.writer(csvfile)
	writer.writerows(data)
