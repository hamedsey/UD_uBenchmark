#include <iostream>
#include <immintrin.h>
#include <assert.h>
#include <byteswap.h>
#include <endian.h>
#include <x86intrin.h>

#if 0
/*
__m512i convert_to_network_byte_order(__m512i vec) {
  // Swap the byte order within each 64-bit element
  __m512i swapped = _mm512_shuffle_epi8(vec, _mm512_set_epi8(
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
  ));

  // Swap the order of 64-bit elements within each 128-bit lane
  __m512i swapped_lanes = _mm512_permutexvar_epi64(_mm512_set_epi64(3, 2, 1, 0, 7, 6, 5, 4), swapped);

  // Swap the order of 128-bit lanes within each 256-bit half
  __m512i swapped_halves = _mm512_permutex2var_epi64(swapped_lanes, _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0), _mm512_set_epi64(3, 2, 1, 0, 7, 6, 5, 4));

  // Swap the order of 256-bit halves within the 512-bit vector
  __m512i result = _mm512_permutex2var_epi64(swapped_halves, _mm512_set_epi64(3, 2, 1, 0, 7, 6, 5, 4), _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 0));

  return result;
}

__m512i host_to_network_avx512(__m512i data) {
  // Swap bytes within 64-bit elements
  __m512i swapped_bytes = _mm512_shuffle_epi8(data, _mm512_set_epi8(
      7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8,
      23, 22, 21, 20, 19, 18, 17, 16, 31, 30, 29, 28, 27, 26, 25, 24,
      39, 38, 37, 36, 35, 34, 33, 32, 47, 46, 45, 44, 43, 42, 41, 40,
      55, 54, 53, 52, 51, 50, 49, 48, 63, 62, 61, 60, 59, 58, 57, 56));

  // Swap adjacent 64-bit elements
  __m512i swapped_elements = _mm512_permutex2var_epi64(
      swapped_bytes, _mm512_set_epi64(1, 0, 3, 2, 5, 4, 7, 6),
      _mm512_set_epi64(0, 1, 2, 3, 4, 5, 6, 7));

  return swapped_elements;
}
*/
void countLeadingZerosAVX512(volatile char* array, int length) {

    for (int i = 0; i < length; i += 64) {
        // Load 64 bytes (512 bits) from the input array
        volatile __m512i values = _mm512_loadu_si512((const void *)(reinterpret_cast<volatile __m512i*>(array + i)));

        //__m512i networkVector = convert_to_network_byte_order(values);
        //__m512i network_vec = host_to_network_avx512(values);

        //__m512i values = _mm512_set_epi64(128, 64, 32, 16, 8, 4, 2, 1);
        // Convert the loaded bytes to 32-bit integers
        //__m512i wideValues = _mm512_cvtepi8_epi64(values);

        // Perform the leading zero operation on the wide values
        __m512i leadingZeros = _mm512_lzcnt_epi64(values);

        // Store the leading zeros count in an array for printing
        alignas(64) uint64_t result[8];
        _mm512_store_si512(reinterpret_cast<__m512i*>(result), leadingZeros);

        // Print the leading zeros count for each 32-bit integer
        std::cout << "Leading Zeros: ";
        for (int j = 0; j < 8; j++) {
            std::cout << result[j] << " ";
        }
        std::cout << std::endl;
    }
}

