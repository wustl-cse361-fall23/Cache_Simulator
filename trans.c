/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, ii, jj, blockRow, blockCol, temp, diagonal;
    int blockSize;

    // Choose block size based on matrix size.
    if (N == 32) {
        blockSize = 8; 
    } else if (N == 64) {
        blockSize = 4; 
    } else {
        blockSize = 17; // Using a prime number for irregular matrix --- similar idea to hashing for reduced collisions in a hash table
    }

    // 64x64 Matrix handled here. Not sure if performance is still not good because of loop unrolling and cpu not having enough registers
    // The regular algorithm does not seem to be cache friendly at all for this case.
    if (N == 64 && M == 64) {
        for (i = 0; i < N; i += blockSize) {
            for (j = 0; j < M; j += blockSize) {
                for (ii = i; ii < i + blockSize; ++ii) {
                    for (jj = j; jj < j + blockSize; ++jj) {
                        if (ii != jj) {
                            //transpose elements normally
                            B[jj][ii] = A[ii][jj];
                        } else {
                            // Diagonal: save values
                            temp = A[ii][jj];
                            diagonal = ii;
                        }
                    }
                    if (i == j) {
                        //  write saved diagonal value
                        B[diagonal][diagonal] = temp;
                    }
                }
            }
        }
    } else {
        // General case for other matrix sizes
        for (i = 0; i < N; i += blockSize) {
            for (j = 0; j < M; j += blockSize) {
                for (ii = i, blockRow = (i + blockSize > N) ? N : i + blockSize; ii < blockRow; ++ii) {
                    for (jj = j, blockCol = (j + blockSize > M) ? M : j + blockSize; jj < blockCol; ++jj) {
                        if (ii != jj || i != j) {
                            //  Transpose elements normally
                            B[jj][ii] = A[ii][jj];
                        } else {
                            // Diagonal: Save the value and index for later
                            temp = A[ii][jj];
                            diagonal = ii;
                        }
                    }
                    if (i == j) {
                        // After copying all non-diagonal elements, write the saved diagonal value
                        B[diagonal][diagonal] = temp;
                    }
                }
            }
        }
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

