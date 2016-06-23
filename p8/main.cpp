#include <algorithm>
#include <vector>
#include <iostream>
#include <cstdlib>

const int MAX_SIZE = 1000*1000;

void parallel_qsort(std::vector<int>& a, int l, int r)
{
    if (r-l < 2)
        return;
    int m = a[(l+r-1)/2];//a[l+rand()%(r-l)];
    int pl = l, pr = r-1;
    do {
        while (a[pl] < m) ++pl;
        while (a[pr] > m) --pr;
        if (pl <= pr) 
            std::swap(a[pl++], a[pr--]);
    } while (pl<=pr);
    //parallel_qsort(a, pl, r);
    //parallel_qsort(a, l, pr);
    #pragma omp parallel sections num_threads(2)
        {
            #pragma omp section 
            {
                parallel_qsort(a, pl, r);
            }
            #pragma omp section
            {
                parallel_qsort(a, l, pr+1);
            }
        }
}

bool inc_array(std::vector<int> &a) 
{
    for(auto i=1; i<a.size(); ++i)
        if (a[i]<a[i-1])
            return 0;
    return 1;
}

int main(int argc, char** argv)
{
    int n_tests;
    std::vector<int> a;
    
    std::cout << "Enter number if tests: ";
    std::cin >> n_tests;

    srand(100);
    while(n_tests--) {
        int n = rand()%MAX_SIZE+1;
        a.resize(n);
        for (auto i = 0; i < n; ++i) {
            a[i] = rand()%1000;
            //std::cout << a[i] << ' ';
        }
        //std::cout << std::endl;
        parallel_qsort(a, 0, n);
        if (!inc_array(a)) {
            std::cout << "not passed\n";
            //for (auto i = 1; i < n; ++i) 
            //    std::cout << a[i] << ' ';
            //std::cout << std::endl << std::endl;
        }
    }

    return 0;
}