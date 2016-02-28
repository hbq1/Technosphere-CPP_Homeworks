#include "allocator.h"

Pointer Allocator::alloc(size_t N) 
{
	auto it_free_part = std::find_if(free_parts.begin(), free_parts.end(), 
		[N](std::pair<size_t, size_t> p){ return p.second>=N;});
	/*if (it_free_part == free_parts.end()) {
		//this->defrag();
		it_free_part = std::find_if(free_parts.begin(), free_parts.end(), 
			[N](std::pair<size_t, size_t> p){ return p.second>=N;});
	}*/
	if (it_free_part == free_parts.end()) {
		throw AllocError(AllocErrorType::NoMemory, "not enought memory");
	}
	if (it_free_part->second == N) {
		free_parts.erase(it_free_part);
	} 
	int id = get_unique_id();
	allocated_memory.push_back(PointerInfo_t(it_free_part->first, N, id));
	it_free_part->first += N;
	it_free_part->second -= N;
	return Pointer(this, id);
}

void Allocator::realloc(Pointer &p, size_t N)
{
	if (N == 0 && p.pointer_unique_id == -1) {
		return;
	}
	if (N == 0) {
		free(p);
		return;
	}
	if (p.pointer_unique_id == -1) {
		p = this->alloc(N);
		return;
	}
	
	auto p_info = get_pointer_info(p);
	if (!p_info->is_valid || p_info->size == N) 
		return;
	char* cbase = reinterpret_cast<char*>(base);

	if (N>p_info->size) {
		auto it_right_free_part = std::find_if(free_parts.begin(), free_parts.end(), 
			[p_info](std::pair<size_t, size_t> part){ 
				return part.first==p_info->base_shift+p_info->size;
			});
		if (it_right_free_part != free_parts.end()&& it_right_free_part->second>=N-p_info->size) {
			it_right_free_part->first += N-p_info->size;
			it_right_free_part->second -= N-p_info->size;
			p_info->size = N;
			return;
		}
		
	}
	if (N<p_info->size) {
		auto it_right_free_part = std::find_if(free_parts.begin(), free_parts.end(), 
			[p_info](std::pair<size_t, size_t> part){ 
				return part.first==p_info->base_shift+p_info->size;
			});
		it_right_free_part->first += N-p_info->size;
		it_right_free_part->second -= N-p_info->size;
		p_info->size = N;
		return;
	} 
	
	this->free(p);
	auto save_id = p_info->pointer_unique_id;
	p_info->pointer_unique_id = -1;
	auto new_p = this->alloc(N);
	auto new_p_info = get_pointer_info(new_p);
	for(auto i=p_info->base_shift; i<p_info->base_shift+p_info->size; i++) {
		//std::cout <<i<<" "<< new_p_info.base_shift+i-p_info.base_shift<<std::endl;
		cbase[new_p_info->base_shift+i-p_info->base_shift] = cbase[i];
	}
	new_p_info->pointer_unique_id = save_id;
	//std::swap(new_p_info->pointer_unique_id,p_info->pointer_unique_id);//p_info->pointer_unique_id;
	//new_p_info->is_valid = true;
	//std::cout <<p_info->pointer_unique_id<<std::endl;
	//std::cout <<new_p_info.pointer_unique_id<<std::endl;
	
	//std::cout<<new_p_info.base_shift<<std::endl;
	//p_info.base_shift = new_p_info.base_shift;
	//p_info.size = N;
	//p_info.is_valid = true;
	//new_p_info.is_valid = false;
	
}

void Allocator::free(Pointer &p)
{
	if (p.pointer_unique_id == -1) {
		return;
	}
	auto p_info = get_pointer_info(p);
	//std::cout <<p_info->pointer_unique_id<<std::endl;
	if (!p_info->is_valid) {
		std::cout <<" SHIT " <<std::endl;
		throw AllocError(AllocErrorType::InvalidFree, "double free maybe..");
	}
	auto it_left_free_part = std::find_if(free_parts.begin(), free_parts.end(), 
			[&](std::pair<size_t, size_t> part){ return part.first+part.second==p_info->base_shift;});
	if (it_left_free_part == free_parts.end()) {
		free_parts.push_back(std::make_pair(p_info->base_shift, p_info->size));
	} else {
		it_left_free_part->second += p_info->size;
	}
	p_info->is_valid = false;
}

void Allocator::defrag()
{
	std::vector<PointerInfo_t> buffer;
	for(auto& p: allocated_memory)
		if (p.is_valid)
			buffer.push_back(p);
	std::sort(buffer.begin(), buffer.end(), [](PointerInfo_t p1, PointerInfo_t p2) {return p1.base_shift<p2.base_shift;});
	allocated_memory.swap(buffer);

	size_t last_free_m = 0;
	char* cbase = reinterpret_cast<char*>(base);
	for(auto i=0; i<allocated_memory.size(); ++i) {
		auto& p = allocated_memory[i];
		auto buf = last_free_m;
		for(auto j=p.base_shift; j<p.base_shift+p.size; j++)
			cbase[last_free_m++] = cbase[j];
		p.base_shift = buf;
	}
	free_parts.clear();
	free_parts.push_back(std::make_pair(last_free_m, size - last_free_m));
}

void *Allocator::get(Pointer& p) 
{
	auto p_info = get_pointer_info(p);
	if (!p_info->is_valid) {
		return nullptr;
	}
	return (char*)(base)+sizeof(char)*p_info->base_shift;
}

std::vector<PointerInfo_t>::iterator Allocator::get_pointer_info(Pointer& p) 
    {
        std::vector<PointerInfo_t>::iterator p_info = std::find_if(allocated_memory.begin(), allocated_memory.end(), 
            [p](PointerInfo_t& p_infox){ return p_infox.pointer_unique_id == p.pointer_unique_id;});
        return p_info;
    }


void *Pointer::get() 
{ 
	return allocator->get(*this); 
} 

/*
Pointer::~Pointer()
{
	if (pointer_unique_id != -1) {
		auto& p_info = allocator->get_pointer_info(*this);
		if (p_info.is_valid) {
			p_info.cnt_pointers--;
			if (p_info.cnt_pointers == 0)
				free(*this);
		}
	}
}

Pointer::Pointer(Pointer& p) {
 	if (p.pointer_unique_id != -1) {
		auto& p_info = allocator->get_pointer_info(p);
		if (p_info.is_valid) 
			p_info.cnt_pointers++;
	}
	allocator = p.allocator; 
	pointer_unique_id = p.pointer_unique_id);
}

Pointer Pointer::operator=(Pointer& p) {
 	if (p.pointer_unique_id != -1) {
		auto& p_info = allocator->get_pointer_info(p);
		if (p_info.is_valid) 
			p_info.cnt_pointers++;
	}
	return Pointer(p.allocator, p.pointer_unique_id);
}*/

/*
Pointer Pointer::operator=(Pointer& p) {
	
}*/