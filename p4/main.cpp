/*
info..

Algorithm:
for all pairs in input:
	1) read pair (doc_id, word_id)
	2) save to buffer (word_id, doc_id)
		* if buffer is full, sort it's pairs and flush to temp file
flush buffer
merge sort to one file
	count (word_id, cnt_docs) to know offset
add offsets

System: MAC OS

*/

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <iostream>

//#define DEBUG_MODE

const uint32_t DEFAULT_BUFSIZE = 2*1000*1000;
const uint32_t SIZE_WORDID = 8;
const uint32_t SIZE_OFFSET = 4;

class IndexMerger {
	uint32_t bufsize;
	std::pair<uint64_t, uint64_t> *buffer; 
	std::map<uint64_t, uint32_t> word_frequency;

	std::string tmp_file_prefix;
	uint32_t cnt_tmp_files;
public:
	IndexMerger(uint32_t bufsize_ = DEFAULT_BUFSIZE, const char* tmp_file_prefix_="temp_"): bufsize(bufsize_)
	{
		cnt_tmp_files = 0;
		tmp_file_prefix = std::string(tmp_file_prefix_);
		buffer = new std::pair<uint64_t, uint64_t>[bufsize];
	}

	~IndexMerger() 
	{
		//TODO: delete tempfiles
		delete [] buffer;
	}

	void flush_buffer(uint32_t cnt_elems = -1)
	{
		if (cnt_elems == -1)
			cnt_elems = bufsize;
		//sort pairs in buffer
		std::sort(buffer, buffer+cnt_elems, 
				  [](std::pair<uint64_t, uint64_t>& a, std::pair<uint64_t, uint64_t>& b) 
				  	 { return a.first<b.first || (a.first==b.first && a.second < b.second); }
				  );
		//save pairs to tempfile
		std::string tmp_file_path = tmp_file_prefix + std::to_string(cnt_tmp_files);
		FILE* tmp_file = fopen(tmp_file_path.c_str(), "wb");
		for(auto i=0; i<cnt_elems; ++i) {
			fwrite(&(buffer[i].first),  SIZE_WORDID, 1, tmp_file);
			fwrite(&(buffer[i].second), SIZE_WORDID, 1, tmp_file);
		}
		fclose(tmp_file);

		++cnt_tmp_files;
	}

	void split(const char *src_path)
	{
		word_frequency.clear();
		uint64_t doc_id, word_id;
		uint32_t n_words;
		uint32_t buffer_cur_pos = 0;

		FILE *src_file = fopen(src_path, "rb");
		while(fread(&doc_id, SIZE_WORDID, 1, src_file) == 1) {
#ifdef DEBUG_MODE
			std::cout << "DOCID: " << doc_id << std::endl;
#endif
			fread(&n_words, SIZE_OFFSET, 1, src_file);
#ifdef DEBUG_MODE
			std::cout << "N_WORDS: " << n_words << std::endl;
#endif
			for(auto i=0; i<n_words; ++i) {
				fread(&word_id, SIZE_WORDID, 1, src_file);
#ifdef DEBUG_MODE				
				std::cout << "WORD_ID: " << word_id << " ";
#endif				
				if (word_frequency.count(word_id) == 0)
					word_frequency.insert(std::make_pair(word_id, 1u));
				else 
					++word_frequency[word_id];
				buffer[buffer_cur_pos++] = std::make_pair(word_id, doc_id);
				if (buffer_cur_pos == bufsize) {
					this->flush_buffer();
					buffer_cur_pos = 0;
				}
			}
#ifdef DEBUG_MODE
			std::cout << std::endl;
#endif
		}
		if (buffer_cur_pos > 0) 
			this->flush_buffer(buffer_cur_pos);

		fclose(src_file);
	}

	bool get_pair_from_file(FILE* file, std::pair<uint64_t, uint64_t>& res)
	{
		return (fread(&(res.first), SIZE_WORDID, 1, file) == 1 && 
			    fread(&(res.second), SIZE_WORDID, 1, file) == 1);
	}

