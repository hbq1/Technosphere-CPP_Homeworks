import struct
import sys
import argparse

file_name = None
parser = argparse.ArgumentParser(description='Utility for reading docids file')
parser.add_argument('-f', default='test', type=str, help='Name of file'\
	' for analysing')
parser.add_argument('--endian', default='big', type=str,
	help='Set bigendian or little for numbers in file. "--endian big"'\
	' or "--endian little"')
args = parser.parse_args()
file_name = args.f
endian = args.endian

f = open(file_name, 'rb')
data = '1'
while data:
	data = f.read(8)
	if not data: break;
	if endian == 'big':
		num = struct.unpack('>Q', data)[0]
	else:
		num = struct.unpack('<Q', data)[0]
	sys.stdout.write('docid: {}, words:\n'.format(num))
	data = f.read(4)
	if endian == 'big':
		cnt = struct.unpack('>I', data)[0]
	else:
		cnt = struct.unpack('<I', data)[0]
	word_ids = list()
	print 'cnt: {}'.format(cnt)
	for i in xrange(cnt):
		data = f.read(8)
		if endian == 'big':
			word_ids.append(struct.unpack('>Q', data)[0])
		else:
			word_ids.append(struct.unpack('<Q', data)[0])
		if len(word_ids) > 10000:
			for w in word_ids:
				sys.stdout.write('{}; '.format(w))
			word_ids = list()
	if word_ids:
		for w in word_ids:
			sys.stdout.write('{}; '.format(w))
		word_ids = list()
	print '\n'