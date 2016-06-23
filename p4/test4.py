import sys
import random
import math
import struct
import argparse
import subprocess
import os
import time

def get_next_doc_id(old_doc_id):
	global doc_id_step
	return old_doc_id + random.randint(100, 500)

def generate_words_from_to(start, end, cnt, mult, plus):
	result = set()
	while len(result) < cnt:
		result.add(random.randint(start, end) * mult + plus)
	return sorted(result)

docid_cnt = 10
words_cnt = 1000
min_words_per_doc = 3
min_need_file_size = 10000
words_deviation = 0.2
word_mult = 5
word_plus = 3
debug = False
file_name = 'test'
execute_file = None
not_regenerate = False

parser = argparse.ArgumentParser(description='Generaring file '\
	'with docids and wordids')
parser.add_argument('--debug', action='store_true', default=False,
	help='Set if you want to see debug messages')
parser.add_argument('-f', default='test', type=str,
	help='File name to store test document')
parser.add_argument('--seed', default=None,
	type=int, help='Seed for random generator')
parser.add_argument('--wp', default=3, type=int,
	help='This value will be added to each word id while its generation')
parser.add_argument('--wm', default=5, type=int,
	help='This value will be used for multiplying word id while its'\
	'generation')
parser.add_argument('--wd', default=0.2, type=float,
	help='This value is used for spreading words count per one document')
parser.add_argument('--file_sz', default=100000,
	type=long, help='It is desirable size of result file in bytes')
parser.add_argument('--min_wpd', default=3, type=int,
	help='It is minimal words count per one document')
parser.add_argument('--wcnt', default=1000, type=int,
	help='This is count of unique word ids')
parser.add_argument('--dcnt', default=100, type=int,
	help='This is count of documents')
parser.add_argument('--execf', default=None, type=str,
	help='Path to executable file')
parser.add_argument('--not_regener', action='store_true', default=False,
	help='Set if you do not want to regenerate source file and want to'\
	' use already generated')
parser.add_argument('--endian', default='big', type=str,
	help='Set bigendian or little for numbers in file. "--endian big"'\
	' or "--endian little"')
args = parser.parse_args()
endian = args.endian

if args.dcnt <= 0:
	print 'dcnt must be positive integer'
	sys.exit(0)
docid_cnt = args.dcnt
if args.wcnt <= 0:
	print 'wcnt must be positive integer'
	sys.exit(0)
words_cnt = args.wcnt
if args.min_wpd <= 0:
	print 'min_wpd must be positive integer'
	sys.exit(0)
min_words_per_doc = args.min_wpd
if args.file_sz <= 0:
	print 'file_sz must be positive integer'
	sys.exit(0)
min_need_file_size = args.file_sz
if args.wd < 0:
	print 'wd must be not negative number'
	sys.exit(0)
words_deviation = args.wd
if args.wm <= 0:
	print 'wm must be positive integer'
	sys.exit(0)
word_mult = args.wm
if args.wp < 0:
	print 'wp must be not negative integer'
word_plus = args.wp
debug = args.debug
if args.seed:
	random.seed(args.seed)
else:
	random.seed()
if len(args.f) == 0:
	print 'file name must be not empty string'
	sys.exit(0)
file_name = args.f
if args.execf:
	execute_file = args.execf
not_regenerate = args.not_regener

if not not_regenerate:
	words_max_in_part = 100000
	max_file_size = docid_cnt * (8 + 4 + words_cnt * 8)
	docid_start = random.randint(0, 4000)

	if min_words_per_doc > words_cnt:
		print 'min_words_per_doc = {} can\'t be bigger than words_cnt = {}'.\
			format(min_words_per_doc, words_cnt)
		sys.exit(0)

	if min_need_file_size > max_file_size:
		print 'max_file_size with this params is {} bytes, but you want {} bytes'.\
			format(max_file_size, min_need_file_size)
		sys.exit(0)

	words_average = max(min_need_file_size - docid_cnt * (8 + 4), 0)
	words_average = max(max(words_average / (docid_cnt * 8), 3), min_words_per_doc)
	words_average_start = words_average - int(words_average * words_deviation)
	words_average_end = words_average + int(words_average * words_deviation)

	if debug:
		print 'max_file_size: {}'.format(max_file_size)
		print 'words_average: {}'.format(words_average)
		print 'words_average start: {}'.format(words_average_start)
		print 'words_average end: {}'.format(words_average_end)


	f = open(file_name, 'wb')
	cur_docid = docid_start

	for i in xrange(docid_cnt):
		doc_words = set()
		words_per_doc = max(random.randint(words_average_start, words_average_end),
			min_words_per_doc)
		words_parts = int(math.ceil(words_per_doc * 1.0 / words_max_in_part))
		words_per_part = int(math.ceil(words_per_doc * 1.0 / words_parts))
		source_word_per_part = int(math.ceil(words_cnt * 1.0 / words_parts))
		if endian == 'big':
			f.write(struct.pack('>QI', cur_docid, words_per_doc))
		else:
			f.write(struct.pack('<QI', cur_docid, words_per_doc))
		cur_docid = get_next_doc_id(cur_docid)
		result_cnt = 0
		if debug:
			print '\ncur_docid: {}\nwords_per_doc: {}\nwords_parts: {}\n'\
				'words_per_part: {}\nsource_word_per_part: {}'.format(
					cur_docid, words_per_doc, words_parts, words_per_part,
					source_word_per_part
				)
		for j in xrange(words_parts):
			start = j * source_word_per_part
			end = min(words_cnt, start + source_word_per_part)
			cnt = words_per_part
			if result_cnt + cnt > words_per_doc:
				cnt = words_per_doc - result_cnt
			res = generate_words_from_to(start + 1, end, cnt,
				word_mult, word_plus)
			result_cnt += cnt
			if debug:
				print 'start: {}, end: {}\n'\
					'iteration: {}, res.len: {}, res: {}'.format(start + 1, end,
						j, len(res), res
					)
			for w in res:
				if endian == 'big':
					f.write(struct.pack('>Q', w))
				else:	
					f.write(struct.pack('<Q', w))
	f.close()
#end of not_regenerate
if execute_file:
	start_time = time.time()
	p = subprocess.Popen([execute_file, file_name])
	resource_usage = os.wait4(p.pid, 0)[2]
	end_time = time.time()
	print 'execution_time: {} secs\nmaximum memory usage: {} bytes'.format(
		end_time - start_time, resource_usage.ru_maxrss
	)




