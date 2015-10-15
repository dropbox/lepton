#include "memory.hh"

void* operator new (size_t size) throw(std::bad_alloc){
 void* ptr = custom_malloc(size); 
 if (ptr == 0) {// did malloc succeed?
     assert(false && "Out of memory error");
     exit(4); // ran out of memory
 }
 return ptr;
}

void* operator new[] (size_t size) throw(std::bad_alloc){
 void* ptr = custom_malloc(size); 
 if (ptr == 0) {// did malloc succeed?
     assert(false && "Out of memory error");
     exit(4); // ran out of memory
 }
 return ptr;
}

void operator delete (void* ptr) throw(){
    custom_free(ptr);
}
void operator delete[] (void* ptr) throw(){
    custom_free(ptr);
}