	void merge(const char *dest_path)
	{
		std::vector<FILE*> tmp_files(cnt_tmp_files, nullptr);
		std::vector<std::pair<uint64_t, uint64_t>> pairs_buffer(cnt_tmp_files);
		std::vector<bool> is_active_buffer(cnt_tmp_files);
		FILE* result_file = fopen(dest_path, "wb");
		std::pair<uint64_t, uint64_t> cur_pair(-1,0), next_pair;
		uint64_t zero = 0;
		uint32_t cnt_active_buffers = 0;
		for(auto i = 0; i<cnt_tmp_files; ++i) {
			tmp_files[i] = fopen((tmp_file_prefix+std::to_string(i)).c_str(), "rb");
			is_active_buffer[i] = get_pair_from_file(tmp_files[i], pairs_buffer[i]);
			//std::cout << pairs_buffer[i].first;
			if (is_active_buffer[i]) 
				++cnt_active_buffers;
			else
				fclose(tmp_files[i]);
		}
		// lets count offsets
		uint64_t offset = SIZE_WORDID*2*(1+word_frequency.size());
		for(auto i=word_frequency.begin(); i!=word_frequency.end(); i++) {
			fwrite(&(i->first), SIZE_WORDID, 1, result_file);
			fwrite(&(offset), SIZE_WORDID, 1, result_file);
#ifdef DEBUG_MODE
			std::cout << i->first << " " << offset << std::endl;
#endif
			offset += (i->second)*SIZE_WORDID+SIZE_OFFSET;
		}
		fwrite(&(zero), SIZE_WORDID, 1, result_file);
		fwrite(&(zero), SIZE_WORDID, 1, result_file);
#ifdef DEBUG_MODE
		std::cout << "0 0" << std::endl;
#endif
		// ok
		// lets merge temp files
		int p;
		while (cnt_active_buffers > 0) {
			p = -1;
			//get min_pair
			for(auto i=0; i<cnt_tmp_files; ++i)
				if (is_active_buffer[i] && (p==-1 || pairs_buffer[i].first < pairs_buffer[p].first || (
					pairs_buffer[i].first==pairs_buffer[p].first && pairs_buffer[i].second < pairs_buffer[p].second))) 
					p = i;
			//check if next word has begun
			if (cur_pair.first != pairs_buffer[p].first) {
				fwrite(&(word_frequency[pairs_buffer[p].first]), SIZE_OFFSET, 1, result_file);
#ifdef DEBUG_MODE
				std::cout << std::endl << word_frequency[pairs_buffer[p].first] << " ";
#endif
			}
			fwrite(&(pairs_buffer[p].second), SIZE_WORDID, 1, result_file);
#ifdef DEBUG_MODE
			std::cout << pairs_buffer[p].second << " ";
#endif
			cur_pair = pairs_buffer[p];
			//read next pair
			is_active_buffer[p] = get_pair_from_file(tmp_files[p], pairs_buffer[p]);
			if (!is_active_buffer[p]) {
				fclose(tmp_files[p]);
				--cnt_active_buffers;
			}
		}
		fclose(result_file);
	}

	void print_index(const char* path_to_index)
	{
		FILE* src_file = fopen(path_to_index, "rb");
		std::map<uint64_t, uint64_t> words_offsets;
		uint64_t t[2];
		t[0] = t[1] = -1;
		while (1) {
			fread(t, 2*SIZE_WORDID, 1, src_file);
			std::cout << t[0] << " " << t[1] << std::endl;
			if (t[0] != 0 || t[1] != 0)
				words_offsets.insert(std::make_pair(t[0], t[1]));
			else
				break;
		}
		for (auto i=words_offsets.begin(); i!=words_offsets.end(); i++) {
			fseek(src_file, i->second, SEEK_SET);
			uint32_t cnt_elems;
			uint64_t buf;
			fread(&cnt_elems, SIZE_OFFSET, 1, src_file);
			std::cout << i->first << ": " << cnt_elems << std::endl;
			for (auto j=0; j<cnt_elems; ++j) {
				fread(&buf, SIZE_WORDID, 1, src_file);
				std::cout << buf << " ";
			}
			std::cout << std::endl;
		}
		fclose(src_file);
	}

};

int main(int argc, char **argv)
{
	if (argc != 3) {
		std::cout << "Args:\n 1 - path to input file\n 2 - path to output file\n";
		return 1;
	}
	IndexMerger IM = IndexMerger();
	IM.split(argv[1]);
	IM.merge(argv[2]);
	//IM.print_index(argv[2]);
	return 0;
}