/**
* Not - Working code for
* 1. Global load from A, B and C (asm)
* 2. Write to LDS (asm)
* 3. Read from LDS (asm)
* 4. 8 iteration of outer product (asm)
* 5. offset based load and stores (asm)
* 6. Store to global (asm)
* + Double Buffering!
*/

/**
* Notes:
* 1. You can only load 4 halfs from A and 8 halfs from B.
* The final outer product will have 32 halfs which can
* be stored in 16 registers.
* 2. Which means you have do ds_read_b64 or prefetch
* using ds_read2_b64 with offsets. Which means you get two
* prefetching layers.
*/

#include <iostream>
#include <hip/hip_runtime.h>
#include <hip/hip_runtime_api.h>
#include "outer_product.h"
#include "global_ops.h"
#include "shared_ops.h"
#include "dims.h"
#include <fstream>

constexpr uint32_t xDim = 8192;
constexpr uint32_t yDim = 8192;
constexpr uint32_t xDim8 = xDim/8;
constexpr uint32_t xDim16 = xDim/16;
constexpr uint32_t x32 = 32;
constexpr uint32_t x16 = 16;
constexpr size_t Size = xDim * yDim * sizeof(__fp16);

// A is N, B is T

__global__ void SGEMM(Half8 *A, Half8 *B, Half8 *C, int *getGlobalAId, int *getGlobalCId) {
    int tx = hipThreadIdx_x;
    int ty = hipThreadIdx_y;
    int bx = hipBlockIdx_x;
    int by = hipBlockIdx_y;

    Half8 rA[4], rB[4];
    Half8 c[16];

    Half8 ra, rb;

    uint32_t redA = 0;
    uint32_t redB = 4096;

    uint32_t ldsReadA = 0;
    uint32_t ldsReadB = 4096;
    uint32_t ldsWriteA = 0;
    uint32_t ldsWriteB = 4096;

    int id = tx + (ty % 2) * x16 + (ty / 2) * x32;
    int a_global_id = tx + (ty % 2) * x16 + (ty / 2) * 1024 + by * x32;
    int b_global_id = tx + (ty % 2) * x16 + (ty / 2) * 1024 + bx * x32;

    int cid0 = tx + ty * 4 * xDim4 + bx * 32 + by * 1024 * 128;
    int cid1 = 0;

    global_load<0>(A, ra, a_global_id);
    global_load<0>(B, rb, b_global_id);
    a_global_id += 8 * 1024;
    b_global_id += 8 * 1024;

    ldsReadA = redA+ty*16;
    ldsReadB = redB+tx*16;

    uint32_t ldsWrite = redA+id*16;
    ldsWriteB = redB+id*16;
    ldsWriteA = redA+id*16;

    Float4 *tmpC = C + cid0;
    global_load<0>(tmpC, c[0]);
    global_load<16>(tmpC, c[4]);
    tmpC += 1024;
    global_load<0>(tmpC, c[1]);
    global_load<16>(tmpC, c[5]);
    tmpC += 1024;
    global_load<0>(tmpC, c[2]);
    global_load<16>(tmpC, c[6]);
    tmpC += 1024;
    global_load<0>(tmpC, c[3]);
    global_load<16>(tmpC, c[7]);

    tmpC = C + 64*1024;
    global_load<0>(tmpC, c[8], cid1);
    global_load<16>(tmpC, c[12], cid1);
    tmpC += 1024;
    global_load<0>(tmpC, c[9], cid1);
    global_load<16>(tmpC, c[13], cid1);
    tmpC += 1024;
    global_load<0>(tmpC, c[10], cid1);
    global_load<16>(tmpC, c[14], cid1);
    tmpC += 1024;
    global_load<0>(tmpC, c[11], cid1);
    global_load<16>(tmpC, c[15], cid1);

/*
    int tmp = 0;
    asm volatile("\n\
    global_load_dwordx4 %0, %16, off \n \
    global_load_dwordx4 %4, %16, off offset:16*4*4 \n \
    \n \
    v_add_u32 %17, %16, 1024 \n \
    \n \
    global_load_dwordx4 %1, %17, off \n \
    global_load_dwordx4 %5, %17, off offset:16*4*4 \n \
    \n \
    v_add_u32 %17, %17, 1024 \n \
    \n \
    global_load_dwordx4 %2, %17, off \n \
    global_load_dwordx4 %6, %17, off offset:16*4*4 \n \
    \n \
    v_add_u32 %17, %17, 1024 \n \
    \n \
    global_load_dwordx4 %3, %17, off \n \
    global_load_dwordx4 %7, %17, off offset:16*4*4 \n \
    \n \
    v_add_u32 %17, %16, 65536 \n \
    \n \
    global_load_dwordx4 %8, %17, off \n \
    global_load_dwordx4 %12, %17, off offset:16*4*4 \n \
    \n \
    v_add_u32 %17, %17, 1024 \n \
    \n \
    global_load_dwordx4 %9, %17, off \n \
    global_load_dwordx4 %13, %17, off offset:16*4*4 \n \
    \n \
    v_add_u32 %17, %17, 1024 \n \
    \n \
    global_load_dwordx4 %10, %17, off \n \
    global_load_dwordx4 %14, %17, off offset:16*4*4 \n \
    \n \
    v_add_u32 %17, %17, 1024 \n \
    \n \
    global_load_dwordx4 %11, %17, off \n \
    global_load_dwordx4 %15, %17, off offset:16*4*4 \n \
    ":
    "=v"(c[0]),"=v"(c[1]),"=v"(c[2]),"=v"(c[3]),
    "=v"(c[4]),"=v"(c[5]),"=v"(c[6]),"=v"(c[7]),
    "=v"(c[8]),"=v"(c[9]),"=v"(c[10]),"=v"(c[11]),
    "=v"(c[12]),"=v"(c[13]),"=v"(c[14]),"=v"(c[15])
    :"v"(C),"v"(cid0),"v"(tmp));
*/
    vmcnt<0>();
    shared_write_b128_off<0>(ra, ldsWriteA);
    shared_write_b128_off<4096>(rb, ldsWriteA);
    lgkmcnt<0>();

    asm volatile("\n \
    ds_read_b128 %0, %4 offset:0      \n \
    ds_read_b128 %1, %4 offset:256   \n \
    ds_read_b128 %2, %5 offset:0      \n \
    ds_read_b128 %3, %5 offset:256   \n \
    "
    :"=v"(rA[0]),"=v"(rA[1]), "=v"(rB[0]), "=v"(rB[1])
    :"v"(ldsReadA),"v"(ldsReadB)
    );

for(int j=1;j<yDim/8;j++) {

    global_load<0>(A, ra, a_global_id);
    global_load<0>(B, rb, b_global_id);
    a_global_id += 8 * 1024;
    b_global_id += 8 * 1024;

    asm volatile("\n \
    ds_read_b128 %0, %4 offset:1*512      \n \
    ds_read_b128 %1, %4 offset:1*512+256  \n \
    ds_read_b128 %2, %5 offset:1*512      \n \
    ds_read_b128 %3, %5 offset:1*512+256  \n \
    "
    :"=v"(rA[2]),"=v"(rA[3]), "=v"(rB[2]), "=v"(rB[3])
    :"v"(ldsReadA), "v"(ldsReadB)
    );

    lgkmcnt<4>();

    // i = 0
    outerProduct4x4(rA[0], rB[0], c[0], c[1], c[2], c[3]);
    outerProduct4x4(rA[0], rB[1], c[4], c[5], c[6], c[7]);
    outerProduct4x4(rA[1], rB[0], c[8], c[9], c[10], c[11]);
    outerProduct4x4(rA[1], rB[1], c[12], c[13], c[14], c[15]);

    asm volatile("\n \
    ds_read_b128 %0, %4 offset:2*512 \n \
    ds_read_b128 %1, %4 offset:2*512+256 \n \
    ds_read_b128 %2, %5 offset:2*512 \n \
    ds_read_b128 %3, %5 offset:2*512+256 \n \
    "
    :"=v"(rA[0]),"=v"(rA[1]), "=v"(rB[0]), "=v"(rB[1])
    :"v"(ldsReadA), "v"(ldsReadB)
    );

    lgkmcnt<4>();

    // i = 1
    outerProduct4x4(rA[2], rB[2], c[0], c[1], c[2], c[3]);
    outerProduct4x4(rA[2], rB[3], c[4], c[5], c[6], c[7]);
    outerProduct4x4(rA[3], rB[2], c[8], c[9], c[10], c[11]);
    outerProduct4x4(rA[3], rB[3], c[12], c[13], c[14], c[15]);


    asm volatile("\n \
    ds_read_b128 %0, %4 offset:3*512      \n \
    ds_read_b128 %1, %4 offset:3*512+256  \n \
    ds_read_b128 %2, %5 offset:3*512      \n \
    ds_read_b128 %3, %5 offset:3*512+256  \n \
    "
    :"=v"(rA[2]),"=v"(rA[3]), "=v"(rB[2]), "=v"(rB[3])
    :"v"(ldsReadA), "v"(ldsReadB)
    );

    lgkmcnt<4>();

    // i = 2
    outerProduct4x4(rA[0], rB[0], c[0], c[1], c[2], c[3]);
    outerProduct4x4(rA[0], rB[1], c[4], c[5], c[6], c[7]);
    outerProduct4x4(rA[1], rB[0], c[8], c[9], c[10], c[11]);
    outerProduct4x4(rA[1], rB[1], c[12], c[13], c[14], c[15]);

    asm volatile("\n \
    ds_read_b128 %0, %4 offset:4*512 \n \
    ds_read_b128 %1, %4 offset:4*512+256 \n \
    ds_read_b128 %2, %5 offset:4*512 \n \
    ds_read_b128 %3, %5 offset:4*512+256 \n \
    "
    :"=v"(rA[0]),"=v"(rA[1]), "=v"(rB[0]), "=v"(rB[1])
    :"v"(ldsReadA), "v"(ldsReadB)
    );

    lgkmcnt<4>();

    // i = 3
    outerProduct4x4(rA[2], rB[2], c[0], c[1], c[2], c[3]);
    outerProduct4x4(rA[2], rB[3], c[4], c[5], c[6], c[7]);
    outerProduct4x4(rA[3], rB[2], c[8], c[9], c[10], c[11]);
    outerProduct4x4(rA[3], rB[3], c[12], c[13], c[14], c[15]);

    asm volatile("\n \
    ds_read_b128 %0, %4 offset:5*512      \n \
    ds_read_b128 %1, %4 offset:5*512+256  \n \
    ds_read_b128 %2, %5 offset:5*512      \n \
    ds_read_b128 %3, %5 offset:5*512+256  \n \
    "
    :"=v"(rA[2]),"=v"(rA[3]), "=v"(rB[2]), "=v"(rB[3])
    :"v"(ldsReadA), "v"(ldsReadB)
    );

    lgkmcnt<4>();

    // i = 4
    outerProduct4x4(rA[0], rB[0], c[0], c[1], c[2], c[3]);
    outerProduct4x4(rA[0], rB[1], c[4], c[5], c[6], c[7]);
    outerProduct4x4(rA[1], rB[0], c[8], c[9], c[10], c[11]);
    outerProduct4x4(rA[1], rB[1], c[12], c[13], c[14], c[15]);

    asm volatile("\n \
    ds_read_b128 %0, %4 offset:6*512 \n \
    ds_read_b128 %1, %4 offset:6*512+256 \n \
    ds_read_b128 %2, %5 offset:6*512 \n \
    ds_read_b128 %3, %5 offset:6*512+256 \n \
    "
    :"=v"(rA[0]),"=v"(rA[1]), "=v"(rB[0]), "=v"(rB[1])
    :"v"(ldsReadA), "v"(ldsReadB)
    );

    lgkmcnt<4>();

    // i = 5
    outerProduct4x4(rA[2], rB[2], c[0], c[1], c[2], c[3]);
    outerProduct4x4(rA[2], rB[3], c[4], c[5], c[6], c[7]);
    outerProduct4x4(rA[3], rB[2], c[8], c[9], c[10], c[11]);
    outerProduct4x4(rA[3], rB[3], c[12], c[13], c[14], c[15]);

    asm volatile("\n \
    ds_read_b128 %0, %4 offset:7*512 \n \
    ds_read_b128 %1, %4 offset:7*512+256 \n \
    ds_read_b128 %2, %5 offset:7*512 \n \
    ds_read_b128 %3, %5 offset:7*512+256 \n \
    "
    :"=v"(rA[2]),"=v"(rA[3]), "=v"(rB[2]), "=v"(rB[3])
    :"v"(ldsReadA), "v"(ldsReadB)
    );

    redA = redA ^ 0x2000;
    redB = redB ^ 0x2000;

    ldsWriteA = redA+id*16;
    ldsWriteB = redB+id*16;
    ldsReadA = redA+ty*16;
    ldsReadB = redB+tx*16;

    vmcnt<0>();

    shared_write_b128_off<0>(ra, ldsWriteA);
    shared_write_b128_off<4096>(rb, ldsWriteA);

    lgkmcnt<2>();

    // i = 6
    outerProduct4x4(rA[0], rB[0], c[0], c[1], c[2], c[3]);
    outerProduct4x4(rA[0], rB[1], c[4], c[5], c[6], c[7]);
    outerProduct4x4(rA[1], rB[0], c[8], c[9], c[10], c[11]);
    outerProduct4x4(rA[1], rB[1], c[12], c[13], c[14], c[15]);

    lgkmcnt<0>();
    __syncthreads();

    asm volatile("\n \
    ds_read_b128 %0, %4 offset:0      \n \
    ds_read_b128 %1, %4 offset:256   \n \
    ds_read_b128 %2, %5 offset:0      \n \
    ds_read_b128 %3, %5 offset:256   \n \
    "
    :"=v"(rA[0]),"=v"(rA[1]), "=v"(rB[0]), "=v"(rB[1])
    :"v"(ldsReadA), "v"(ldsReadB)
    );

    // i = 7
    outerProduct4x4(rA[2], rB[2], c[0], c[1], c[2], c[3]);
    outerProduct4x4(rA[2], rB[3], c[4], c[5], c[6], c[7]);
    outerProduct4x4(rA[3], rB[2], c[8], c[9], c[10], c[11]);
    outerProduct4x4(rA[3], rB[3], c[12], c[13], c[14], c[15]);

}


asm volatile("ds_read_b128 %0, %4 offset:1*512      \n \
ds_read_b128 %1, %4 offset:1*512+256  \n \
ds_read_b128 %2, %5 offset:1*512      \n \
ds_read_b128 %3, %5 offset:1*512+256"
:"=v"(rA[2]),"=v"(rA[3]), "=v"(rB[2]), "=v"(rB[3])
:"v"(ldsReadA), "v"(ldsReadB)
);

lgkmcnt<4>();

// i = 0
outerProduct4x4(rA[0], rB[0], c[0], c[1], c[2], c[3]);
outerProduct4x4(rA[0], rB[1], c[4], c[5], c[6], c[7]);
outerProduct4x4(rA[1], rB[0], c[8], c[9], c[10], c[11]);
outerProduct4x4(rA[1], rB[1], c[12], c[13], c[14], c[15]);

asm volatile("\n \
ds_read_b128 %0, %4 offset:2*512 \n \
ds_read_b128 %1, %4 offset:2*512+256 \n \
ds_read_b128 %2, %5 offset:2*512 \n \
ds_read_b128 %3, %5 offset:2*512+256 \n \
"
:"=v"(rA[0]),"=v"(rA[1]), "=v"(rB[0]), "=v"(rB[1])
:"v"(ldsReadA), "v"(ldsReadB)
);

lgkmcnt<4>();

// i = 1
outerProduct4x4(rA[2], rB[2], c[0], c[1], c[2], c[3]);
outerProduct4x4(rA[2], rB[3], c[4], c[5], c[6], c[7]);
outerProduct4x4(rA[3], rB[2], c[8], c[9], c[10], c[11]);
outerProduct4x4(rA[3], rB[3], c[12], c[13], c[14], c[15]);


asm volatile("\n \
ds_read_b128 %0, %4 offset:3*512      \n \
ds_read_b128 %1, %4 offset:3*512+256  \n \
ds_read_b128 %2, %5 offset:3*512      \n \
ds_read_b128 %3, %5 offset:3*512+256  \n \
"
:"=v"(rA[2]),"=v"(rA[3]), "=v"(rB[2]), "=v"(rB[3])
:"v"(ldsReadA), "v"(ldsReadB)
);

lgkmcnt<4>();

// i = 2
outerProduct4x4(rA[0], rB[0], c[0], c[1], c[2], c[3]);
outerProduct4x4(rA[0], rB[1], c[4], c[5], c[6], c[7]);
outerProduct4x4(rA[1], rB[0], c[8], c[9], c[10], c[11]);
outerProduct4x4(rA[1], rB[1], c[12], c[13], c[14], c[15]);

asm volatile("\n \
ds_read_b128 %0, %4 offset:4*512 \n \
ds_read_b128 %1, %4 offset:4*512+256 \n \
ds_read_b128 %2, %5 offset:4*512 \n \
ds_read_b128 %3, %5 offset:4*512+256 \n \
"
:"=v"(rA[0]),"=v"(rA[1]), "=v"(rB[0]), "=v"(rB[1])
:"v"(ldsReadA), "v"(ldsReadB)
);

lgkmcnt<4>();

// i = 3
outerProduct4x4(rA[2], rB[2], c[0], c[1], c[2], c[3]);
outerProduct4x4(rA[2], rB[3], c[4], c[5], c[6], c[7]);
outerProduct4x4(rA[3], rB[2], c[8], c[9], c[10], c[11]);
outerProduct4x4(rA[3], rB[3], c[12], c[13], c[14], c[15]);

asm volatile("\n \
ds_read_b128 %0, %4 offset:5*512      \n \
ds_read_b128 %1, %4 offset:5*512+256  \n \
ds_read_b128 %2, %5 offset:5*512      \n \
ds_read_b128 %3, %5 offset:5*512+256  \n \
"
:"=v"(rA[2]),"=v"(rA[3]), "=v"(rB[2]), "=v"(rB[3])
:"v"(ldsReadA), "v"(ldsReadB)
);

lgkmcnt<4>();

// i = 4
outerProduct4x4(rA[0], rB[0], c[0], c[1], c[2], c[3]);
outerProduct4x4(rA[0], rB[1], c[4], c[5], c[6], c[7]);
outerProduct4x4(rA[1], rB[0], c[8], c[9], c[10], c[11]);
outerProduct4x4(rA[1], rB[1], c[12], c[13], c[14], c[15]);

asm volatile("\n \
ds_read_b128 %0, %4 offset:6*512 \n \
ds_read_b128 %1, %4 offset:6*512+256 \n \
ds_read_b128 %2, %5 offset:6*512 \n \
ds_read_b128 %3, %5 offset:6*512+256 \n \
"
:"=v"(rA[0]),"=v"(rA[1]), "=v"(rB[0]), "=v"(rB[1])
:"v"(ldsReadA), "v"(ldsReadB)
);

lgkmcnt<4>();

// i = 5
outerProduct4x4(rA[2], rB[2], c[0], c[1], c[2], c[3]);
outerProduct4x4(rA[2], rB[3], c[4], c[5], c[6], c[7]);
outerProduct4x4(rA[3], rB[2], c[8], c[9], c[10], c[11]);
outerProduct4x4(rA[3], rB[3], c[12], c[13], c[14], c[15]);

asm volatile("\n \
ds_read_b128 %0, %4 offset:7*512 \n \
ds_read_b128 %1, %4 offset:7*512+256 \n \
ds_read_b128 %2, %5 offset:7*512 \n \
ds_read_b128 %3, %5 offset:7*512+256 \n \
"
:"=v"(rA[2]),"=v"(rA[3]), "=v"(rB[2]), "=v"(rB[3])
:"v"(ldsReadA), "v"(ldsReadB)
);
lgkmcnt<4>();
// i = 6
outerProduct4x4(rA[0], rB[0], c[0], c[1], c[2], c[3]);
outerProduct4x4(rA[0], rB[1], c[4], c[5], c[6], c[7]);
outerProduct4x4(rA[1], rB[0], c[8], c[9], c[10], c[11]);
outerProduct4x4(rA[1], rB[1], c[12], c[13], c[14], c[15]);

lgkmcnt<4>();

// i = 5
outerProduct4x4(rA[2], rB[2], c[0], c[1], c[2], c[3]);
outerProduct4x4(rA[2], rB[3], c[4], c[5], c[6], c[7]);
outerProduct4x4(rA[3], rB[2], c[8], c[9], c[10], c[11]);
outerProduct4x4(rA[3], rB[3], c[12], c[13], c[14], c[15]);

    tmpC = C + cid0;
    global_store<0>(tmpC, c[0]);
    global_store<16>(tmpC, c[4]);
    tmpC += 1024;
    global_store<0>(tmpC, c[1]);
    global_store<16>(tmpC, c[5]);
    tmpC += 1024;
    global_store<0>(tmpC, c[2]);
    global_store<16>(tmpC, c[6]);
    tmpC += 1024;
    global_store<0>(tmpC, c[3]);
    global_store<16>(tmpC, c[7]);

    tmpC = C + 64*1024;
    global_store<0>(tmpC, c[8]);
    global_store<16>(tmpC, c[12]);
    tmpC += 1024;
    global_store<0>(tmpC, c[9]);
    global_store<16>(tmpC, c[13]);
    tmpC += 1024;
    global_store<0>(tmpC, c[10]);
    global_store<16>(tmpC, c[14]);
    tmpC += 1024;
    global_store<0>(tmpC, c[11]);
    global_store<16>(tmpC, c[15]);

    vmcnt<0>();
}