int main() {
    int length = 64;
    int size = 1;
    int numberOfQueues = length;
    //unsigned char array [16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};//"Hello, AVX-512! This is a Test.";
    alignas(64) volatile char *array = (volatile char *)calloc(numberOfQueues, size);
    /*
    for (int j = 0; j < numberOfQueues; ++j) {
        array[j] = 0x01;
        printf("%x \n",&((array)[j]));
    }
    */
            //what i want
            // 0  1  2  3  4  5  6  7     8  9 10 11 12 13 14 15   16 17 18 19 20 21 22 23
            //01 00 00 00 00 00 00 00    00 01 00 00 00 00 00 00   01 01 01 01 01 01 01 01
    
            //what actually happens
            //00 00 00 00 00 00 00 01    00 00 00 00 00 00 01 00   01 01 01 01 01 01 01 01
    
    array[0] = 0x00;
    array[1] = 0x00;
    array[2] = 0x00;
    array[3] = 0x00;
    array[4] = 0x00;
    array[5] = 0x00;
    array[6] = 0x00;
    array[7] = 0xFF;

/*
    qpn[2:0], addr[2:0]
    000 -> 111
    001 -> 110
    010 -> 101
    011 -> 100
    100 -> 011
    101 -> 010
    110 -> 001
    111 -> 000
*/

    array[8] = 0xFF;
    array[9] = 0x00;
    array[10] = 0x00;
    array[11] = 0x00;
    array[12] = 0x00;
    array[13] = 0x00;
    array[14] = 0x00;
    array[15] = 0x00;

    array[23] = 0xFF;
    array[24] = 0xFF;
    array[39] = 0xFF;
    array[40] = 0xFF;
    array[55] = 0xFF;
    array[56] = 0xFF;
    /*
    for (int j = 0; j < numberOfQueues; ++j) {
        //array[j] = 0x01;
        printf("%u ",array[j]);
    }
    printf("\n");
    */
    //assert(res->buf != NULL);

    countLeadingZerosAVX512(array, length);

    return 0;
}
#endif

static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#define MSG_SIZE 1
#define NUM_QUEUES 256
#define NUM_ITERATIONS 1000000

#define TSC_FREQUENCY ((double)2.2E9) 
inline unsigned long readTSC() { 
	_mm_mfence(); // wait for earlier memory instructions to retire 
	//_mm_lfence(); // block rdtsc from executing 
	unsigned long long tsc = __rdtsc(); 
	_mm_mfence();
	//_mm_lfence(); // block later instructions from executing 
	return tsc; 
}

