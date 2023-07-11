#ifndef A2D_SPARSE_MATRIX_H
#define A2D_SPARSE_MATRIX_H

#include <string>

#include "a2dobjs.h"
#include "array.h"

namespace A2D {

/**
 * @brief Block CSR matrix, object acts like shared_ptr
 *
 * @tparam T data type
 * @tparam M number of rows for each block
 * @tparam N number of columns for each block
 */
template <typename T, index_t M, index_t N>
class BSRMat {
 public:
  /**
   * @brief Constructor
   *
   * @tparam VecType a vector type
   * @param nbrows number of rows of blocks
   * @param nbcols number of columns of blocks
   * @param nnz number of non-zero blocks, note that nnz(mat) = nnz * M * N
   * @param _rowp vector of row pointers
   * @param _cols vector of column indices
   */
  template <class VecType>
  BSRMat(index_t nbrows, index_t nbcols, index_t nnz, const VecType &rowp_,
         const VecType &cols_)
      : nbrows(nbrows), nbcols(nbcols), nnz(nnz), vals("vals", nnz) {
    rowp = IdxArray1D_t("rowp", nbrows + 1);
    cols = IdxArray1D_t("cols", nnz);

    for (index_t i = 0; i < nbrows + 1; i++) {
      rowp[i] = rowp_[i];
    }

    for (index_t i = 0; i < nnz; i++) {
      cols[i] = cols_[i];
    }
  }

  /**
   * @brief Copy constructor (shallow copy)
   */
  A2D_INLINE_FUNCTION BSRMat(const BSRMat &src)
      : nbrows(src.nbrows),
        nbcols(src.nbcols),
        nnz(src.nnz),
        rowp(src.rowp),
        cols(src.cols),
        diag(src.diag),
        perm(src.perm),
        iperm(src.iperm),
        num_colors(src.num_colors),
        color_count(src.color_count),
        vals(src.vals) {}

  A2D_INLINE_FUNCTION ~BSRMat() = default;

  // Zero the entries of the matrix
  A2D_INLINE_FUNCTION void zero() { A2D::BLAS::zero(vals); }

  /**
   * @brief Find the address of the column index given block indices (row, col)
   *
   * @param row block row index
   * @param col block column index
   * @return index_t the block column index, MAX_INDEX if (row, col) isn't in
   * the nonzero pattern
   */
  index_t find_column_index(index_t row, index_t col) {
    index_t jp_start = rowp[row];
    index_t jp_end = rowp[row + 1];

    for (index_t jp = jp_start; jp < jp_end; jp++) {
      if (cols[jp] == col) {
        return jp;
      }
    }

    return MAX_INDEX;
  }

  template <class Mat>
  void add_values(const index_t m, const index_t i[], const index_t n,
                  const index_t j[], Mat &mat) {
    for (index_t ii = 0; ii < m; ii++) {
      index_t block_row = i[ii] / M;
      index_t eq_row = i[ii] % M;

      for (index_t jj = 0; jj < n; jj++) {
        index_t block_col = j[jj] / N;
        index_t eq_col = j[jj] % N;

        index_t jp = find_column_index(block_row, block_col);
        if (jp != MAX_INDEX) {
          vals(jp, eq_row, eq_col) += mat(ii, jj);
        }
      }
    }
  }

  void zero_rows(const index_t nbcs, const index_t dof[]) {
    for (index_t ii = 0; ii < nbcs; ii++) {
      index_t block_row = dof[ii] / M;
      index_t eq_row = dof[ii] % M;

      for (index_t jp = rowp[block_row]; jp < rowp[block_row + 1]; jp++) {
        for (index_t k = 0; k < N; k++) {
          vals(jp, eq_row, k) = 0.0;
        }

        if (cols[jp] == block_row) {
          vals(jp, eq_row, eq_row) = 1.0;
        }
      }
    }
  }

  // Convert to a dense matrix
  void to_dense(index_t *m_, index_t *n_, T **A_) {
    index_t m = M * nbrows;
    index_t n = N * nbcols;
    index_t size = m * n;

    T *A = new T[size];
    std::fill(A, A + size, T(0.0));

    for (index_t i = 0; i < nbrows; i++) {
      for (index_t jp = rowp[i]; jp < rowp[i + 1]; jp++) {
        index_t j = cols[jp];

        for (index_t ii = 0; ii < M; ii++) {
          const index_t irow = M * i + ii;
          for (index_t jj = 0; jj < N; jj++) {
            const index_t jcol = N * j + jj;
            A[n * irow + jcol] = vals(jp, ii, jj);
          }
        }
      }
    }

    *A_ = A;
    *m_ = m;
    *n_ = n;
  }

  // Write the sparse matrix in the mtx format
  void write_mtx(const std::string mtx_name = "matrix.mtx") {
    // Open file and destroy old contents, if any
    std::FILE *fp = std::fopen(mtx_name.c_str(), "w");

    // Write header
    std::fprintf(fp, "%%%%MatrixMarket matrix coordinate real general\n");

    // Write M, N and nnz
    std::fprintf(fp, "%d %d %d\n", nbrows * M, nbcols * N, nnz * M * N);

    // Write entries
    for (index_t i = 0; i < nbrows; i++) {
      for (index_t jp = rowp[i]; jp < rowp[i + 1]; jp++) {
        index_t j = cols[jp];  // (i, j) is the block index pair

        for (index_t ii = 0; ii < M; ii++) {
          const index_t irow = M * i + ii + 1;  // convert to 1-based index
          for (index_t jj = 0; jj < N; jj++) {
            // (irow, jcol) is the entry coo
            const index_t jcol = N * j + jj + 1;  // convert to 1-based index
            std::fprintf(fp, "%d %d %30.20e\n", irow, jcol, vals(jp, ii, jj));
          }
        }
      }
    }
    std::fclose(fp);
    return;
  }

  // Array type
  using IdxArray1D_t = A2D::MultiArrayNew<index_t *>;

  // Number of block rows and block columns
  index_t nbrows, nbcols;

  // Number of non-zero blocks
  index_t nnz;  // = rowp[nbrows];

  // rowp and cols array
  IdxArray1D_t rowp;  // length: nbrows + 1
  IdxArray1D_t cols;  // length: nnz = rowp[nbrows]

  // Pointer to the diagonal block, this is not allocated until
  // factorization
  IdxArray1D_t diag;  // length: nbrows

  // permutation perm[new var] = old var
  // This is not allocated by default
  IdxArray1D_t perm;

  // Inverse permutation iperm[old var] = new var
  // This is not allocated by default
  IdxArray1D_t iperm;

  // When coloring is used, its ordering is stored in the permutation array
  index_t num_colors;        // Number of colors
  IdxArray1D_t color_count;  // Number of nodes with this color, not
                             // allocated by default

  // A multi-dimensional array that stores entries, shape: (M, N, nnz)
  MultiArrayNew<T *[M][N]> vals;
};

}  // namespace A2D

#endif  // A2D_SPARSE_MATRIX_H
