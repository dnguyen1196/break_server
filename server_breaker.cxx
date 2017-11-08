// Lab 2: Server breaker

//	c++ -std=c++11 -pthread -O2 server_breaker.cxx ee193_utils.cxx 

// We will always allocate a vector of this many lines, and then choose which
// ones to actually use on any run.
static const int MAX_LINES = 10000000;

static const int N_ITERATIONS=5;	// Top-level iterations...

// Fixed by the CPU microarchitecture.
static const int BYTES_PER_LINE=64;

#include <iostream>
#include <thread>
#include <vector>
#include <random> 
#include <algorithm>
#include <mutex>
#include "ee193_utils.hxx"
using namespace std;

// This is all of the memory lines that we can use.
// We just allocate the biggest version we might ever need.
static unsigned char g_mem[MAX_LINES * BYTES_PER_LINE];

static void compute_thread (int me, const vector<int> &indices, int n_stores, int n_loads, int n_line_accesses);
static void pick_unique_lines (vector<int> &indices, int n_lines);
static void run (int n_lines, int n_stores, int n_loads, int n_threads, int n_line_accesses);

static mutex mut;	// For debug printing.

////////////////////////////////////////////////////////////////////////////////
// Program main
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv) 
{
    // Run through all of the parameter combinations that the homework asks for.
    // Parameters are n_lines, n_stores, n_loads, n_threads.
    for (int n_lines=100; n_lines<=100000; n_lines *= 10)
	for (int n_threads=1; n_threads<=16; n_threads *= 2)
	   run (n_lines, 0, 2, n_threads, 204800000);

    for (int n_lines=1000; n_lines<=100000; n_lines *= 10)
	for (int n_threads=1; n_threads<=16; n_threads *= 2)
	    run (n_lines, 4, 1, n_threads, 102400000);
}

// This function takes care of picking lines, timing how long your code takes,
// and instantiating the threads.
static void run (int n_lines, int n_stores, int n_loads, int n_threads,
		 int n_total_line_accesses) {
    vector<int> indices;	// Indices into g_mem[] of our chosen lines
    pick_unique_lines (indices, n_lines);

    // Print a summary of the upcoming run's parameters.
    LOG ("\nPicking "<<n_lines<<" lines from a total of "<<MAX_LINES);
    int n_loops = n_total_line_accesses/n_lines;
    LOG (n_threads<<" threads, each doing "<<n_loops<<" loops of "
	 <<n_lines<<" lines ("<< n_stores << " stores+"<< n_loads<<" loads)");

    // The outer loop of iterations. Each iteration is really an entire
    // run; we do it multiple times to see any run-to-run variability.
    for (int its=0; its<N_ITERATIONS; ++its) {
	// The main loop. We start the timer, call compute_thread() to do the
	// work, end the timer, and write out statistics.
	auto start = start_time();	// Time the loads and stores.

	// Instantiate all of the threads.
	vector<thread> threads;
	for (int i=0; i<n_threads; ++i)
	    threads.push_back (thread (compute_thread,i,ref(indices),
			       n_stores,n_loads,n_total_line_accesses));

	for (auto &th:threads)	// Wait for all threads to finish.
	    th.join();

	// Collect & print timing statistics.
	long int time = delta_usec (start);
	LOG ("Execution took "<<(time/1000.0)<<"msec");
    }
}

// The function that each thread executes.
// You must write this one yourself.
// For details on what it does, see the homework assignment .pdf.
// The parameters:
//	me: my thread id. Threads are numbered 0, 1, etc.
//	indices: a vector of line numbers. The lines are numbered [0,MAX_LINES)
//		They are used to index into g_mem[]. However, note that g_mem[]
//		is a vector of bytes, not of 64B lines. Thus, the actual index
//		into g_mem[] for line #i would be i*BYTES_PER_LINE.
//	n_stores, n_loads: the number of times that a particular loop stores
//		or loads into its memory location.
static void compute_thread (int me, const vector<int> &indices,
			    int n_stores, int n_loads, int n_line_accesses) {

    unsigned char data_val = 0;

    // We want to keep the same total number of line_accesses
    int n_lines = indices.size(), n_loops= n_line_accesses/n_lines;
    // Each thread gets the same number of line_accesses/ but with different offset?
    bool found_error = false;

    // main loop: loop through all lines as many times as requested.
    for (int loop=0; loop<n_loops; ++loop) {
		// For each loop, do some repeated stores

		for (int s=0; s<n_stores; ++s) {
		    // Go through all of the lines, and first do the stores.

            for (int i = 0; i < indices.size(); i++) {
                int line_num = indices[i]; // Get the line number to store
            	// Store into g_mem, offset by me indices
            	// Since me < 16 -> 
            	// Store 0 into the location on the first line_num, 1 into 
            	// our desired location on the second line_num etc
            	// Have to cast to character because we are accessing by character
                g_mem[line_num*BYTES_PER_LINE + me] = (unsigned char)(i + n_lines*loop);
			}
		}

		// Go through all of the lines again, and read the data back.
		// This should be very similar to the check above
		for (int l=0; l<n_loads; ++l) {
           	for (int i = 0; i < indices.size(); i++) {
                int line_num = indices[i]; // Get the line number to store
            	// Store into g_mem, offset by me indices
            	// Since me < 16 -> 
            	// Store 0 into the location on the first line_num, 1 into 
            	// our desired location on the second line_num etc
                if(n_stores != 0 && !found_error 
                	&& g_mem[line_num*BYTES_PER_LINE + me] != (unsigned char)(i + n_lines*loop) ){
                	cout << "Error" << endl;
                	found_error = true;
                }
			}
		}
    }
}

// Randomly choose 'n_lines' lines. Return them implicitly via 'indices'.
// Specifically, we return a vector with the integers [0,MAX_LINES) randomly
// permuted. These will serve (after a small bit of munging) as indices into
// g_mem[].
static void pick_unique_lines (vector<int> &indices, int n_lines) {
    // Initialize it to integer values in [0,N_BUCKETS).
    default_random_engine gen;
    uniform_int_distribution<int> dist(0,MAX_LINES-1);

    //LOG ("Picking rand in [0,"<<MAX_LINES-1<<"]");

    indices.clear();
    // Random numbers may pick the same number multiple times. So we are a
    // bit tricky; we do two loops, and remove duplicates on the inner loop.
    // Yes, there are more efficient ways to do this.
    while (indices.size() < n_lines) {
	// First fill up indices with the correct number of lines.
	while (indices.size() < n_lines) {
	    int line = dist(gen);
	    indices.push_back (line);
        }

	// However, we may have put in some duplicates.
	// So, sort-uniquify-merge on our indices.
	sort (indices.begin(), indices.end());
	indices.erase (unique (indices.begin(), indices.end()), indices.end());
    }
    //cout << "Lines: {";
    //for (int ln : indices) cout <<ln<<" ";
    //cout << "}\n";
}