int main() {

    uint64_t size = MSG_SIZE;
    uint64_t numberOfQueues = NUM_QUEUES;
    printf("number of queues = %d \n", numberOfQueues);

    uint64_t numAllocatedBytes;
    if(numberOfQueues % 64 == 0) numAllocatedBytes = numberOfQueues;
    else numAllocatedBytes = ((numberOfQueues/64) + 1)*64;
    alignas(64) volatile char *buf;                 // memory buffer pointer, used for
    buf = (volatile char *)calloc(numAllocatedBytes, size);
    assert(buf != NULL);

    //array fits in the L3 cache, and NIC is not involved.
    //entry in cache is stale and CPU has to go to memory.

    //(buf)[0] = 255;
    (buf)[0] = 255;
    

    //for(uint64_t t = 0; t < numAllocatedBytes; t++) printf("%x , %llu \n",&((buf)[t]), (buf)[t]);

    unsigned long long leadingZeros = 0;
    uint64_t connIndex = 0;
    alignas(64) uint64_t result[8] = {0,0,0,0,0,0,0,0};
    uint64_t value;
    uint64_t offset;
    uint64_t leadingZerosinValue;
    uint8_t resultIndexWorkFound;
    uint8_t j;

   __m512i sixtyfour = _mm512_setr_epi64(64, 64, 64, 64, 64, 64, 64, 64);
    uint64_t log2;

    unsigned long start_cycle = readTSC(); 


    for(uint64_t x = 0; x < NUM_ITERATIONS; x++) {
        bool foundWork = false;

        #if 1
        //7 cycles (0), 90 cycles (255) --- RR is 80 cycles
        for (uint64_t i = 0; i < NUM_QUEUES; i += 8) {
            unsigned long long value = htonll(*reinterpret_cast<volatile unsigned long long*>(buf + i));
            __asm__ __volatile__ ("lzcnt %1, %0" : "=r" (leadingZeros) : "r" (value):);
            
            //printf("i = %llu, leading count = %llu, value = %llu \n", i, leadingZeros, value);
            
            #if debugFPGA
            for(uint64_t y = 0; y < 8 ; y++) {
                printf("%x  ", (res.buf)[y]);
            }
            printf("\n \n");
            #endif

            //printf("connIndex = %llu \n", connIndex);

            if(leadingZeros != 64) {
                connIndex = i + (leadingZeros/8); //this value will change depending on leading zeros
                foundWork = true;
                break;
            }
        }
        //if(foundWork == true) break;

        #endif

        #if 0
        //7 cycles (0), 63 cycles (255)  --- RR is 65 cycles
        for (uint64_t i = 0; i < NUM_QUEUES; i += 64) {
            // Load 64 bytes (512 bits) from the input array
            //__m512i values = _mm512_loadu_si512(reinterpret_cast<__m512i*>(res.buf + i));

            //printf("i = %llu \n", i);
            volatile __m512i values = _mm512_loadu_si512((const void *)(reinterpret_cast<volatile __m512i*>(buf + i)));
            //volatile __m512i values = (reinterpret_cast<volatile __m512i*>(res.buf + i));

            // Perform the leading zero operation on the wide values
            __m512i leadingZeros = _mm512_lzcnt_epi64(values);


            // Store the leading zeros count in an array for printing
            _mm512_store_si512(reinterpret_cast<__m512i*>(result), leadingZeros);

            #if 0
            for (j = 0; j < 8; j++) {
                if(result[j] != 64) {
                    foundWork = true;
                    resultIndexWorkFound = result[j];
                    connIndex = (i + (8*j) + (resultIndexWorkFound/8)) ^ 7;
                    #if debugFPGA
                    printf("found work... i = %llu j = %llu, result[j] = %llu \n", i, j, resultIndexWorkFound);
                    #endif
                    break;
                }

            }
            if (foundWork == true) break;

            #endif


            #if 1
            // Perform element-wise inequality comparison
            __mmask8 mask = _mm512_cmpneq_epu64_mask(leadingZeros, sixtyfour);

            if(mask == 0) continue;
            else {

                // BSR instruction to find MSB position
                __asm__ __volatile__ ("bsr %1, %0" : "=r" (log2) : "r" (uint64_t(mask)));


                    //connIndex = i + (8*j) + (result[j]/8);
                    //conn = connections[connIndex];
                    //(buf)[connIndex^byteFlipMask];


                value = *(volatile uint64_t*)(buf+i+(log2*8));
                offset = i+(log2*8);

                //one way (27 cycles for 255, 13 cycles for 0, RR is 75 cycles)
                __asm__ __volatile__ ("lzcnt %1, %0" : "=r" (leadingZerosinValue) : "r" (value):);
                connIndex = (offset + ((leadingZerosinValue) >> 3)) ^ 7;

                //for(int i = 0; i < 8; i++) printf("  %x  ", buf[i]);
                //printf("\n");
                //printf("i = %llu, mask = %llu, log2 = %llu, offset = %llu, value = %llu, leadingZeros = %llu, connIndex = %llu \n", i, uint8_t(mask), log2, offset , value, leadingZerosinValue, connIndex);

                //another way (40 cycles for 255, 13 cycles for 0) --this is slower
                //connIndex = (offset + ((leadingZeros[log2]) >> 3)) ^ 7;
                //printf("i = %llu, mask = %llu, log2 = %llu, offset = %llu, value = %llu, leadingZeros = %llu, connIndex = %llu \n", i, uint8_t(mask), log2, offset , value, leadingZeros[log2], connIndex);

                break;
            }
            #endif

            //0x0000000000000000 leading zeros = 64
            //0xFFFFFFFFFFFFFFFF leading zeros = 0
            //0x0000000000000000 leading zeros = 64
            //0xFFFFFFFFFFFFFFFF leading zeros = 0
        }
        #endif
        //printf("connIndex = %llu \n", connIndex);            
        buf[connIndex] = 0x00;
        buf[(255) & (connIndex+1)] = 0xFF;
    }

    unsigned long end_cycle = readTSC(); 
    //double execution_time_seconds = ((double) (end_cycle-start_cycle)) / TSC_FREQUENCY;
    printf("clz elapsed = %f , connIndex = %llu \n", (double) (end_cycle-start_cycle)/NUM_ITERATIONS, connIndex);

    

}