int main() {
    hipSetDevice(1);
    std::vector<Half8> a(xDim8*yDim), b(xDim8*yDim), c(xDim*xDim8);
    __fp16 *_a = reinterpret_cast<__fp16*>(a.data());
    __fp16 *_b = reinterpret_cast<__fp16*>(b.data());
    __fp16 *_c = reinterpret_cast<__fp16*>(c.data());

    for(int j=0;j<yDim;j++) {
        for(int i=0;i<xDim;i++) {
            _a[i + j * xDim] = __fp16((j+i*xDim)*1.0f + 1.0f);
            if(i == j) {
            _b[i + j * xDim] = __fp16(1.0f);
            } else {
            _b[i + j * xDim] = __fp16(0.0f);
            }
            _c[i + j * xDim] = __fp16(0.0f);
        }
    }

    Half8 *Ad, *Bd, *Cd;
    int *buffA, *buffB;
    hipHostMalloc(&buffA, 16*16*sizeof(int));
    hipHostMalloc(&buffB, 16*16*sizeof(int));
    hipMalloc(&Ad, a.size()*sizeof(Half8));
    hipMalloc(&Bd, b.size()*sizeof(Half8));
    hipMalloc(&Cd, c.size()*sizeof(Half8));
    hipMemcpy(Ad, a.data(), a.size()*sizeof(Half8), hipMemcpyHostToDevice);
    hipMemcpy(Bd, b.data(), b.size()*sizeof(Float4), hipMemcpyHostToDevice);
    hipMemcpy(Cd, c.data(), c.size()*sizeof(Float4), hipMemcpyHostToDevice);
    auto start = std::chrono::high_resolution_clock::now();
    hipLaunchKernelGGL(SGEMM, dim3(32,32,1), dim3(16,16,1), 4*sizeof(float)*8*128*2, 0, Ad, Bd, Cd, buffA, buffB);
    hipDeviceSynchronize();
    auto stop = std::chrono::high_resolution_clock::now();
    double sec = std::chrono::duration_cast<std::chrono::duration<double>>(stop - start).count();
    std::cout<<sec<<std::endl;
    double flops = (double)xDim * (double)yDim * (double)yDim * 2 / (double)1.0e12;
    double floppersec = flops / sec;
    std::cout<<flops<<" "<<sec<<" "<<floppersec<<std::endl;
    hipMemcpy(c.data(), Cd, c.size()*sizeof(Float4), hipMemcpyDeviceToHost);
    std::ofstream outfile;
    outfile.open("outfile.txt");

    std::cout<<"writing to outfile"<<std::endl;

    for(int j=0;j<4;j++) {
        for(int i=0;i<xDim;i++) {
            outfile << _c[i+j*xDim] <<" ";
        }
        outfile <<"\n";
    }
    outfile<<"\n\n\n";
/*
    std::cout<<"Done writing to outfile"<<std::endl;
    for(int j=0;j<16;j++) {
        for(int i=0;i<16;i++) {
            outfile << buffA[i+j*16]<<" ";
        }
        outfile << "\n";
    }
    outfile<<"\n\n\n";
    for(int j=0;j<16;j++) {
        for(int i=0;i<16;i++) {
            outfile << buffB[i+j*16]<<" ";
        }
        outfile << "\n";
    }
    outfile<<"\n\n\n";
*/
    outfile.close();
}