#include <stdexcept>
#include <string>
#include <vector>
#include <list>
#include <algorithm>

#include <iostream>

enum class AllocErrorType {
    InvalidFree,
    NoMemory,
};

class AllocError: std::runtime_error {
private:
    AllocErrorType type;

public:
    AllocError(AllocErrorType _type, std::string message):
            runtime_error(message),
            type(_type)
    {}

    AllocErrorType getType() const { return type; }
};

class Allocator;

class Pointer {
    Allocator* allocator;
public:
    int pointer_unique_id;
    Pointer(): allocator(nullptr), pointer_unique_id(-1) {}
    Pointer(Allocator* a, int pid): allocator(a), pointer_unique_id(pid){}
    //~Pointer();
    //Pointer(Pointer& p);
    //Pointer operator=(Pointer& p);
    void *get();
};

struct PointerInfo_t {
    size_t base_shift;
    size_t size;
    int pointer_unique_id;
    bool is_valid;
    PointerInfo_t(size_t b_s, size_t s, int pid): base_shift(b_s), size(s), pointer_unique_id(pid), is_valid(true) {}
};

class Allocator {
    void *base;
    size_t size;
    int last_unique_id;
    std::vector<PointerInfo_t> allocated_memory;
    std::list<std::pair<size_t, size_t>> free_parts;
public:
    Allocator(void *_base, size_t _size):
        base(_base),
        size(_size)
    {
        free_parts.clear();
        allocated_memory.clear();
        free_parts.push_back(std::make_pair(0ul, _size));
        last_unique_id = 0;
    }
    
    Pointer alloc(size_t N);
    void realloc(Pointer &p, size_t N);
    void free(Pointer &p);

    void defrag();
    void dump() { 
        for(auto i=0; i<allocated_memory.size();++i)
            std::cout<<allocated_memory[i].pointer_unique_id<<": "<<allocated_memory[i].is_valid<<std::endl;
    }

    void *get(Pointer& p);
    size_t get_unique_id() { last_unique_id++; return last_unique_id-1;}
    std::vector<PointerInfo_t>::iterator get_pointer_info(Pointer& p) ;
};